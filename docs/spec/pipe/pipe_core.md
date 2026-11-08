# sys file・fd・pipeコア

更新日: 2026-11-08

## 役割

`src/core/sys`はanonymous pipeに必要なfile、inode、buffer cache、read/write、
schedulerをeshから独立して提供する。pipe APIの範囲はanonymous pipeであり、
通常ファイル、path探索、signalは扱わない。

UNIX V7の次の関係を最小構成で維持する。

```text
struct proc p_ofile[]
  -> global struct file table
       -> shared struct inode table
            -> buffer from shared struct buf table
```

`fio.c`はfdとfile lifecycle、`alloc.c`と`iget.c`はinode lifecycle、`pipe.c`は
pipe作成・待機条件・inode lock、`rdwri.c`はinode data転送、`sys2.c`はcurrent
process向けread/write/close入口を担当する。`bio.c`は固定長buffer cacheを持つ。

## 固定長pool

- process当たりfd: `NOFILE` = 12
- global file entry: `NFILE` = 12
- pipe inode: `NINODE` = 3
- buffer cache entry: `NBUF` の既定値は5。最小値は `NINODE` = 3
- pipe data: `PIPSIZ` = `BSIZE` = 256 byte

`init_bufc()`はbuffer cache、`init_fsys()`はfile tableとinode tableを初期化する。
pipeの共有資源は次の順で明示的に初期化する。

```c
init_bufc();
init_fsys();
```

共有資源の初期化は、所有中のfile、inode、bufferや実行中processがない状態で
行う。各processのfd tableと実行状態は`init_proc()`で初期化する。Windowsの
`eshinit()`は先に`init_ramdisk()`を呼び、共有資源の初期化後に`init_tty()`を呼ぶ。

## 所有権

`falloc()`は空きfdとfile entryを同時に確保し、fdが所有する1参照を設定する。
`fd_install()`と`fd_install_at()`は既存file参照を1個追加する。`fd_close()`はfdを
外して`closef()`へ1参照を渡す。

file参照が0になると、pipe wait channelを起床してinode参照を解放する。channelは
inode先頭からのbyte offsetで識別し、lockは0、writeは1、readは2とする。
wait listはinode外のscheduler hashが所有する。最後の
readまたはwrite endpointが閉じた状態は`i_count < 2`で判定する。両endpointが
閉じると`iput()`がbufferを返し、inodeを空き状態へ戻す。

`pipe_for_proc()`はinodeと対応buffer、read file、write fileの順に確保する。
`ialloc()`は`getblk(PIPEDEV, inodeの添字)`で共通cacheからbufferを取得し、
`iput()`まで保持する。途中で失敗した
場合は確保済みfd/file/inodeを逆順に戻す。process終了時は`close_all()`が全fdを
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

`readp()`は現在取得可能なdataを返す。`writep()`は要求全体を完了するまで部分
転送と待機を繰り返す。pipe I/Oと`plock()`は、`sched_prepare()`に成功し、
schedulerから実行中のcurrent processで使用する。空・満杯・inode lock競合は
`sched_sleep()`で待機し、再開後に条件を再検査する。これらの関数には
`p_ctx == NULL`の判定や`-SYS_EAGAIN`を返す経路を設けない。

backend未設定processでのpipe I/Oはサポートしない。`readp()`と`writep()`は
`struct file *`、buffer、byte数を受け取り、待機対象processはschedulerが解決する。
fd作成・接続・closeは実行準備前にも行えるため、接続用の一時的なprocessには
backendを必要としない。

最終writer close後もbuffer内のdataを読み出せる。その後のreadは0を返す。
reader不在時のwriteは`-SYS_EPIPE`を返し、signalは発生させない。

## 排他制御

公開API経由のpool、参照数、inode、buffer操作は非再帰sys lockで保護する。
inodeの`ILOCK`と`IWANT`はV7の`plock()`、`prele()`に対応し、lock競合時は専用
wait channelで待つ。再開後はlockとpipe条件を必ず再検査する。

## V7からの採用と省略

採用した要素はper-process fd table、global file table、共有inode、endpoint別
file offset、`FREAD/FWRITE/FPIPE`、`falloc/getf/closef`、`readp/writep`、
`readi/writei`、inode lock、sleep/wakeupである。

pipe経路で省略した要素はdisk inode、directory、mount、block mapping、
user address space copy、trap dispatch、signal deliveryである。buffer cacheは
RAM diskなどのblock deviceと共用する。pipe dataは取得したbufferへ直接読み書きし、
pipeの転送ではdevice strategyへI/Oを発行しない。
