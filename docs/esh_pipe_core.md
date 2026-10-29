# sys file・fd・pipeコア

更新日: 2026-10-15

## 役割

`src/core/sys`はanonymous pipeに必要なfile、inode、buffer cache、read/write、
schedulerをeshから独立して提供する。通常ファイル、path、disk I/O、signalは
扱わない。

UNIX V7の次の関係を最小構成で維持する。

```text
struct proc p_ofile[]
  -> global struct file table
       -> shared struct inode table
            -> memory-only struct buf table
```

`fio.c`はfdとfile lifecycle、`alloc.c`と`iget.c`はinode lifecycle、`pipe.c`は
pipe作成・待機条件・inode lock、`rdwri.c`はinode data転送、`sys2.c`はcurrent
process向けread/write/close入口を担当する。`bio.c`は固定長buffer cacheを持つ。

## 固定長pool

- process当たりfd: `NOFILE` = 12
- global file entry: `NFILE` = 12
- pipe inode: `NINODE` = 3
- buffer cache entry: `NBUF` = 3
- pipe data: `PIPSIZ` = `BSIZE` = 256 byte

`binit()`はbuffer cache、`finit()`はfile tableとinode tableを初期化する。
完全初期化は次の順で明示的に呼び出す。

```c
binit();
finit();
```

`finit()`は`binit()`を内部で呼ばない。所有中のfile、inode、buffer、実行中
processがある状態では完全初期化しない。

## 所有権

`falloc()`は空きfdとfile entryを同時に確保し、fdが所有する1参照を設定する。
`fd_install()`と`fd_install_at()`は既存file参照を1個追加する。`fd_close()`はfdを
外して`closef()`へ1参照を渡す。

file参照が0になると、pipe wait channelを起床してinode参照を解放する。channelは
inode先頭からのbyte offsetで識別し、lockは0、writeは1、readは2とする。
wait listはinode外のscheduler hashが所有する。最後の
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

`readi()`はread fileの`f_offset`から`i_size`までを読み取る。全dataを読み切ると
read offsetと`i_size`を0へ戻す。`writei()`は`i_size`から`PIPSIZ`までへ追記する。
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
user address space copy、trap dispatch、signal deliveryである。buffer cache APIは
残すが、pipe bufferはmemory-only anonymous blockとして使用する。
