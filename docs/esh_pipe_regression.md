# sys pipe移行回帰試験

更新日: 2026-10-13

## Windows対話入力

Windows Makefileは`USE_STACKMACH`、`ESH_SCRIPT`、`ESH_STDIO_PIPE`を有効にする。
入力をlexer、parser、executorへ渡し、sys pipeを使うpipeline実行部を利用する。
既存builtin番号は変更せず、末尾にstream転送専用`cat`を追加している。

MSYS2 MINGW64 shellでbuildと実行を行う。PowerShellからgccを直接起動しない。

```sh
cd src/target/windows
make -B SCHED_BACKEND=cortn
./main.exe
```

thread backendは`SCHED_BACKEND=thread`で選ぶ。確認入力は次のとおり。

```sh
sam1
sam2 1 10 100
sam3 -a -b string
echo hello|cat
echo four|cat|cat|cat
k=3
echo "$k"|cat
```

`cat`は引数なしのstream転送専用で、ファイル名は扱わない。先頭`cat`はconsole
入力を待ち、末尾`cat`はconsoleへ出力する。`if`と`while`は複数行を蓄積し、
最外側blockが閉じてから実行する。変数は完了した入力間で保持する。

## APIと所有権

`esh_script_init()`は単一console sessionを初期化する。`esh_script_line()`はNUL
終端された`TTY_NBUF` byteの呼出元bufferを変更する。行末tokenが必要で、上限
超過または欠落時は実行しない。負値は拒否、1は未完blockまたは完了status 1、
その他は最後のexecutor statusである。公開非同期APIではない。

lexer、parser、console sessionはsingletonで、再入・並列利用は禁止する。
parserが`PARINVLD`を返したcodeは実行せず、`par_init()`と`code_init()`で再初期化
する。consoleはerror時も変数tableを保持する。

sys層では`struct proc.p_ofile[]`が`struct file`を参照し、read/write endpointが
共有`struct inode`とmemory-only `struct buf`を参照する。kernel lockはpipe条件、
inode lock、参照数、wait listを保護する。process終了時はfdをcloseしてから
lock外でbackendをreleaseまたはjoinする。

詳細は`esh_pipe_api.md`、`esh_pipe_core.md`、`esh_pipe_sched.md`、
`esh_pipe_thread.md`、`esh_pipe_stdio.md`を参照する。

## Pipe意味の回帰点

- `sys_pipe()`はcurrent processへread/write fdを作る
- `sys_read()`は現在取得可能なdataを返し、空ならwriter有無を再判定する
- `sys_write()`は要求全体を完了するまで部分転送と待機を繰り返す
- 最終writer close後は残存dataを読み切ってからEOFを返す
- reader不在では`-SYS_EPIPE`を返し、signalを送らない
- closeはread/write waiterを起こす
- data位置はV7型offset方式で、未読dataをringとして回り込ませない
- 全data読取後に`f_offset`と`i_size`を0へ戻してbufferを再利用する
- coroutine/threadで同じ転送結果、EOF、EPIPE、終了statusを返す

## 回帰試験

```sh
cd src/core/sys/test/bio
bash ./run_test.sh
cd ../file
bash ./run_test.sh
cd ../pipe
bash ./run_test.sh
cd ../sched
bash ./run_test.sh

cd ../../../esh/test/cortn
bash ./run_test.sh
cd ../pipe
bash ./run_test.sh
cd ../sched
bash ./run_test.sh
cd ../pipeline_exec
bash ./run_test.sh
cd ../par
bash ./run_test1.sh
bash ./run_test2.sh
cd ../exe
bash ./run_test.sh
cd ../cquote
make -B
./test_cquote.exe
```

bio、file/inode、sys pipe、coroutineはO0/O2を試験する。file/inode、sys pipe、
coroutine、esh pipe、schedulerは`-Wall -Wextra -Werror -pedantic`を使用する。
esh pipe、scheduler、pipelineはcoroutine/threadをO0/O2で試験する。

pipelineは0、1、256、257、2089 byteと2/3/4段を確認する。各組合せを反復し、
NULを含むbinary data、順序、変換結果、file/inode使用数0を確認する。2089 byteは
V7型256-byte blockのfull/read/resetを複数回繰り返す。

producer先行終了、consumer早期終了、中間filter error、zero-length I/O、pipe
枯渇、backend部分準備失敗、rollback後の再実行も確認する。fd/file枯渇はpipe
単体、process/stack pool枯渇とhandle回収はscheduler、stack guardはcoroutine
単体で確認する。

console自動試験は4段、while/if、変数と引用変数、数値command substitution、
`break 2`、構文error・nest超過後の復帰を含む。境界試験はcode容量、命令途中
切れ、不正ID/opcode、stack上限、symbol・変数・label枯渇、NULL、未終端入力を
含む。

## 既知の制約

- 同時pipelineは1個、最大4段。nested pipelineとbackground実行は未対応
- command substitutionは数値戻り値代入で、stdout captureではない
- consoleの行・token・symbol・code容量は固定し、動的拡張しない
- CPU-only commandの強制cancel、signal、通常ファイルは未対応
- mapped `getchar()`はread errorとEOFをともにEOFとして返す
- pipe stdinへ`scanf`を使用できない
- STM32/Cortex-M/RTOS移植は未着手

## 既存warning

strictなsys、pipe、scheduler、coroutine suiteにはwarningがない。pipeline、
executor、Windows targetの広いbuildには移行前から次が残る。

- `help.c`: 未使用`argc`、`argv`
- `lex.c`: 未使用static関数、`snprintf`/`strncpy`切詰め警告
- `par.c`: `char`型array subscript
- `test.c`: 未使用static関数

これらはgeneric sys移行による新規warningではない。試験中に出るlexer/parser
error表示はerror pathを確認する意図された診断で、suiteは成功する。

## Sanitizer

2026-10-03のMSYS2 MinGW64環境では`-lasan`と`-lubsan`がなく、sanitizer実行は
できなかった。coroutineの手動stack切替にはsanitizer fiber対応も未実装である。
利用可能なhostでsanitizerを使う場合はthread backendを対象にする。
