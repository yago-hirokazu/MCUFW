# Milestone 9: 回帰試験と堅牢化

## Windows 対話入力

Windows Makefile は `USE_STACKMACH`、`ESH_SCRIPT` を有効にする。
入力を lexer、parser、executor に渡し、既存の pipeline 実行部を利用する。
他のターゲットでは `ESH_SCRIPT` を定義しない限り従来の直接実行を維持する。
既存 builtin の番号は変更していない。末尾に `cat` を追加した。

MSYS2 MINGW64 shell で実行する。PowerShell から gcc を直接起動しない。

```sh
cd src/target/windows
make -B ESH_SCHED_BACKEND=cortn
./main.exe
```

thread backend は `ESH_SCHED_BACKEND=thread` で切り替える。
対話例:

```sh
echo hello|cat
echo four|cat|cat|cat
k=3
echo "$k"|cat
```

`cat` は引数なしのストリーム転送専用で、ファイル名は扱わない。
先頭の `cat` は console 入力を待つ。末尾の `cat` は console へ出力する。
`if`、`while` は複数行を蓄積し、最外側のブロックが閉じてから実行する。
変数は完了した入力間で保持する。エラーでは未実行のコードを破棄する。

## API と所有権

`esh_script_init()` は単一 console セッションを初期化する。
`esh_script_line()` は NUL 終端された `TTY_NBUF` バイトの呼出元バッファを
変更する。行末 token が必要で、上限超過・欠落を切り詰めて実行しない。
戻り値は負値が拒否、未完ブロックは 1、完了時は最後の executor status。
完了 status の 1 と未完の 1 は区別しない。公開非同期 API ではない。

lexer、parser、console セッションは singleton で、再入・並列利用は禁止。
parser が `PARINVLD` を返したコードは実行せず、`par_init()` と `code_init()`
で再初期化する。失敗直前まで追加された命令・記号はロールバックしない。
console はエラー時も変数テーブルを保持するため、解析途中で登録した未設定の
変数が残ることがある。

kernel lock は非再帰の単一 lock。pipe 条件と待機リストを保護する。
I/O 待機は lock 内で登録し、backend が解放・待機・再取得を行う。
終了時は fd を close した後、lock 外で backend を release/join する。
controller は一つだけとし、全 process の終了前に pool を初期化しない。
詳細は `esh_pipe_thread.md` と `esh_pipe_stdio.md` を参照する。

## 境界検査

- executor の pc/sp を int にし、128 命令到達時の符号付き char 折返しを解消
- NULL、pc、命令長、不明 opcode、applet ID、記号 ID、label、stack を検査
- OP_NOP は pc を進め、同じ命令で停止し続けない
- parser のコード、文字列、変数、label、ネスト、条件・ループ管理領域を検査
- lexer の長すぎる記号・引用文字列を拒否
- tokenizer の未終端入力・token 上限を拒否し、64-bit pointer の切詰めを解消
- break のループ添字を検査
- pipeline の全 applet を確認してから process 状態を初期化

## 回帰試験

```sh
cd src/core/esh/test/pipeline_exec
bash ./run_test.sh
cd ../pipe
bash ./run_test.sh
cd ../sched
bash ./run_test.sh
cd ../par
bash ./run_test1.sh
bash ./run_test2.sh
cd ../exe
bash ./run_test.sh
```

pipeline、pipe、scheduler は cortn/thread × O0/O2 を試験する。
pipeline の転送量は 0、1、256、257、2089 byte、段数は 2、3、4。
各組合せを 10 回反復し、NUL を含む binary データ、順序、変換結果、
file/pipe の使用数ゼロを確認する。2089 byte は 256-byte ring を数回周回する。
producer 先行終了、consumer 早期終了、中間 filter のエラー、zero-length I/O、
pipe 枯渇、backend 部分準備失敗、回収後の再実行も確認する。
fd/file 枯渇は pipe 単体試験、process/stack pool 枯渇と handle 回収は scheduler
試験で確認する。coroutine stack guard は cortn 単体試験で確認する。

console 自動試験は 4 段、while/if、変数と引用変数、数値 command substitution、
`break 2`、構文エラー・ネスト超過後の復帰を含む。
境界試験はコード容量ちょうど、命令途中切れ、不正 ID/opcode、stack の上限、
記号・変数・label の枯渇、NULL・未終端入力を含む。

## 既知の制約

- 同時 pipeline は一つ、最大 4 段。nested pipeline/background 実行は未対応
- command substitution は従来どおり数値戻り値の代入で、stdout capture ではない
- while の status は最後の条件評価など既存 executor の規則を維持する
- console の行・token・記号・コード容量は固定。長い script の動的拡張はしない
- 対話入力は既存 scanner の文字制限と引用符処理に従う
- CPU-only コマンドの強制キャンセル、signal、通常ファイルは未対応
- getchar は read エラーと EOF をともに EOF として返す。cat も区別しない
- stdio の対応範囲と pipeline 中の scanf 制限は `esh_pipe_stdio.md` のとおり

## Sanitizer

2026-10-03 の MSYS2 MinGW64 GCC 16.1.0 で、thread backend に
`-fsanitize=address,undefined` を付けて compile/link を試行した。
リンク時に `-lasan` と `-lubsan` が見つからず、実行試験はできなかった。
sanitizer による不正アクセス・リーク検証済みとはしない。
利用可能な host では同じ suite を次の設定で実行する:

```sh
cd src/core/esh/test/pipeline_exec
make -B ESH_SCHED_BACKEND=thread OUTDIR=_output_san TARGET=test_san.exe \
  OPTFLAGS='-O1 -fsanitize=address,undefined' \
  LDFLAGS='-lwinmm -fsanitize=address,undefined'
./test_san.exe
```

coroutine の手動 stack 切替は sanitizer 専用の fiber 対応を実装していない。
sanitizer 対象には thread backend を使用する。

## 確認結果 (2026-10-03)

- pipeline、pipe、scheduler: cortn/thread × O0/O2 成功
- coroutine 単体: O0/O2 成功、stack guard を確認
- executor: 既存 1〜10 の golden 出力が両 backend で一致
- parser: 既存 2 suite の golden 出力が一致
- cquote: 既存試験の build・実行成功
- 引数付き break、引用変数: 既存追加試験と console 自動試験成功
- Windows 対話: 両 backend で sam1/sam2/sam3、2/4 段、引用変数成功
- 新規 warning なし。従来の warning は残るが、pointer 切詰めと
  non-void 関数の return 欠落は解消。pipe/scheduler/cortn は Werror で成功
- sanitizer: runtime library 不在で実行不可

未対応の Cortex-M/RTOS 移植は Milestone 10 の対象とする。
