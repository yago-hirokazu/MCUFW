# esh pipe API・データモデル

更新日: 2026-10-02

## 対象範囲

本書は `esh_pipe_coroutine_summary.md` の Milestone 1 で固定した仕様を示す。
実装対象はanonymous pipeだけとし、通常ファイル、パス探索、物理ストレージ、
リダイレクト、background job、signalは含めない。

UNIX V7の `file`、pipe inode、processごとのfd tableという関係を保つ。ただし、
V7のディスク上の一時ファイルは使用せず、pipe inodeが固定長ring bufferを直接
所有する。

## 資源上限

| 資源 | 上限 | 定義 |
|---|---:|---|
| 同時process・pipeline段数 | 4 | `ESH_PIPELINE_MAX` |
| process当たりfd | 12 | `ESH_OPEN_MAX` |
| global file entry | 12 | `ESH_FILE_MAX` |
| pipe inode | 3 | `ESH_PIPE_MAX` |
| pipe buffer | 256 byte/pipe | `ESH_PIPE_BUFFER_SIZE` |

4段pipelineに必要なpipeは最大3本である。全poolはcore側の固定長配列とし、
動的メモリ確保を必須にしない。`PROCS` と `ESH_PIPELINE_MAX` の不一致は
コンパイルエラーにする。

## 所有権

```text
struct esh_proc
  -> struct esh_fd_table
       -> struct esh_file
            -> struct esh_pipe_inode
                 -> struct esh_pipe_buffer
                 -> read/write wait channel
```

- fd entryはfile entryを1回参照する
- file entryはreadまたはwriteの一方向だけを表す
- file参照数が0になった時点で対応endpointを閉じる
- endpoint close時にinodeのreaderまたはwriter数を減らす
- readerとwriterがともに0になった時点でinodeとbufferをpoolへ戻す
- process終了時はfd番号順に全fdを閉じる
- `esh_pipe()` 失敗時はwrite file、read file、inode、fdの逆順で戻す
- closeは反対側のwaiterを起こし、EOFまたはbroken pipeを再判定させる

## API結果

- `esh_pipe()`、`esh_close()`は成功時0、失敗時は負の`ESH_E*`を返す
- `esh_read()`、`esh_write()`は転送byte数、EOFの0、または負の`ESH_E*`を返す
- `count == 0` はfdやpeer状態を検査せず0を返す
- global `errno`は使用しない
- `esh_read()`は取得可能なデータを返し、空かつwriterありの場合だけ待機する
- `esh_write()`は空きへ書ける分を転送する。残量があれば空きを待って継続し、
  正常時は要求全体の転送byte数を返す
- 最終writer close後はbufferを読み切ってからEOFを返す
- reader不在時のwriteは`-ESH_EPIPE`を返す。signalは発生させない

内部pipe処理は`ESH_IO_*`で進行、read待ち、write待ち、EOF、broken pipe、
その他のエラーを区別する。公開APIが待機と再試行を隠蔽する。

## Processとwait channel

`struct esh_proc`はfd table、実行状態、待機先、終了statusを所有する。既存の
executor用`struct proc`は`struct esh_proc`を埋め込み、コマンド引数などの
shell固有情報とportable scheduler状態を分離する。

wait channelの待機リストはscheduler backendが所有する。pipe inodeはread用と
write用のchannelを別々に所有する。pipe状態検査、wait登録、kernel lock解放を
不可分にし、再開後は必ず条件を再検査する。

pipeline全体の終了statusは最後のcommandのstatusとする。途中のcommandの
エラーは資源回収には影響させるが、この規則を変更しない。

## 標準I/O

- fd 0、1、2をstdin、stdout、stderrとする
- pipelineはstdoutと次段stdinだけを接続する
- stderrとdebug logはconsoleへ出し、pipeデータへ混入させない
- pipe対応commandの入力基本APIは`esh_read()`とする
- 既存`scanf`はconsole入力だけを互換範囲とし、初期pipe対応には含めない

## Coroutine引き継ぎ条件

Windows版coroutine stack初期容量は16 KiBとする。両端にguard patternを置き、
yieldと終了時に破壊を検査し、使用量watermarkを取得できるようにする。

Microsoft x64 ABIの非volatile整数レジスタ、`XMM6`～`XMM15`、x87 control word、
`MXCSR`を保存・復元する。Windows上でFPU/SIMD利用を制限する方式は採用しない。
具体的なcontext構造と切替APIはMilestone 2で確定する。
