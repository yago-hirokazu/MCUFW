# sys file・fd・pipeコア

更新日: 2026-11-19

## 役割

`src/core/sys`はanonymous pipeに必要なfile、inode、buffer cache、read/write、
schedulerをshから独立して提供する。pipe APIの範囲はanonymous pipeであり、
通常ファイル、path探索、signalは扱わない。

UNIX V7の次の関係を最小構成で維持する。

```text
struct proc p_ofile[]
  -> global struct file table
       -> shared struct inode table
            -> device/block identity
                 -> shared struct buf table
                      -> RAM disk block
```

`fio.c`はfdとfile lifecycle、`alloc.c`と`iget.c`はinode lifecycle、`pipe.c`は
pipe作成・待機条件・inode lock、`rdwri.c`はinode data転送、`sys2.c`はcurrent
process向けread/write/close入口を担当する。`bio.c`は固定長buffer cacheの取得・
解放とblock I/Oを担当する。pipe dataもこのcacheを通して読み書きする。

## 固定長pool

- process当たりfd: `NOFILE` = 12
- global file entry: `NFILE` = 12
- pipe inode: `NINODE` = 3
- buffer cache entry: `NBUF` の既定値は5。inode数とは独立して設定できる
- pipe data: `PIPSIZ` = `BSIZE` = 256 byte

`PIPEDEV`は`0x0101U`で、RAM diskのmajor 1、unit 1に対応する。block番号は
inode tableの添字とし、先頭`NINODE` block（現在0から2）をpipe用に予約する。
これらのblockは他用途のblock I/Oには使用しない。filesystemや空きblock管理は
導入せず、inodeとdata blockを固定で対応させる。

`init_bufc()`はbuffer cache、`init_fsys()`はfile tableとinode tableを初期化する。
pipeの共有資源は次の順で明示的に初期化する。

```c
init_bufc();
init_fsys();
```

共有資源の初期化は、所有中のfile、inode、bufferや実行中processがない状態で
行う。各processのfd tableと実行状態は`init_proc()`で初期化する。Windowsの
`shinit()`は先に`init_ramdisk()`を呼び、共有資源の初期化後に`init_tty()`を呼ぶ。

## 所有権

`falloc()`は空きfdとfile entryを同時に確保し、fdが所有する1参照を設定する。
`fdalloc()`と`fdassign()`は既存file参照を1個追加する。`fdfree()`はfdを
外して`closef()`へ1参照を渡す。

file参照が0になると、pipe wait channelを起床してinode参照を解放する。channelは
inode先頭からのbyte offsetで識別し、lockは0、writeは1、readは2とする。
wait listはinode外のscheduler hashが所有する。最後の
readまたはwrite endpointが閉じた状態は`i_count < 2`で判定する。両endpointが
閉じると`iput()`がinodeを空き状態へ戻す。cache内のdirty dataはclose後も残り、
後からRAM diskへ書き戻される場合がある。次のinode割当てでは同じcache keyの
blockを初期化し、以前のdataを置き換える。

`pipe_for_proc()`はinodeと対応blockの初期化、read file、write fileの順に処理する。
inodeは`i_dev`と`i_blkno`を保持し、bufferへのpointerは保持しない。`ialloc()`は
`getblk()`で対応blockを取得し、`clrbuf()`で初期化して`bdwrite()`で返す。
bufferの`B_BUSY`は各操作中だけ保持し、inodeの生存中もcache entryを占有し続けない。
inodeまたはbufferを確保できなければ作成は`-SYS_ENFILE`で失敗する。途中で失敗した
場合は確保済みfd/file/inodeを逆順に戻す。process終了時は`fdclean()`が全fdを
閉じる。

## Pipe I/O

内部結果は次を区別する。

- `PIPE_IO_PROGRESS`: 1 byte以上転送
- `PIPE_IO_WAIT_READ`: dataが空でwriterが存在する
- `PIPE_IO_WAIT_WRITE`: `i_size == PIPSIZ`でreaderが存在する
- `PIPE_IO_EOF`: dataが空でwriterが存在しない
- `PIPE_IO_BROKEN`: readerが存在しない
- `PIPE_IO_ERROR`: 内部data転送のエラー

`sys_read()`と`sys_write()`は未割当fdまたは方向違反を`-SYS_EBADF`で返す。
0 byte要求はfdを参照する前に0を返す。

`readi()`はread fileの`f_offset`から`i_size`までを読み取る。全dataを読み切ると
read offsetと`i_size`を0へ戻す。`writei()`は`i_size`から`PIPSIZ`までへ追記する。
未読dataが残る間はbuffer先頭へ回り込まない。

`readi()`は`bread()`でbufferを取得し、転送後に`brelse()`で返す。`writei()`は
block全体を上書きする場合に`getblk()`、一部を更新する場合に`bread()`で取得し、
転送後に`bdwrite()`で返す。cache hitならdevice strategyを呼ばずに転送が完了する。

dirty bufferは`B_DELWRI`付きでcacheに残る。他blockへの再利用や`bflush()`では
RAM diskへ書き戻し、次のcache missで読み戻す。pipe dataの生存期間とbufferの
占有期間を分離するため、pipeが残っていてもcache entryを別blockへ再利用できる。

I/O時にbufferを取得できなければ`-SYS_EAGAIN`を返す。device I/Oが失敗した場合は
bufferの`b_error`に対応する負のerrorを返す。その回の`readi()`または`writei()`で
転送前に失敗した場合、read offsetと`i_size`は変更しない。要求全体を複数回に分ける
`writep()`では、それ以前の回に転送したdataまで取り消すものではない。

`readp()`は現在取得可能なdataを返す。`writep()`は要求全体を完了するまで部分
転送と待機を繰り返す。pipe I/Oと`plock()`は、`sched_prepare()`に成功し、
schedulerから実行中のcurrent processで使用する。空・満杯・inode lock競合は
`sched_sleep()`で待機し、再開後に条件を再検査する。これらの関数には
`p_ctx == NULL`の判定や`-SYS_EAGAIN`を返す経路を設けない。

backend未設定processでのpipe I/Oはサポートしない。`readp()`と`writep()`は
`struct file *`、buffer、byte数を受け取り、待機対象processはschedulerが解決する。
fd作成・接続・closeは実行準備前にも行えるため、接続用の一時的なprocessには
backendを必要としない。

最終writer close後も残存dataを読み出せる。その後のreadは0を返す。
reader不在時のwriteは`-SYS_EPIPE`を返し、signalは発生させない。

## 排他制御

公開API経由のpool、参照数、inode、buffer操作は非再帰sys lockで保護する。
inodeの`ILOCK`と`IWANT`はV7の`plock()`、`prele()`に対応し、lock競合時は専用
wait channelで待つ。再開後はlockとpipe条件を必ず再検査する。

## V7からの採用と省略

採用した要素はper-process fd table、global file table、共有inode、endpoint別
file offset、`FREAD/FWRITE/FPIPE`、`falloc/getf/closef`、`readp/writep`、
`readi/writei`、inode lock、sleep/wakeupである。

pipe経路で省略した要素はdisk inode、directory、mount、filesystemのblock割当て、
user address space copy、trap dispatch、signal deliveryである。inodeとdata blockの
対応は固定し、buffer cacheとRAM diskへのblock I/Oを利用する。通常のcache hitでは
device strategyを呼ばず、cache missやdirty bufferの書き戻し時に呼ぶ。
