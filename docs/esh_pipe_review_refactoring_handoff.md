# esh pipe: review & refactoring 引継ぎ書

作成日: 2026-10-03
対象リポジトリ: `C:\usr\src\MCUFW`
開始時ブランチ: `feature/pipe`
基準コミット: `df9f4de Harden pipeline execution and add regression tests`
作成終了時 HEAD: `4ee9861 update AGENTS.md` (規約更新)

## 1. 次タスクの目的と範囲

Milestone 1〜9 で実装した Windows pipe 機能をレビューし、既存仕様を
維持しながら可読性、責務分離、終了処理、境界検査、試験を整理する。

**Milestone 10 は未着手のまま残す。** Cortex-M の context switch、
MSP/PSP、FPU、割り込み規則、MCU stack 計測、RTOS backend の実装は
今回の review & refactoring に含めない。STM32 への移植を開始しない。

この引継ぎ書の作成では、新しいチャットの作成、実装変更、コミット、push は
行っていない。次タスクでは、まずレビュー結果と整理方針を示す。
互換性や API の意味を変える案は、機械的な整理と分けて判断する。

## 2. 実装履歴

| Milestone | コミット | 内容 |
|---|---|---|
| 1 | `e91509e` | Pipe API とデータモデル |
| 2 | `a06121b` | Windows coroutine backend |
| 3 | `e951bb6` | File/fd/pipe core |
| 4 | `2ac5538` | Blocking I/O と scheduler |
| 5 | `affc0a4` | Lexer/parser と pipeline bytecode |
| 6 | `17638ab` | Coroutine pipeline executor |
| 7 | `e4a3144` | Applet stdio と process fd の接続 |
| 8 | `855026c` | Windows thread scheduler backend |
| 9 | `df9f4de` | 回帰試験、境界検査、Windows 対話入力の接続 |

Milestone 9 は 18 ファイル、788 行追加・105 行削除。
新規ファイルは `cat.c`、`script.c`、`include/script.h`、
`docs/esh_pipe_regression.md`。
最新コミット以降の変更は開始時に再確認すること。

## 3. 最初に読む文書

1. 作業ディレクトリの `AGENTS.md`。現行内容に従う
2. `docs/coding_style_summary_linux_kernel.md`
3. `.agents/skills/esh_pipe/references/esh_pipe_coroutine_summary.md`
   の「11. 実行作業プラン」、特に Milestone 9/10 と Definition of Done
4. [回帰試験と堅牢化](esh_pipe_regression.md)
5. [Thread backend](esh_pipe_thread.md) と [stdio 接続](esh_pipe_stdio.md)
6. `esh_pipe_api.md`、`esh_pipe_core.md`、`esh_pipe_sched.md`、
   `esh_pipe_parser.md`、`esh_pipe_executor.md` を対象コードに合わせて読む

`.agents/` と coding style 文書は現状未追跡。
別 checkout/worktree や clone には存在しない可能性がある。
必要資料と現行 AGENTS.md を引継ぎ先で読めることを確認する。
参照計画は作業背景であり、今回の依頼範囲を Milestone 10 に拡張しない。

## 4. 構成と固定している仕様

| 層 | 主なファイル | 責務 |
|---|---|---|
| Console | `tty.c`, `script.c`, `include/script.h` | 入力、複数行蓄積、実行、変数保持 |
| Interpreter | `lex.c`, `par.c`, `code.c`, `exe.c`, `break.c` | Token/bytecode、制御文、pipeline |
| Pipe core | `file.c`, `pipe.c`, `include/esh_file.h`, `include/esh_pipe.h` | Fd、参照数、ring、EOF/EPIPE |
| Scheduler | `sched.c`, `include/esh_sched.h`, `include/esh_sched_backend.h` | Process と wait/wakeup 契約 |
| Coroutine | `cortn.c`, Windows `cortn_context.c`, `cortn_switch.S`, `sched_cortn.c` | Context、stack、backend |
| Thread | Windows `sched_thread.c` | TLS、SRW lock、CV、thread handle |
| Stdio | Windows `stdio_adapt.c`, `include/bltn.h`, `echo.c`, `cat.c` | 既存 applet の入出力経路 |
| Build | Windows `sched_backend.mk`、各 Makefile | Backend 選択と条件付き接続 |

Windows 固有ファイルは `src/core/esh/target/windows/` にある。
`struct proc` は shell 引数を所有し、`struct esh_proc` を埋め込む。

- 最大 4 段ネスト、pipe 3 本、256 byte/pipe、fd 12/process、file entry 12
- Anonymous pipe のみ。通常ファイル、path、物理ストレージは実装しない
- Pipeline 全体の status は最終 command の status
- Reader がなくなれば `-ESH_EPIPE`。signal は送らない
- 最終 writer close 後、残データを読み切って EOF
- Zero-length read/write は fd/peer の検査なしに 0
- Stdout と次段 stdin だけを接続。stderr/debug は console
- Default は `ESH_SCHED_BACKEND=cortn`、切替値は `thread`
- Thread backend も controller が一つずつ実行を許可する。
  Native thread 化は applet の自動並列実行を意味しない
- Coroutine stack は Windows では現在 64 KiB × 4。
  16 KiB では CRT formatting の結合試験で問題が出たため拡大した
- Kernel lock は非再帰。条件検査と wait 登録を lock 内で行い、復帰後に再検査
- Fd close の後、lock 外で backend release/join。全終了前に pool を初期化しない

## 5. 既知の制約と維持すべき挙動

- Lexer/parser/console は singleton。再入、nested pipeline、複数 controller、
  background 実行、CPU-only command の強制キャンセルは未対応
- Command substitution は数値戻り値の代入。stdout capture ではない
- While の status は既存 executor の最終条件評価などの規則に従う
- Console は固定長。長すぎる入力、token、symbol、bytecode を動的拡張しない
- `esh_script_line()` は呼出元の `TTY_NBUF` バイトの buffer を変更する。
  行末 token が必要。未完ブロックの 1 と完了 status の 1 は区別しない
- Parser エラーで追加済み bytecode/symbol は巻き戻さない。
  エラーコードは実行せず、parser/code を再初期化する契約
- Console はエラー時も変数テーブルを保持する。
  解析途中で追加した未設定変数が残る可能性がある
- `cat` は引数なしのストリーム転送のみ。read エラーと EOF は区別しない
- `printf`、`putchar`、`fprintf(stdout, ...)`、`getchar` が pipe 対応。
  `puts/fputs/fwrite/vprintf` と FILE buffering は対応範囲外
- Pipe stdin の `scanf` は入力を消費せず EOF を返す
- Windows formatter は呼出しごとに host buffer を malloc/free する。
  Portable pipe core の必須動的確保ではない

## 6. レビューの優先順と候補

以下はレビュー対象・整理案であり、新たな不具合として確定した一覧ではない。

### A. 正しさと終了処理

- EOF/EPIPE、部分転送、早期終了、失敗 rollback、waiter の切離しを再確認
- Native backend の check/register/wait、token/stop、resume 報告との競合、
  TLS と lock 所有権、join/handle close を状態遷移に沿って確認
- 不正 bytecode、stack/pc/label/ID、argv、pool の全入口と内部呼出しを確認。
  Milestone 9 の検査追加を網羅的な安全性証明と扱わない
- Parser エラー後の状態、break が参照する parser metadata の寿命、
  script のリセットと変数保持の整合性を確認
- Header の lock/ref 契約と実際の呼出しが一致しているか確認

### B. 責務分離と可読性

- `exe_valid_instruction()` と実行 switch の重複、長さ、検査の配置を整理
- Single command と pipeline の初期化・実行・後始末の共通化を検討
- `par.c` の helper と global `parse_error` の依存関係を整理。
  Transaction 化や context 化は仕様・API 変更と分けて扱う
- `script.c` の成功/error 経路、戻り値の曖昧さ、初期化 API を確認。
  状態と command status の分離は候補だが、無断で互換性を変えない
- 新規・変更ソースを現行 AGENTS.md のセクション順に整える
- 新しい typedef は追加しない。既存 typedef を全域で削除する大変更は避け、
  今回の対象内での対応方針を先に説明する
- Warning は既存と新規を分け、再現するものから小さく整理する
- Debug 設定も確認する。例: `run.c` の DEBUG_SHELL 分岐には
  `ffush(stdout)` という記述がある。通常 build 成功だけで debug 対応を断定しない

### C. 試験と文書

- 境界条件の拒否だけでなく、部分変更・失敗後の再利用を assertion で確認
- Console 出力を目視確認する試験と、byte 一致を検査する試験を区別
- 正常 status、shell 制御文 status、負の内部 error を区別して期待値を整理
- 未追跡の break 試験を clean checkout でどう再現するか確認。
  既存ファイルを承諾なくコミット対象に混ぜない
- 古い文書と現行実装の相違を整理。`esh_pipe_api.md` の初期 stack 16 KiB と
  現行 64 KiB は歴史的な記述。stdio/thread/regression 文書と整合を取る
- Sanitizer 未実施を「検証済み」に置き換えない

## 7. 検証基準と再現手順

Milestone 9 の確認結果は `docs/esh_pipe_regression.md` に記録済み。
本引継ぎ書の作成時には試験を再実行していない。

- Pipeline/pipe/scheduler: cortn/thread × O0/O2 成功
- Pipeline: 0/1/256/257/2089 byte × 2/3/4 段 × 10 回。
  Binary/NUL、変換・順序、早期終了、枯渇、終了後資源回収を確認
- Coroutine 単体: O0/O2、stack guard 成功
- Executor: test1〜10 の golden 出力が両 backend で一致
- Parser: 既存 2 suite の golden 出力一致。cquote: build/実行成功
- 引数付き break、引用変数、while/if、数値 substitution と復帰試験成功
- Windows 対話: 両 backend で sam1/sam2/sam3、2/4 段、引用変数成功
- Pipe/scheduler/cortn は Werror で成功。広い結合 build には既存 warning が残る

**GCC は MSYS2 MINGW64 shell 内からのみ起動する。**
PowerShell から MinGW gcc を直接呼ぶと、cc1.exe の DLL 探索で
`clock_gettime64` エラーが多発した。典型的な起動例:

```powershell
$env:MSYSTEM = 'MINGW64'
& 'C:\msys64\usr\bin\bash.exe' -lc `
  'cd /c/usr/src/MCUFW/src/core/esh/test/pipeline_exec && bash ./run_test.sh'
```

MSYS2 shell 内での基本 suite:

```sh
cd /c/usr/src/MCUFW/src/core/esh/test/pipeline_exec
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

Executor の両 backend は明示的に rebuild して test1〜10 と golden を比較する。
`run_test.sh` だけで両 backend を検証したことにしない。
Coroutine/cquote は各 Makefile で build し、test executable を実行する。
未追跡の `test/exe/run_break_argument_test.sh` は test11/test12 を使う。

Backend/最適化を切り替えるときは `make -B`、または別 OUTDIR/TARGET を使う。
Makefile の `clean` は既存 exe/DLL/output を削除する場合があるため、
実行前に対象を確認する。可能なら独立した試験出力先を使う。

Windows 対話は `src/target/windows` で両 backend を rebuild し、
`sam1`、`sam2 1 10 100`、`sam3 -a -b string`、`echo hello|cat`、
`echo four|cat|cat|cat`、`k=3` と `echo "$k"|cat` を確認する。

MinGW64 GCC 16.1.0 で ASan/UBSan を試したが、link 時に
`-lasan`/`-lubsan` が見つからず実行不可だった。
利用可能な host での実行手順は regression 文書を参照する。
手動 coroutine stack 切替の sanitizer fiber 対応は未実装なので、thread を使う。
過去の試験 log は `C:\msys64\tmp\mcufw-m9-*.log` にあるが、
ローカル生成物なので引継ぎ先での存在を前提にしない。

## 8. 作業ツリーの注意

引継ぎ書作成前、Milestone 9 の実装差分は全てコミット済み。
開始時にあった `AGENTS.md` の別更新は、作成中に
`4ee9861 update AGENTS.md` としてコミットされた。
作成終了時には追跡ファイルの未コミット差分はない。
この引継ぎ書の作業ではその更新を変更・コミットしていない。

既存の未追跡ファイル・ディレクトリには次がある:

- `.agents/`、`cscope.files`、`cscope.out`、coding style 文書
- `src/core/esh/test/exe/run_break_argument_test.sh`、`test11.sh`、`test12.sh`
- `src/target/NUCLEO-U575ZI-Q/`、各 `.bak`
- Cortn/cquote/pipe/sched と Windows の既存 build 出力、exe、DLL

これらを不要物と判断して削除しない。`git add .` で巻き込まない。
試験生成物を消す場合も、今回生成した正確なパスを確認してから行う。
本引継ぎ書自体は新規文書で、現時点では未コミット。

## 9. 次タスクの進め方と完了条件

1. 現行 AGENTS.md、資料の所在、branch/HEAD、dirty tree を確認
2. コミット済み実装と試験を読み、baseline を再現
3. レビュー結果を重要度、根拠、再現試験付きで提示。
   不具合・整理案・仕様変更案を区別する
4. 不具合なら最小の再現試験を追加し、修正と機械的整理を分離
5. 小さな単位で整理し、対応 suite と両 backend を確認
6. 最後に既存 golden、対話入力、資源回収を確認して文書を更新
7. 変更一覧、試験結果、未確認事項を報告。コミット/push はユーザーの指示に従う

完了条件は、既存の EOF/EPIPE/status/I/O 契約を維持し、新規 warning を増やさず、
回帰試験が成功し、未対応・未検証事項が明記されていること。
Milestone 10 はこの段階でも未着手として残す。

## 10. 新しいタスクの依頼文例

> C:\usr\src\MCUFW の
> docs/esh_pipe_review_refactoring_handoff.md を読んで、Milestone 1〜9 の
> review & refactoring を進めてください。Milestone 10 の Cortex-M/RTOS
> 移植は未着手のまま残してください。まず現行 AGENTS.md と作業ツリー、
> baseline 試験を確認し、レビュー結果と整理方針を提示してください。
> 既存の未コミット・未追跡ファイルは保持し、仕様変更案は整理と分けて相談してください。
