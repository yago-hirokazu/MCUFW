# task_list

last update: 2026-11-23

Legend:
[o]: open
[c]: close
/commit: 作業が完了した後、コミットする
/push: コミットできたら git push する

## task

1. [c] ESH_STDIO_PIPE の役割を調べて教えてください

ESH_STDIO_PIPE は、Windows 版の標準入出力を MCUFW 内部のパイプに接続するビルド設定です。
現在のプロセスに fd 0 があれば getchar() は sys_read() を使い、fd 1 があれば putchar()、`printf()`、`fprintf(stdout, ...)` は sys_write() を使います。
対応する fd がない場合と stderr は Windows 側の入出力に渡ります。パイプ入力に対する scanf() はデータを消費せず EOF を返します。


2. [c] 下記の初期化関数の名前を変更してください。下記の初期化関数を実行するテストも該当する関数の名前を変更してからテストを実行して、問題ないことを確認してください。/commit, /push
   - pinit => init_proc
   - finit => init_fsys
   - binit => init_bufc
   - ttyinit => init_tty
   - lex_init => init_lex
   - par_init => init_par
   - error_init => init_error
   - cortn_init => init_cortn
   - raminit => init_ramdisk
   - code_init => init_code

3. [c] MCUFW/AGENTS.md に下記を追記してください。/commit, /push
   - MCUFW/src/core/lib以外の初期化関数の名前は下記とする
     init_モジュール名

4. [c] TODO_BREAK_WITH_ARGUMENT が常時有効になるように、下記を実行してください。/commit, /push
   - TODO_BREAK_WITH_ARGUMENT == 1 のときに有効になるコードを残してください。そして TODO_BREAK_WITH_ARGUMENT と関連する #if - #endif をなくしてください
   - TODO_BREAK_WITH_ARGUMENT == 1 のときに無効になるコードをなくしてください。そして TODO_BREAK_WITH_ARGUMENT と関連する #if - #endif もなくしてください

5. [c] 現在のソースコードを参照して、下記のドキュメントを改訂してください。/commit, /push
   - /c/usr/src/MCUFW/docs/spec/pipe/*.md
   - /c/usr/src/MCUFW/docs/spec/coroutine/*.md

6. [c] /c/usr/src/MCUFW/src/core/sys/sys/fio.c の下記の関数の役割を教えてください
   - int fd_install(struct proc *proc, struct file *fp)
   - int fd_install_at(struct proc *proc, int fd, struct file *fp)
   - int fd_close(struct proc *proc, int fd)

7. [c] 下記の関数の違いを教えてください
   - struct file *falloc(struct proc *proc, int *fd)
   - int fd_install(struct proc *proc, struct file *fp)

8. [c] MCUFW/AGENTS.md に下記を追記してください。
   - 文字コードは UTF-8, no BOM, CRLF とする。


9. [c] MCUFWのファイルの文字コードを下記に統一してください。
   - UTF-8, no BOM, CRLF (Windows)


10. [c] `/c/usr/src/MCUFW/src/core/sys/h/sched.h` の `enum wait_reason` を `enum wait_cause` に変更してください。`enum wait_reason` のインスタンスに `reason` という名前を使っていたら、それも `cause` に変更してください。

11. [c] `/c/usr/src/MCUFW/src/core/sys/sys/fio.c` の下記の関数の名前を変更してください。
    - fd_install => fdalloc
    - fd_install_at => fdassign
    - fd_close => fdfree
    - close_all => fdclean

12. [c] `C:\usr\src\MCUFW\src\target` を `C:\usr\src\MCUFW\src\dev` に変更


## note

### タスク6〜7

| 関数 | 役割 |
|---|---|
| `fd_install(proc, fp)` | 最小の空き fd に既存の `struct file` を登録し、参照数を1増やします。成功時は割り当てた fd を返します。 |
| `fd_install_at(proc, fd, fp)` | 指定した空き fd に既存の `struct file` を登録し、参照数を1増やします。指定先が使用中なら `-SYS_EMFILE` を返します。パイプを次段の標準入力や前段の標準出力に接続する際に使われます。 |
| `fd_close(proc, fd)` | fd の登録を解除し、参照数を1減らします。最後の参照なら `closef()` がファイルエントリを解放し、パイプの場合は待機中の相手を起こします。未割り当ての fd なら `-SYS_EBADF` を返します。 |

`falloc()` はグローバルファイルテーブルから**新しいエントリ**を確保し、参照数を1にして空き fd に登録します。ファイルポインタを返し、fd は `*fd` に格納します。`fd_install()` は**既存のエントリを共有**するため、複数の fd が同じファイルオフセットを参照します。[fio.c](C:/usr/src/MCUFW/src/core/sys/sys/fio.c:34)


### タスク12

`src/target` を `src/dev` に変更した。未コミット変更、未追跡ファイル、同梱ツールチェーン、STM32 側の入れ子の Git リポジトリも移動先へ保持した。

- Makefile、setpath.bat、.gitignore、AGENTS.md、関連文書の参照先を更新した。
- cscope.files のパスを更新し、cscope.out を再生成した。
- 新しいパスで Windows 版をビルドし、`sam1`、`sam2 1 10 100`、`sam3 -a -b string` の正常動作を確認した。
- parser の run_test1.sh / run_test2.sh と executor の run_test.sh が成功した。
- cquote / pipeline は -O0 / -O2、pipeline_exec は cortn / thread × -O0 / -O2 の全構成で成功した。
- 全ビルド・テスト後に make clean を実行し、今回生成した *.d、*.o、*.exe を削除した。


EOF
