# unix v6 u_error

## draft task ver.1 (to be refined)
`/c/usr/src/MCUFW/src/core/sys/h/sched.h` の `struct proc` 構造体に `int p_error` を追加しました。unix v6 で `u_error` と同じ扱いでソースコードにエラー記録処理を追加してください。

## request1
unix v6 で u_error の下記について詳細に教えてください。
- 役割
- 使用方法
- 最初期化のタイミング

## reply1
UNIX V6 の `u.u_error` は、**実行中のプロセスのカーネル処理が、失敗理由を呼び出し元へ伝えるための欄**です。起動時には `main()` に入る前に user block の一括クリアで初めて `0` になり、ユーザープロセスがシステムコールを発行するたびに `trap()` が明示的に `0` に戻します。

### 役割

`sys/user.h` の `struct user` に `char u_error` として定義されています。`u` は実行中のプロセスの user area を指すため、カーネルのコードではどのプロセスでも同じ `u.u_error` という式を使いますが、値を保持する領域はプロセスごとにあります。`0` はエラーなしを表し、`EBADF` や `EIO` などの値は失敗理由を表します。下位関数はこれを使って、通常の戻り値とは別に失敗理由を伝えられます。 [V6 `user.h`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/user.h:20)

### 使用方法

たとえば `read(fd, buf, count)` では、カーネル内の `read()` が `rdwr(FREAD)` を呼び、`rdwr()` が `getf()` でファイル記述子を引きます。無効な記述子なら `getf()` は `u.u_error = EBADF` を設定して `NULL` を返します。`rdwr()` は `NULL` を見て処理を打ち切ります。有効な記述子でも読み取り用に開かれていなければ、今度は `rdwr()` 自身が `EBADF` を設定します。 [V6 `sys2.c`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/sys2.c:13) · [V6 `fio.c`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/fio.c:20)

`trap()` はシステムコールの入口で `u.u_error` をゼロにし、保存されたユーザー側 PS のキャリービットを落としてから処理を呼び出します。処理が戻ったとき、通常のエラーコードならキャリービットを立て、保存された `R0` にエラー番号を入れます。正常な `read` では `R0` に読み取ったバイト数が残ります。ユーザー側の C ライブラリの `read` は `bec` でキャリービットを調べ、エラー時には `cerror` に進みます。`cerror` が `R0` のエラー番号をユーザー空間の `errno` に書き、C の呼び出し元へ `-1` を返します。成功時はライブラリが `errno` をクリアしないため、`read()` の成否は戻り値で判断します。 [V6 `trap.c`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/trap.c:100) · [V6 `read.s`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/source/s5/read.s) · [V6 `cerror.s`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/v6root/usr/source/s4/cerror.s)

カーネル内部で、戻り値と `u.u_error` の組み合わせで次の処理を決める例もあります。`creat()` が `namei(&uchar, 1)` を呼んで `NULL` を受け取った場合、`u.u_error` が `0` なら新規作成へ進み、値が入っていれば探索中の失敗として処理を打ち切ります。ブロック I/O では `geterror()` がバッファの `B_ERROR` を調べ、失敗時に `b_error` を `u.u_error` へ写し、個別のエラー番号がなければ `EIO` を入れます。 [V6 `sys2.c` の `creat()`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/sys2.c:79) · [V6 `bio.c` の `geterror()`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/dmr/bio.c:621)

`u_error` は最初のエラーを固定する欄ではなく、コードが代入するたびに更新されます。たとえば `trap1()` から戻った時点で `u.u_intflg` が残っていれば、`trap()` は `EINTR` を代入します。また、復帰処理は `100` 以上の値を `SIGSYS` に回します。未実装のシステムコールを処理する `nosys()` が設定する `100` がその例です。 [V6 `trap.c`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/trap.c:120)

### 最初に初期化されるタイミング

1. **カーネル起動時。** PDP-11/40 用の `m40.s` と PDP-11/45・11/70 用の `m45.s` は、初期 user block を `_u` からワード単位の `clr` でゼロ埋めし、その後に `_main` を呼びます。`u_error` の最初のゼロ化はこの一括クリアに含まれます。`main()` はこの領域を使い、`u.u_procp` を `&proc[0]` に設定します。 [V6 `m40.s`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/conf/m40.s:753) · [V6 `m45.s`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/conf/m45.s:157) · [V6 `main.c`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/main.c:100)
2. **プロセス生成時。** `newproc()` は親の user area を含むプロセスイメージを子に引き継ぎます。生成処理に `u_error` 専用のゼロ代入はなく、子の値は引き継いだ領域の内容から始まります。起動時に作られるプロセス 1 も `main()` から `newproc()` を経由します。 [V6 `slp.c` の `newproc()`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/slp.c:351) · [V6 `main.c`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/main.c:121)
3. **システムコール開始時。** `trap()` の `case 6+USER` に入ると、システムコール番号を取り出す前に `u.u_error = 0` を実行します。この時点から、下位関数が設定する値は今回の呼び出しの結果として扱えます。カーネル内部から始める処理にも個別のクリアがあり、たとえば `core()` はコアファイルの名前探索に入る前に `u.u_error` をゼロにします。 [V6 `trap.c`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/trap.c:100) · [V6 `sig.c` の `core()`](/C:/usr/src/MCUFW/.agents/skills/ancient_unix/references/unix_v6/sys/ken/sig.c:188)


## request2
MCUFW では trap を使わないので、UNIX V6 の u_error の扱いを踏襲する場合、どの関数で初期化し、コルーチンでパイプを実行するときはどのコンテキストに保持すべきか、現行ソースを確認して教えてください。

## reply2
MCUFW で UNIX V6 の `u_error` の扱いを踏襲するなら、**保持先は既存の `struct proc::p_error`、呼び出しごとのクリアは公開 `sys_*()` 関数の入口**です。現行コードでは `p_error` は宣言済みで、`init_proc()` の `memset()` によって初期値は `0` になります。I/O のエラー伝達には現在、負の `-SYS_E*` 戻り値が使われています。 [sched.h](C:/usr/src/MCUFW/src/core/sys/h/sched.h:39) · [sched.c](C:/usr/src/MCUFW/src/core/sys/sys/sched.c:136) · [sys2.c](C:/usr/src/MCUFW/src/core/sys/sys/sys2.c:8)

### どの関数で初期化するか

| タイミング | 関数 | 扱い |
|---|---|---|
| 実行コンテキストの生成時 | `init_proc()` | 現行の `memset()` で `p_error` も `0` になる。単独コマンドでは `execute_commands()` が、パイプでは `exe_run_pipeline()` が段ごとに呼ぶ。 |
| I/O 呼び出しの開始時 | `sys_read()`、`sys_write()`、`sys_close()`、`sys_pipe()` | `current_proc()` で呼び出し元を取得し、`p_error = 0` にする。ここが V6 の `trap()` のシステムコール入口に相当する。 |
| I/O 呼び出しの終了時 | 同じ `sys_*()` 関数 | 戻り値が負なら `p_error = -result` として正の `SYS_E*` を記録する。成功時は入口で設定した `0` のまま返す。 |

既存の負の戻り値を維持し、公開 API の境界でだけ `p_error` に反映する案です。`sys_read()` と `sys_write()` は、現状では `count == 0` のとき `current_proc()` を取得する前に返ります。この成功経路も呼び出しごとのクリアに含めるなら、ゼロ長の早期復帰より前にクリアを置きます。EOF による読み取り結果 `0` も `p_error == 0` です。 [exe.c の単独実行](C:/usr/src/MCUFW/src/core/shell/cli/exe.c:55) · [exe.c のパイプ実行](C:/usr/src/MCUFW/src/core/shell/cli/exe.c:295) · [sys2.c](C:/usr/src/MCUFW/src/core/sys/sys/sys2.c:8) · [pipe.c の `sys_pipe()`](C:/usr/src/MCUFW/src/core/sys/sys/pipe.c:62)

### コルーチンでパイプを実行するときの保持先

**各段の `exe_proc.p_exec.p_error`** に保持します。たとえば `A | B` では、`A` の `sys_write()` が記録する `SYS_EPIPE` は `A` の `p_exec` に入り、`B` の `sys_read()` は `B` の `p_exec` にだけ触れます。`exe_run_pipeline()` は各段の `p_exec` を `sched_prepare()` に渡し、`sched_resume()` は再開対象を `current_proc()` から取得できるように設定します。スレッド版でもワーカースレッドが自身の `proc` を設定するので、エラーの保持先は共通にできます。 [exe.h](C:/usr/src/MCUFW/src/core/shell/cli/include/exe.h:24) · [exe.c](C:/usr/src/MCUFW/src/core/shell/cli/exe.c:319) · [sched.c の `sched_resume()`](C:/usr/src/MCUFW/src/core/sys/sys/sched.c:175) · [sched_thread.c](C:/usr/src/MCUFW/src/core/sys/dev/windows/sched_thread.c:188)

クリアは **API を新たに呼び出したときに一度**です。`readp()` や `writep()` が `sched_sleep()` で待機し、別の段が実行されてから戻ってきても、元の `sys_read()` や `sys_write()` は同じ呼び出しの途中です。そのため、`sched_resume()` や待機後の再試行にはクリアを入れません。共有パイプの inode にエラーを持たせると、読み手と書き手の結果が混ざります。 [pipe.c の待機ループ](C:/usr/src/MCUFW/src/core/sys/sys/pipe.c:73) · [sched.c の `sched_sleep()`](C:/usr/src/MCUFW/src/core/sys/sys/sched.c:56)

パイプの**接続準備**は別の呼び出し経路です。`connect_proc()` は一時的な `owner` を使って `pipe_for_proc()` を直接呼び、その結果を `exe_run_pipeline()` に返します。段のコマンドが走る前の失敗なので、現行どおり準備処理の負の戻り値で伝えるのが自然です。段の終了ステータスも既存の `p_ret` が担当します。 [exe.c の `connect_proc()`](C:/usr/src/MCUFW/src/core/shell/cli/exe.c:512) · [exe.c の `proc_entry()`](C:/usr/src/MCUFW/src/core/shell/cli/exe.c:480)

今回は現行ソースの確認と設計案の提示までで、コード変更は行っていません。


## request3
上記の方針で実装してください。
- p_error の追加場所は、 unix v6 のソースコードを参照して決めてください。
- p_error にセットするエラーは、 unix v6 のソースコードを参照して決めてください。
- 各パイプ段の p_exec.p_error に保持し、公開 sys_() の入口でクリア、失敗時に正の SYS_E を記録してください。
- 既存の負の戻り値は維持し、コルーチンの待機・再開を含めてテストしてください。
