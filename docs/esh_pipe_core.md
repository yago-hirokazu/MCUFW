# esh file・fd・pipeコア

更新日: 2026-10-02

## 役割

Milestone 3ではschedulerによる待機を接続せず、anonymous pipeの所有権と
状態遷移を実装する。通常ファイル、path、disk I/O、signalは扱わない。

UNIX V7の次の関係を最小構成で維持する。

```text
process fd table
  -> global file entry
       -> shared pipe inode
            -> memory-only ring buffer
```

V7のpipe inodeが一時ファイルを介してbuffer cacheを利用する部分は、256 byteの
固定長ring bufferへ置き換える。物理storageへはアクセスしない。

## 固定長pool

- process当たりfd: 12
- global file entry: 12
- pipe inode: 3
- pipe buffer: 256 byte/pipe

`esh_file_system_init()`はfile tableとpipe inode poolを初期化する。所有中の
fileがある状態では呼び出さない。

## 所有権

`esh_file_alloc()`は構築中参照を1個持つfileを返す。`esh_fd_install()`は
file参照を1個追加する。pipe生成に成功した場合は構築中参照を解放し、fd参照
だけを残す。

file参照が0になると対応endpointを閉じる。最終read endpointまたはwrite
endpointのcloseをinodeへ反映し、両方が0になるとinodeとbufferをpoolへ戻す。

`esh_pipe()`の途中で失敗した場合は、登録済みfd、write file、read file、inodeの
順で巻き戻す。process終了時は`esh_close_all()`が全fdを閉じる。

## 非blocking I/O

内部APIは次を区別する。

- `ESH_IO_PROGRESS`: 1 byte以上転送、またはzero-length操作
- `ESH_IO_WAIT_READ`: bufferが空でwriterが存在する
- `ESH_IO_WAIT_WRITE`: bufferが満杯でreaderが存在する
- `ESH_IO_EOF`: bufferが空でwriterが存在しない
- `ESH_IO_BROKEN_PIPE`: readerが存在しない
- `ESH_IO_ERROR`: 不正fd、方向違反、不正引数

readとwriteは現在転送可能な分だけを転送する。ring bufferの末尾を越える場合も
データ順序を維持する。

Milestone 4以降の公開`esh_read()`と`esh_write()`は、scheduler backendが
bindされたprocessを待機させ、再開後に条件を再検査する。backendをbindして
いない単体利用では、待機が必要な場合に`-ESH_EAGAIN`または転送済みbyte数を
返す。

最終writer close後もbuffer内のデータをすべて読み出せる。その後のreadは0を
返す。reader不在時のwriteは`-ESH_EPIPE`を返し、signalは発生させない。

## 排他制御

Milestone 3の内部非blocking単体試験は単一実行単位で動かす。公開API経由の
pool、参照数、ring buffer操作はMilestone 4のkernel lockで保護する。
