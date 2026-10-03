# esh pipe review & refactoring 実行計画

作成日: 2026-10-03
対象: 実装 Milestone 1〜9
対象外: 実装 Milestone 10 Cortex-M/RTOS 移植

## 1. 目的

`docs/esh_pipe_review_results.md` の整理方針を、順次実行と選別実行の両方が
できる小さな Review Milestone に分割する。

元の実装 Milestone と区別するため、本計画では `R0`〜`R10` を使用する。
各Milestoneは、原則として試験追加、最小変更、対応試験の順で完結させる。

## 2. 共通ルール

- EOF、EPIPE、pipeline status、stdioの既存仕様を維持する
- 仕様・公開APIの意味を変える案は `S` 系候補として分離する
- Milestone 10 のCortex-M/RTOS関連ファイルを変更しない
- 既存の未コミット・未追跡ファイルを削除、上書き、追加対象化しない
- `git add .` を使用しない
- GCCはMSYS2 MINGW64 shell経由でだけ実行する
- 既存生成物を消す`make clean`は元の作業ツリーで不用意に実行しない
- 可能なら固有`OUTDIR`、`TARGET`または独立スナップショットを使用する
- 新しいwarningを追加しない
- コメントは電報スタイル、80カラム以内とする
- typedefを新規追加しない
- 1 Milestone内で不具合修正と無関係な整形を混在させない

## 3. 状態表記

| 状態 | 意味 |
|---|---|
| `pending` | 未着手 |
| `active` | 作業中 |
| `blocked` | 仕様判断または外部条件待ち |
| `done` | 完了条件と試験を満たした |
| `skipped` | 選別実行で意図的に実施しない |

## 4. Review Milestone一覧

| ID | 名称 | 依存 | 初期状態 | 種別 |
|---|---|---|---|---|
| R0 | Baseline固定と試験入口整理 | なし | done | 基準 |
| R1 | 追加ファイルのコーディング規約整備 | R0 | done | Refactoring |
| R2 | Debug build修正 | R1 | done | 不具合修正 |
| R3 | 文書と実装の整合 | R1 | pending | 文書 |
| R4 | Pipeline失敗・再利用試験 | R1 | pending | 試験 |
| R5 | Pipeline cleanup整理 | R4 | pending | Refactoring |
| R6 | Pipe close・部分転送試験 | R1 | pending | 試験 |
| R7 | Scheduler waiter・release試験 | R1 | pending | 試験 |
| R8 | Executor不正bytecode試験 | R1 | pending | 試験 |
| R9 | Executor検査処理整理 | R8 | pending | Refactoring |
| R10 | Script/parser機械的整理 | R4、R8 | pending | Refactoring |
| RV | 全体回帰確認と記録更新 | 選択したR1〜R10 | pending | 検証 |

R6とR7は調査結果により修正が不要な場合、試験追加だけで`done`にできる。
仕様判断が必要になった場合は、そのMilestoneを`blocked`にしてS系候補へ移す。

## 5. 各Review Milestone

### R0: Baseline固定と試験入口整理

状態: `done`

#### 目的

変更前の再現可能な基準を固定し、既存作業ツリーを保護する。

#### 完了済み作業

- branch、HEAD、dirty tree確認
- 独立スナップショットでbaseline実行
- cortn/thread × O0/O2確認
- parser、executor、break、coroutine、cquote確認
- 既存warningと厳格build結果の分離
- 試験後に元の作業ツリーが不変であることを確認

#### 成果物

- `docs/esh_pipe_review_results.md`
- 本実行計画

### R1: 追加ファイルのコーディング規約整備

状態: `done`

依存: R0

#### 目的

指定された実装Milestone 1〜9のコミットで新たに追加されたソース、header、
assemblyを、現行`AGENTS.md`と
`docs/coding_style_summary_linux_kernel.md`に基づいて整える。

動作、公開APIの意味、EOF/EPIPE/status/I/O契約は変更しない。

#### 対象コミット

- `df9f4de Harden pipeline execution and add regression tests`
- `855026c Add Windows thread scheduler backend`
- `e4a3144 Connect applet stdio to pipeline process fds`
- `17638ab Add coroutine pipeline executor`
- `affc0a4 Add pipeline parser bytecode`
- `2ac5538 Add blocking pipe scheduler`
- `e951bb6 Add file and pipe core`
- `a06121b Add Windows coroutine backend`
- `e91509e Add pipe API data model`

`e4a3144`では新規ソース・header追加がないため、このコミットに由来する
新規ファイルはない。既存ファイルへの変更はR1の対象外とする。

#### 対象ファイル

Production code:

- `src/core/esh/cat.c`
- `src/core/esh/script.c`
- `src/core/esh/include/script.h`
- `src/core/esh/target/windows/sched_thread.c`
- `src/core/esh/include/esh_sched_backend.h`
- `src/core/esh/target/windows/sched_cortn.c`
- `src/core/esh/cortn.c`
- `src/core/esh/target/windows/cortn_context.c`
- `src/core/esh/target/windows/cortn_context.h`
- `src/core/esh/target/windows/cortn_switch.S`
- `src/core/esh/file.c`
- `src/core/esh/include/cortn.h`
- `src/core/esh/include/esh_errno.h`
- `src/core/esh/include/esh_file.h`
- `src/core/esh/include/esh_pipe.h`
- `src/core/esh/include/esh_sched.h`
- `src/core/esh/pipe.c`
- `src/core/esh/sched.c`

Test code:

- `src/core/esh/test/pipeline_exec/test_pipeline_exec.c`
- `src/core/esh/test/pipeline_exec/tty_config.h`
- `src/core/esh/test/pipeline/test_pipeline.c`
- `src/core/esh/test/pipeline/tty_config.h`
- `src/core/esh/test/sched/test_sched.c`
- `src/core/esh/test/pipe/test_pipe.c`
- `src/core/esh/test/cortn/test_cortn.c`

#### 作業

1. Source file section順を`tty.c`モデルへ合わせる
2. Include順、local定数、local型、static変数、prototype、関数定義を整理
3. Linux kernel styleに従いindent、空白、brace、命名、returnを確認
4. Header guardは既存の`__NAME_H__`形式を保持
5. コメントを電報スタイルへ直し、冠詞と末尾periodを可能な限り省略
6. コメントをコメント記号込み80カラム以内にする
7. Source/headerの80カラム超過を確認し、意味を変えずに折り返す
8. 対象内のtypedefを使用しない形へ変更し、必要な呼出し側を追従
9. 無関係な既存ファイルの全域整形を行わない
10. Formattingと同時に動作変更、不具合修正、API意味変更を行わない

#### Typedef対応

対象内の`esh_ssize_t`、`esh_proc_entry_t`、`cortn_entry_t`は現行規約に
従ってtypedefを使わない宣言へ変更する。呼出し側の宣言追従はR1に含めるが、
戻り値、引数、表現範囲、calling conventionは変更しない。

#### 完了条件

- 対象ファイルが現行`AGENTS.md`の構成・書式・コメント規則に適合
- 対象内にtypedefが残らない
- 意味変更を伴う差分がない
- 変更行に新規warningがない
- cortn/thread × O0/O2の関連試験が成功
- 既存未追跡ファイルを変更または追加対象化しない

#### 最小試験

- Coroutine O0/O2、`-Werror`
- Pipe cortn/thread O0/O2、`-Werror`
- Scheduler cortn/thread O0/O2、`-Werror`
- Pipeline executor cortn/thread O0/O2
- Parser test1、test2
- Executor test1〜12
- 変更ファイルのwhitespace、80-column確認

#### 実施結果

変更:

- 対象25ファイルを現行規約に照らして確認
- Source内のlocal型、static変数、prototype、関数定義順を整理
- Test `main()`をlocal helper定義より前へ移動し、prototypeを追加
- Include順を整理し、header guardは既存の`__NAME_H__`形式を保持
- `esh_ssize_t`、`esh_proc_entry_t`、`cortn_entry_t`を削除
- Offset検査用typedefを`_Static_assert`へ置換
- 必要な既存呼出し側`stdio_adapt.c`を`ptrdiff_t`へ追従
- コメントの冠詞、末尾period、80-columnを確認
- 動作、戻り値、calling convention、公開APIの意味は変更なし

試験:

- Coroutine O0/O2、`-Werror`: pass
- Pipe cortn/thread × O0/O2、`-Werror`: pass
- Scheduler cortn/thread × O0/O2、`-Werror`: pass
- Pipeline executor cortn/thread × O0/O2: pass
- Pipeline parser: pass
- Parser test1、test2: pass
- Executor test1〜12 cortn/thread: pass
- cquote build、実行: pass
- `git diff --check`: pass
- 対象ファイルのtypedef、末尾空白、80-column検査: pass

既知事項:

- 広いpipeline/executor buildの既存warningは継続
- R1変更による新規warningなし
- Milestone 10は未着手

### R2: Debug build修正

状態: `done`

#### 目的

通常buildでは隠れている`DEBUG_SHELL`分岐の不良を再現し、修正する。

#### 対象

- `src/core/esh/run.c`
- debug buildに必要なMakefileまたは試験入口

#### 作業

1. `DEBUG_SHELL=1`で失敗する最小buildを固定
2. `ffush(stdout)`を修正
3. cortn/threadのdebug buildを確認
4. debug出力がpipe payloadへ混入しないことを確認

#### 完了条件

- 両backendのdebug build成功
- debug出力はstderr/consoleだけへ出力
- 通常buildとpipeline試験が退行しない

#### 最小試験

- 対象debug build
- Pipeline executor cortn/thread O0

#### 実施結果

- `run.c` の debug print macro にある `ffush()` を `fflush()` に修正
- `DEBUG_SHELL` を build option から上書き可能に変更
- `DEBUG_SHELL=1` の最小 compile 確認に合格
- coroutine/thread 両 backend の debug pipeline 結合試験に合格
- debug print が pipeline payload に混入しないことを確認
- `DEBUG_SHELL` の既定値 `0` を維持し、通常 build の動作を変更しない構成
- Cortex-M/RTOS 移植には未着手

### R3: 文書と実装の整合

状態: `pending`

#### 目的

実装済み仕様を文書へ正確に反映する。

#### 対象

- `docs/esh_pipe_api.md`
- 必要に応じてpipe関連文書

#### 作業

1. Windows coroutine stackを16 KiBから64 KiBへ訂正
2. 16 KiBから変更した理由とWindows限定値であることを記載
3. stdio、thread、regression文書との重複・矛盾を確認
4. MCU stack sizingはMilestone 10として未決定のまま残す

#### 完了条件

- 現行定数と文書が一致
- Windows固有値と将来のMCU判断を混同しない
- 新しい仕様を追加しない

#### 最小試験

- 文書内定数とheader定数の目視照合
- C/header変更がなければbuild不要

### R4: Pipeline失敗・再利用試験

状態: `pending`

#### 目的

Pipeline準備失敗後のrollbackと次回実行を試験で固定する。

#### 対象

- `src/core/esh/test/pipeline_exec/`
- 必要な場合だけtest hook

#### 試験項目

- 1本目のpipe作成後、後続pipe確保失敗
- 一部backend prepare成功後、後続prepare失敗
- 失敗後のfile、inode、fd、backend slot回収
- 同一process配列を使った次回pipeline成功
- cortn/thread × O0/O2

#### 完了条件

- 失敗直後に全pool使用数が基準値へ戻る
- 次回pipelineが成功する
- 既存pipeline試験が成功する
- production code変更が必要ならR5へ分離する

#### 最小試験

- Pipeline executor cortn/thread O0/O2

### R5: Pipeline cleanup整理

状態: `pending`

依存: R4

#### 目的

Pipelineの成功・失敗経路を、所有段階が読み取れる終了処理へ整理する。

#### 対象

- `src/core/esh/exe.c`
- 必要な内部header

#### 作業

1. 作成済みpipe数と準備済みbackend数を明示
2. fd close経路を共通化
3. backend release経路を共通化
4. closeしてからlock外でrelease/joinする順序を維持
5. 成功時の最終command statusを維持

#### 完了条件

- R4試験が変更前後で同じ結果
- cleanup順序が単一経路から読み取れる
- EOF/EPIPE通知前にbackendを解放しない
- 全終了前にglobal poolを再初期化しない

#### 最小試験

- R4全試験
- Pipe、scheduler cortn/thread O0
- Executor test1〜10

### R6: Pipe close・部分転送試験

状態: `pending`

#### 目的

EOF、EPIPE、部分転送、close wakeupの現行動作を明示する。

#### 対象

- `src/core/esh/test/pipe/`
- `src/core/esh/pipe.c`は不具合確認時だけ変更

#### 試験項目

- zero-length read/writeはfdとpeerを検査せず0
- 最終writer close後、残データ読出し後にEOF
- 空pipeでwriter存在時はread待ち
- 満杯pipeでreader存在時はwrite待ち
- reader closeでwriterをwakeしEPIPE
- writer closeでreaderをwakeしEOF
- ring wrap境界で順序維持
- 非backend利用時のEAGAINと部分byte数
- blocking write途中のreader close

#### 完了条件

- 現行動作がassertionで固定される
- 仕様判断が必要な項目はproduction codeを変更せずS系へ移す
- 両backend、O0/O2、`-Werror`で成功

#### 最小試験

- Pipe cortn/thread O0/O2
- Scheduler cortn/thread O0

### R7: Scheduler waiter・release試験

状態: `pending`

#### 目的

wait list、wake、release、thread joinの境界を試験で固定する。

#### 対象

- `src/core/esh/test/sched/`
- `src/core/esh/sched.c`
- Windows scheduler backendは不具合確認時だけ変更

#### 試験項目

- 複数waiterの全wakeと再検査
- 待機中processのreleaseとwait list切離し
- wake直後のrelease
- backend yield失敗時のwait list回収
- thread handle数が反復後に増加しない
- TLS current processがcontrollerとworkerで混線しない
- runnableなし、unfinishedありのdeadlock検出

#### 完了条件

- wait channelに解放済みprocessが残らない
- native handleをすべてclose
- 両backend、O0/O2、`-Werror`で成功
- 修正が必要な場合はcoroutineとthreadの共通契約を変えない

#### 最小試験

- Scheduler cortn/thread O0/O2
- Pipe cortn/thread O0

### R8: Executor不正bytecode試験

状態: `pending`

#### 目的

Executorが不正bytecodeを範囲外参照なしで拒否し、再利用できることを固定する。

#### 対象

- `src/core/esh/test/exe/`または専用executor試験
- `src/core/esh/exe.c`は試験追加後に必要最小限だけ修正

#### 試験項目

- 不明opcode
- operand欠落
- command ID、string ID、variable ID、label ID範囲外
- 未終端string/variable
- stack underflow/overflow
- processなしのargument/execute
- pipeline段数超過
- jump先がcode範囲外
- 失敗後にvalid bytecodeを実行できる

#### 完了条件

- 各不正入力を失敗として返す
- 範囲外read/writeを行わない設計を確認
- 失敗後のexecutor再利用に成功
- 既存golden test1〜10が一致

#### 最小試験

- 新規不正bytecode試験
- Executor test1〜10
- Pipeline executor cortn/thread O0

### R9: Executor検査処理整理

状態: `pending`

依存: R8

#### 目的

`exe_valid_instruction()`と実行switchの重複を減らし、検査責務を明確にする。

#### 対象

- `src/core/esh/exe.c`
- 必要な場合だけprivate helper宣言

#### 作業

1. 命令幅判定を集約
2. operand種別ごとの検査を分離
3. stack、process、argv前提を分離
4. 実行switch内の重複境界検査を必要最小限へ整理
5. 命令追加時に検査追加箇所が分かる構成にする

#### 完了条件

- R8の全試験が成功
- 既存valid bytecodeの意味を変更しない
- public APIを変更しない
- 新規warningなし

#### 最小試験

- R8全試験
- Executor test1〜10
- Parser test1、test2
- Pipeline executor cortn/thread O0/O2

### R10: Script/parser機械的整理

状態: `pending`

依存: R4、R8

#### 目的

現在のsingleton、status、変数保持、error後初期化を維持して可読性を改善する。

#### 対象

- `src/core/esh/script.c`
- `src/core/esh/par.c`の局所helper

#### 作業

1. `esh_script_line()`の成功/error cleanupを共通化
2. variable table退避・復元を一箇所にする
3. parser error flag操作の局所的重複を整理
4. parser metadata寿命をコメントと試験で明示
5. debug用未使用要素は設定別buildを確認してから整理

#### 実施しない事項

- 戻り値型または意味の変更
- parser transaction化
- parser context化
- error時のsymbol/variable rollback追加
- singleton廃止

#### 完了条件

- 未完ブロックとstatus `1`の現行動作を維持
- error時も既存どおりvariable tableを保持
- 引数付きbreak metadataの寿命を維持
- parser、executor、script統合試験が成功

#### 最小試験

- Parser test1、test2
- Executor test1〜12
- Pipeline script integration cortn/thread O0/O2

### RV: 全体回帰確認と記録更新

状態: `pending`

#### 目的

選択して実施したMilestone群が既存仕様を壊していないことを確認する。

#### 必須確認

- Pipeline executor: cortn/thread × O0/O2
- Pipe: cortn/thread × O0/O2、`-Werror`
- Scheduler: cortn/thread × O0/O2、`-Werror`
- Coroutine: O0/O2、`-Werror`
- Parser: test1、test2
- Executor: test1〜12
- cquote
- Windows target build: cortn/thread
- Windows対話: `sam1`、`sam2 1 10 100`、`sam3 -a -b string`
- Windows対話: 2段/4段pipeline、引用変数
- file、inode、backend slot、thread handle回収
- 変更C/headerのwhitespace、80-column、comment style

#### 記録

- 実行したMilestone
- skippedまたはblockedにしたMilestoneと理由
- 試験結果
- 既存warningと新規warning
- 未確認事項
- 仕様変更候補

## 6. 選別実行方法

### 6.1 最小修正セット

明白な不具合と文書不一致だけを直す。

```text
R0 -> R1 -> R2 -> R3 -> RV
```

### 6.2 Pipe/scheduler安全性セット

I/Oと待機・解放を重点確認する。

```text
R0 -> R1 -> R6
         -> R7
   -> RV
```

R6とR7はR1後に個別実行でき、相互依存しない。

### 6.3 Pipeline cleanupセット

失敗rollbackと終了処理だけを整理する。

```text
R0 -> R1 -> R4 -> R5 -> RV
```

### 6.4 Executor安全性セット

不正bytecode防御と検査処理を整理する。

```text
R0 -> R1 -> R8 -> R9 -> RV
```

### 6.5 Script/parser整理セット

前提試験を固定してから機械的整理を行う。

```text
R0 -> R1 -> R4 -> R8 -> R10 -> RV
```

### 6.6 推奨全実行順

```text
 R0
 -> R1
 -> R2
 -> R3
 -> R4
 -> R5
 -> R6
 -> R7
 -> R8
 -> R9
 -> R10
 -> RV
```

R2、R3、R4、R6、R7、R8は独立性が高いため、R1後であれば順番を
入れ替えられる。Refactoringを伴うR5、R9、R10は、それぞれの前提試験
Milestone完了後に行う。

## 7. 仕様変更候補

次はReview Milestoneから除外し、個別承認後にだけ実施する。

| ID | 候補 | 影響 |
|---|---|---|
| S1 | `esh_script_line()`結果型分離 | API、呼出し側 |
| S2 | 部分write後のEPIPE規則確定 | I/O意味、試験 |
| S3 | Parser error rollback | Parser状態、変数保持 |
| S4 | Parser context化 | API、singleton前提 |
| S6 | `cat`のEOF/read error分離 | Applet status |

Review中に仕様判断が必要になった場合、対象変更を止め、次を提示する。

1. 現行動作
2. 再現試験
3. 選択肢
4. 互換性への影響
5. 推奨案

## 8. Milestone更新方法

各Milestone開始時に状態を`active`へ変更し、終了時に次を追記する。

```text
状態: done
変更:
- ...
試験:
- ...: pass/fail
未確認:
- ...
```

選別実行で見送る場合は`skipped`、仕様判断待ちは`blocked`とし、理由を記録する。
複数Milestoneを一つの未分離変更として処理しない。

## 9. 完了条件

- 選択したReview Milestoneがすべて`done`または理由付き`skipped`
- `blocked`項目は仕様変更候補として明示
- RVの該当試験が成功
- 既存EOF、EPIPE、status、I/O契約を維持
- 新規warningなし
- 既存未コミット・未追跡ファイルを保持
- Milestone 10は未着手
