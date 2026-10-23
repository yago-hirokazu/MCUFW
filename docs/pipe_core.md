# sys file・fd・pipeコア

更新日: 2026-10-24

## 役割

`src/core/sys`はanonymous pipeに必要なfile、inode、buffer cache、read/write、
schedulerをeshから独立して提供する。通常ファイル、path、disk I/O、signalは
扱わない。

UNIX V7の次の関係を最小構成で維持する。

```text
struct proc p_addr -> struct user u_ofile[]
  -> global struct file table
       -> shared struct inode table
            -> memory-only struct buf table
```

`fio.c`はfdとfile lifecycle、`alloc.c`と`iget.c`はinode lifecycle、`pipe.c`は
pipe作成・待機条件・inode lock、`rdwri.c`はinode data転送、`sys2.c`はcurrent
process向けread/write/close入口を担当する。`bio.c`は固定長buffer cacheを持ち、
pipeとRAM block I/Oで共有する。

## 固定長pool

- process当たりfd: `NOFILE` = 12
- global file entry: `NFILE` = 12
- pipe inode: `NINODE` = 3
- buffer cache entry: `NBUF` = 5（既定値）
- pipe data: `PIPSIZ` = `BSIZE` = 256 byte

`binit()`はbuffer cache、`finit()`はfile tableとinode tableを初期化する。
完全初期化は次の順で明示的に呼び出す。

```c
binit();
finit();
```

`finit()`は`binit()`を内部で呼ばない。所有中のfile、inode、buffer、実行中
processがある状態では完全初期化しない。

Windows shell起動時は`eshinit()`が`raminit(); binit(); finit();`の順で呼ぶ。
RAM内容も消去する場合は、停止状態で`raminit()`を先に呼ぶ。
3本のpipeが3個のbufferを保持すると、既定構成ではRAM I/O用に2個が残る。
専用予約はなく、busy hit・空きなしでは`getblk()`が待機せずNULLを返す。

## 所有権

`falloc()`はcurrent userの空きfdとfile entryを確保し、fd所有の1参照を設定する。
fdは`u_r.r_val1`、失敗は`u_error`へ設定する。`ufalloc()`もfdと結果領域を設定し、
失敗時は-1を返す。`getf(fd)`は失敗時NULLと`SYS_EBADF`を設定する。
`fd_install()`と`fd_install_at()`は既存file参照を1個追加する。`fd_close()`はfdを
外して`closef()`へ1参照を渡す。

file参照が0になると、pipe wait channelを起床してinode参照を解放する。最後の
readまたはwrite endpointが閉じた状態は`i_count < 2`で判定する。両endpointが
閉じると`iput()`がbufferを返し、inodeを空き状態へ戻す。

`pipe_create()`と`pipe_for_proc()`は共通処理でinode、read file、write fileを確保する。
前者はcurrent user、後者は実行前の指定userへerrorと両端fdの結果を設定する。
別userの準備ではcurrentを切り替えず、呼出し元のerror/resultを保持する。
file割当は`falloc_for_user()`、inode割当は`ialloc_error()`を共有する。
`ialloc()`は同じ確保処理を使い、失敗時current userへ`SYS_ENFILE`を設定する。
途中で失敗した場合は確保済みfd/file/inodeを逆順に戻す。process終了時は`close_all()`が全fdを
閉じる。

## Pipe I/O

`readp(fp)`と`writep(fp)`はcurrent userの転送状態を使うvoid APIで、
内部status構造体や負の戻り値を使わない。失敗は正の`u_error`、EOFは残量を減らさず
errorなしで返す。`plock(ip)`もvoidとなり、失敗時に取得していないlockを解放しない。

`readi(ip)`と`writei(ip)`はcurrent userの`u_base/u_count/u_offset`を使う。
`readi()`は`u_offset`から`i_size`まで、`writei()`は`PIPSIZ`まで転送する。
`iomove()`はB_READでbufferからuser、B_WRITEでuserからbufferへコピーし、
成功時だけbase/offsetを転送量だけ進め、countを同量減らす。内部成功はerrorを消さない。
負のoffset、範囲外のinode size/offset、不正pointer・残量・方向・offset加算overflowは
EINVALとなり、転送状態を変更しない。ゼロ長ではpointer演算しない。

Step 4の一時helperは削除済みであり、pipeからinode I/Oへ同じuserを引き継ぐ。
writepは全要求の残量をローカルに保持し、各区間を`PIPSIZ - i_size`以下に制限する。
区間後は未着手分を含む残量をu_countへ戻してからlockを解放する。
途中エラー・待機脱出でもbaseは実際の転送分だけ進み、全要求の残量を保持する。
全data読取後のread offsetと`i_size`の0への復帰は`readp()`が行い、
`writep()`は追記位置として`i_size`を使う。
未読dataが残る間はbuffer先頭へ回り込まない。

`readp()`は現在取得可能なdataを転送し、`writep()`は要求全体を完了するまで部分
転送と待機を繰り返す。backendなしで空/fullならEAGAINとするが、writeが転送後に
fullへ到達した場合だけ短い成功を維持する。inode lock競合やsleep失敗は、
転送済み量があってもエラーとなる。

最終writer close後もbuffer内のdataを読み出せる。その後のreadは0を返す。
reader不在時のwriteはu_errorをEPIPEとし、公開APIは`-SYS_EPIPE`を返す。signalは発生させない。

## 排他制御

公開API経由のpool、参照数、inode、buffer操作は非再帰kernel lockで保護する。
inodeの`ILOCK`と`IWANT`はV7の`plock()`、`prele()`に対応し、lock競合時は専用
wait channelで待つ。再開後はlockとpipe条件を必ず再検査する。

## V7からの採用と省略

採用した要素はper-process fd table、global file table、共有inode、endpoint別
file offset、`FREAD/FWRITE/FPIPE`、`falloc/getf/closef`、`readp/writep`、
`readi/writei`、inode lock、sleep/wakeupである。

省略した要素はdisk inode、directory、mount、block mapping、physical device I/O、
user address space copy、trap dispatch、signal deliveryである。pipe bufferは
`getblk(PIPEDEV, inode番号)`で取得し、寿命中はbusyで保持する。
RAM driverのblock I/O、遅延書込み、flushは実装済みである。

## User areaとbuffer error

`proc_init(proc, user)`はprocと呼出元所有userを初期化し、`p_addr`と`u_procp`を設定する。
`current_user()`はcurrent procの`p_addr`を返す。current procがなければbackendの
base userを使う。Windows thread backendではbase userもTLSで分離する。

`struct user`は`u_error`、`ptrdiff_t`型2個の結果領域`u_r`、
所有procへの`u_procp`、fd tableの`u_ofile[]`、転送用`u_base/u_count/u_offset`を保持する。
inode/pipe I/Oとsys2はuserへ移行済みで、公開read/writeは開始時に状態を準備する。
終了時の`u_r.r_val1`は要求量-u_count。途中エラー時も診断用の転送量を保持するが、
外部戻り値は負のerrorを優先する。close/pipeの入口でもerror/resultを準備する。
buffer I/Oの呼出元は操作前に`u.u_error = 0`とし、操作後にerrorを確認する。
成功時の`geterror()`は以前のerrorを消さない。pipeの公開APIは従来どおり転送量
または負の`SYS_E*`を返す。RAM APIの詳細は[RAM block I/O](ram_block_device.md)参照。
