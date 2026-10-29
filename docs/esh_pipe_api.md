# sys pipe API・データモデル

更新日: 2026-10-13

## 対象範囲

本書は`src/core/sys`のanonymous pipe APIと、eshからの利用方法を示す。
通常ファイル、path探索、物理storage、リダイレクト、background job、signalは
含めない。

UNIX V7の`file`、pipe inode、processごとのfd tableという関係を保つ。pipe
dataはmemory-only buffer cache上の1 blockへ格納し、物理deviceへは送らない。

## 資源上限

| 資源 | 上限 | 定義 |
|---|---:|---|
| 同時process・pipeline段数 | 4 | `PIPELINE_MAX` |
| process当たりfd | 12 | `NOFILE` |
| global file entry | 12 | `NFILE` |
| pipe inode | 3 | `NINODE` |
| buffer cache entry | 3 | `NBUF` |
| pipe data | 256 byte/pipe | `PIPSIZ` = `BSIZE` |

4段pipelineに必要なpipeは最大3本である。全tableとpoolは固定長配列とし、
portable sys層では動的memory確保を使用しない。`NBUF < NINODE`はcompile
errorにする。

## 所有権

```text
struct proc
  -> p_ofile[NOFILE]
       -> struct file
            -> struct inode
                 -> struct buf
                 -> lock/read/write wait channel
```

- `p_ofile[]` entryはfile entryを1回参照する
- file entryは`FREAD|FPIPE`または`FWRITE|FPIPE`のendpointを表す
- `f_count`が0になると対応endpointを閉じる
- read/write fileは同じinodeを参照し、`i_count`で生存endpointを数える
- `i_count`が0になるとinodeとbufferをpoolへ戻す
- process終了時は`close_all()`がfd番号順に全fdを閉じる
- `pipe_for_proc()`失敗時は確保済みfile、fd、inodeを巻き戻す
- endpoint closeはread/write channelを起こし、EOFまたはEPIPEを再判定させる

## 公開API

`file.h`はcurrent processへ作用する次のAPIを公開する。

- `sys_pipe()`：read fdとwrite fdを作成する
- `sys_read()`：fdから読み取る
- `sys_write()`：fdへ書き込む
- `sys_close()`：fdを閉じる

sys層内部とesh executorは、明示的な`struct proc *`を受け取る
`pipe_for_proc()`、`getf()`、`fd_install_at()`、`fd_close()`、`close_all()`も
使用する。

- 作成とcloseは成功時0、失敗時は負の`SYS_E*`を返す
- read/writeは転送byte数、EOFの0、または負の`SYS_E*`を返す
- `count == 0`はfd、buffer、peer状態を検査せず0を返す
- global `errno`は使用しない
- readは取得可能なdataを返し、空かつwriterありの場合だけ待機する
- writeは空きへ書ける分を転送し、残量があれば待機して要求全体を完了する
- 最終writer close後は残存dataを読み切ってからEOFを返す
- reader不在時のwriteは`-SYS_EPIPE`を返し、signalは発生させない

内部pipe処理は`PIPE_IO_*`で進行、read待ち、write待ち、EOF、broken pipe、
errorを区別する。`sys_read()`と`sys_write()`が待機と再試行を隠す。

## Pipe data位置

V7と同様、read endpointの`f_offset`とinodeの`i_size`を単調に進める。
write位置は現在の`i_size`である。未読dataをring末尾から先頭へ回り込ませない。

readerが`i_size`まで読み切った時点でread offsetと`i_size`を0へ戻し、1 block
全体を再利用する。このため、未読dataが残る間は先頭側に空きがあっても再利用
しない。これは旧esh ring bufferから意図的に変更したV7型の意味である。

## Processとwait channel

`struct proc`はfd table、実行状態、待機先、backend、終了statusを所有する。
esh executor用processはこれを`p_exec`として埋め込み、command引数等のshell
固有情報とportable scheduler状態を分離する。

wait listはportable schedulerが管理し、backendはcontext切替とkernel lockを
実装する。channelはアドレスで識別し、pipeは`(char *)ip`でlock、
`(char *)ip + 1`でwrite、`(char *)ip + 2`でreadを区別する。inode内に
wait list格納領域は持たず、schedulerの`slpque[]`で管理する。状態検査、
wait登録、backend待機をkernel lockの下で行い、再開後は条件を再検査する。

pipeline全体の終了statusは最後のcommandのstatusとする。途中commandのerrorは
資源回収には影響させるが、この規則を変更しない。

## 標準I/O

- fd 0、1、2をstdin、stdout、stderrとする
- pipelineはstdoutと次段stdinだけを接続する
- stderrとdebug logはconsoleへ出し、pipe dataへ混入させない
- pipe対応commandは`sys_read()`またはmapped `getchar()`を使用する
- `scanf`はconsole入力だけを互換範囲とし、pipe stdinには使用しない

## V7との差分

- trapとglobal user area `u`を使わず、current processまたは明示引数を使う
- disk block、device strategy、MMU user copyを使わない
- inodeはdirectoryやmodeを持たず、pipeに必要なstateだけを持つ
- buffer cache entryはmemory-onlyで、pipe当たり1 blockに固定する
- V7 signal相当の`SIGPIPE`を送らず、`-SYS_EPIPE`だけを返す
- coroutine/thread backendを選択でき、両者で同じpipe結果を返す
- dynamic allocationを使わず、上限をcompile時定数で固定する

Windows coroutine stackは64 KiBとする。guard patternとwatermarkを持ち、
Microsoft x64 ABIの非volatile stateを保存する。MCU stack容量はSTM32移植時に
別途決定する。
