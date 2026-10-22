# sys file・fd・pipeコア

更新日: 2026-10-23

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

`falloc()`は空きfdとfile entryを同時に確保し、fdが所有する1参照を設定する。
`fd_install()`と`fd_install_at()`は既存file参照を1個追加する。`fd_close()`はfdを
外して`closef()`へ1参照を渡す。

file参照が0になると、pipe wait channelを起床してinode参照を解放する。最後の
readまたはwrite endpointが閉じた状態は`i_count < 2`で判定する。両endpointが
閉じると`iput()`がbufferを返し、inodeを空き状態へ戻す。

`pipe_for_proc()`はinode、read file、write fileの順に確保する。途中で失敗した
場合は確保済みfd/file/inodeを逆順に戻す。process終了時は`close_all()`が全fdを
閉じる。

## Pipe I/O

内部結果は次を区別する。

- `PIPE_IO_PROGRESS`: 1 byte以上転送、またはzero-length操作
- `PIPE_IO_WAIT_READ`: dataが空でwriterが存在する
- `PIPE_IO_WAIT_WRITE`: `i_size == PIPSIZ`でreaderが存在する
- `PIPE_IO_EOF`: dataが空でwriterが存在しない
- `PIPE_IO_BROKEN`: readerが存在しない
- `PIPE_IO_ERROR`: 不正fd、方向違反、不正引数

`readi()`はread fileの`f_offset`から`i_size`までを読み取る。全data読取後の
read offsetと`i_size`の0への復帰は`readp()`が行う。
`writei()`は渡されたoffsetから`PIPSIZ`までへ書き、`writep()`が追記位置に
`i_size`を使う。
未読dataが残る間はbuffer先頭へ回り込まない。

`readp()`は現在取得可能なdataを返す。`writep()`は要求全体を完了するまで部分
転送と待機を繰り返す。backend未設定processで待機が必要な場合は
`-SYS_EAGAIN`、またはすでに転送済みのbyte数を返す。

最終writer close後もbuffer内のdataを読み出せる。その後のreadは0を返す。
reader不在時のwriteは`-SYS_EPIPE`を返し、signalは発生させない。

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

`struct user`は`u_error`、所有procへの`u_procp`、fd tableの`u_ofile[]`を保持する。
inode転送のbuffer/count/offsetは明示引数のままで、userへの全面移行は未完了である。
buffer I/Oの呼出元は操作前に`u.u_error = 0`とし、操作後にerrorを確認する。
成功時の`geterror()`は以前のerrorを消さない。pipeの公開APIは従来どおり転送量
または負の`SYS_E*`を返す。RAM APIの詳細は[RAM block I/O](ram_block_device.md)参照。
