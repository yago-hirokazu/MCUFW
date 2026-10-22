# esh pipe Milestone 1〜9 review & refactoring 初期レビュー結果

## 現況（2026-10-22）

初期レビュー時点の記録。file・pipe・schedulerはcore/sysへ移行済み。stackは64 KiB、stdioとthread backendは実装済み。

現行仕様は[文書案内](README.md)、[pipe core](pipe_core.md)、
[RAM block I/O](ram_block_device.md)を参照する。以下の計画・試験結果は記載当時の記録で、
今回の文書整備では試験を再実行していない。

作成日: 2026-10-03
対象リポジトリ: `C:\usr\src\MCUFW`
対象ブランチ: `feature/pipe`
確認時 HEAD: `4ee9861 update AGENTS.md`

## 1. 目的と範囲

Milestone 1〜9 で実装した Windows pipe 機能について、現行規約、作業ツリー、
baseline 試験、主要な所有権・終了処理・状態遷移を確認し、今後の
review & refactoring 方針を記録する。

Milestone 10 の Cortex-M/RTOS 移植は対象外とし、未着手のまま残す。
Cortex-M context switch、MSP/PSP、FPU、割り込み規則、MCU stack 計測、
RTOS backend の実装は開始しない。

本書作成時点ではソースコード、試験、既存文書を変更していない。
既存の未コミット・未追跡ファイルも変更または削除していない。

## 2. 確認した基準

### 2.1 Repository 状態

- branch: `feature/pipe`
- HEAD: `4ee986167b403e538ddbd855bc9711ac527090a6`
- Milestone 1〜9: `e91509e`〜`df9f4de`
- 追跡済みファイルの未コミット差分: なし
- 引継ぎ書記載の未追跡ファイル・生成物: 存在を確認し、そのまま保持

Baseline 試験は元の作業ツリーを変更しない独立スナップショットで実行した。
試験後は作成した一時スナップショットだけを削除し、元の作業ツリーが
開始前と同じ状態であることを確認した。

### 2.2 適用規約

- 現行 `AGENTS.md`
- `docs/coding_style_summary_linux_kernel.md`
- `docs/pipe_review_refactoring_handoff.md`
- `.agents/skills/esh_pipe/references/esh_pipe_coroutine_summary.md`
- pipe 関連設計文書と回帰試験文書

特に次を維持条件とする。

- Windows-first、portable core、固定長資源
- EOF、EPIPE、status、stdio の現行契約
- pipe状態検査とwait登録の不可分性
- 待機から復帰した後の条件再検査
- fd close後、kernel lock外でbackend release/join
- typedefを新規追加しない
- コメントは電報スタイル、80カラム以内

## 3. Baseline 試験結果

次の試験はすべて成功した。

- Pipeline executor: cortn/thread × O0/O2
- Pipe core: cortn/thread × O0/O2、`-Werror`
- Scheduler: cortn/thread × O0/O2、`-Werror`
- Parser: `run_test1.sh`、`run_test2.sh`
- Executor: test1〜10
- 引数付きbreak試験: test11、test12
- Coroutine: O0/O2、`-Werror`
- cquote: build、実行

広い pipeline/executor build では既知warningが再現した。主な内容は次のとおり。

- `lex.c`: 文字列切り詰め、未使用関数
- `par.c`: `char`添字、fallthrough、未使用変数・関数
- `help.c`: 未使用引数
- `test.c`: 未使用関数

Pipe、scheduler、coroutine の厳格buildではwarningは発生しなかった。
広いbuildのwarningは既存warningとして、新規変更によるwarningと区別する。

## 4. 初期レビュー結果

### 4.1 修正対象として確定した事項

#### 4.1.1 Debug build のコンパイル不良

`src/core/esh/run.c` の `DEBUG_SHELL` 分岐に `ffush(stdout)` がある。
通常設定では展開されないためbaselineを通過するが、debug buildでは
未定義シンボルになる。

これは仕様変更を伴わない明白な修正対象とする。

#### 4.1.2 設計文書と実装のstack容量不一致

`docs/pipe_api.md` は Windows coroutine stack初期容量を16 KiBと記載する。
現行実装は `src/core/esh/include/cortn.h` で64 KiBを使用する。

16 KiBでCRT formatting結合試験に問題が発生し、64 KiBへ変更した経緯は
stdio文書と引継ぎ書に記録されている。現行実装と実測結果に合わせて
API文書を更新する。

#### 4.1.3 現行typedef規約との不一致

Milestone 1〜9 で追加された公開ヘッダに次のtypedefがある。

- `esh_file.h`: `esh_ssize_t`
- `esh_sched.h`: `esh_proc_entry_t`
- `cortn.h`: `cortn_entry_t`

現行 `AGENTS.md` の「typedefを使わない」に抵触する。Review Milestone R1で
対象ファイルの規約整備として、表現範囲やcalling conventionを変えずに
typedefを使用しない宣言へ変更する。

### 4.2 仕様を維持した整理対象

#### 4.2.1 Executor命令検査

`exe.c` の `exe_valid_instruction()` と実行switchには、命令幅、operand種別、
stack条件、process条件に関する知識が重複している。

動作を変えず、命令分類と個別状態検査を小さなhelperへ分離する余地がある。
不正bytecode試験を先に追加し、検査漏れを防いでから整理する。

#### 4.2.2 Pipeline終了処理

Pipelineの準備、fd close、backend releaseは、現状でも順序を意識して
実装されている。一方、成功、pipe準備失敗、backend準備失敗、実行失敗の
終了経路が複数箇所に分散している。

所有段階を明示し、共通cleanup経路へ寄せられるか検討する。次の順序は維持する。

1. 全process fdをclose
2. closeによってEOF/EPIPEを通知し、waiterをwake
3. kernel lock外でbackendをrelease/join
4. 全backend終了後に資源を再利用

#### 4.2.3 Script成功・error経路

`script.c` は成功時とerror時の両方でvariable tableを退避し、parser、lexer、
code、executorを初期化してからvariable tableを復元する。

現行の変数保持契約を維持したまま、重複するcleanupを共通化できる。
戻り値の意味変更はこの整理に含めない。

#### 4.2.4 Parser依存関係

`par.c` のhelperはglobal `parse_error` に依存している。局所的な可読性改善は
可能だが、transaction化、rollback追加、parser context化は意味とAPIを
変え得るため、機械的整理から分離する。

#### 4.2.5 Header契約

Lock、参照数、fd所有権、backend releaseに関するheaderコメントと実装は
概ね一致している。今後は各契約を対応する試験名と結び付け、呼出し側が
lockを保持すべき内部APIを明確にする。

### 4.3 追加試験後に判断する事項

Baselineを破る新規の重大不具合は初期確認では見つかっていない。
ただし、次の経路は最小再現試験を追加してから安全性を判断する。

- 部分転送後にreaderがcloseした場合のwrite結果
- backend prepare途中失敗後のprocess状態と再利用
- wake/release競合時のwait list切離し
- 不正bytecodeで失敗した後のexecutor再利用
- parser失敗後に残るsymbol/variableと次回実行
- debug設定を有効にした両backendのbuild

## 5. UNIX V6/V7との対応確認

Historical UNIXの完全再現ではなく、pipeに必要な観測可能な意味だけを維持する。

| UNIX V6/V7機構 | MCUFW機構 |
|---|---|
| process fd table | `struct esh_fd_table` |
| global file table | 固定長`struct esh_file` pool |
| pipe inode | 固定長`struct esh_pipe_inode` pool |
| inode-backed pipe data | inode内256-byte ring buffer |
| `sleep`/`wakeup` wait channel | portable scheduler wait channel |
| process scheduling | coroutine/thread backend |
| signalによるbroken pipe通知 | `-ESH_EPIPE`戻り値 |
| system call trap | 直接C関数呼出し |

次の不変条件を維持する。

- writerが存在する空pipeのreadは待機する
- 最終writer close後、残データを読み切ってからEOFを返す
- reader不在のwriteはbroken pipeとして扱う
- 満杯pipeのwriteは空きができるまで待機する
- endpoint closeは待機中peerをwakeする
- 復帰後はpipe条件を再検査する
- allocation失敗時はfile、fd、inode所有権を逆順に戻す

## 6. Refactoring実施方針

次の順で、小さな差分に分けて進める。

### Step 1: 再現試験追加

- debug build
- 部分転送とpeer close
- backend準備失敗後の再利用
- waiter解放
- 不正bytecode後の再利用

### Step 2: 明白な不具合と文書不一致の修正

- `ffush`修正
- Windows coroutine stack容量記述の更新
- 実装と文書の明白な不一致修正

### Step 3: 終了処理と所有権の整理

- pipeline準備状態の明示
- fd close経路の共通化
- backend release経路の共通化
- rollback後の再利用確認

### Step 4: Executor検査処理の整理

- 命令幅判定の集約
- operand検査の整理
- process、stack、label境界検査の対応付け
- 実行switchとの重複削減

### Step 5: Parserとscriptの機械的整理

- `script.c` cleanup共通化
- 現行statusとvariable保持を維持
- parserの局所helper整理
- transaction化やcontext化は行わない

### Step 6: 回帰確認

- cortn/thread × O0/O2
- pipe/scheduler/coroutine厳格build
- parser、executor既存golden
- 引数付きbreak試験
- Windows対話入力
- resource pool回収
- debug build

## 7. 仕様・API変更として別途相談する事項

次の項目は無断で変更しない。

1. `esh_script_line()` の未完状態とcommand status `1`の分離
2. 部分write後のreader closeで部分byte数と`-ESH_EPIPE`のどちらを返すか
3. Parser error時のbytecode、symbol、variable rollback
4. Parser singletonのcontext化
5. `cat`でEOFとread errorを区別するか

これらは再現試験、互換性への影響、選択肢を整理した後、機械的整理とは
別の変更として判断する。

## 8. 現時点の結論

Milestone 1〜9 のbaselineは両backendと主要最適化設定で成功している。
Pipe core、scheduler、coroutineの厳格buildにもwarningはない。

最初の変更は、指定された実装Milestone 1〜9のコミットで新規追加した
ソース・headerを、現行`AGENTS.md`に基づいて整える。その後、仕様を変えない
再現試験、debug typo、文書不一致へ進み、既存のEOF、EPIPE、status、
I/O契約を維持したまま終了処理とexecutor検査を小さく整理する。

Milestone 10 は本レビューおよびrefactoring完了後も未着手として残す。

具体的な実行単位、依存関係、選別実行方法は
`docs/pipe_review_refactoring_plan.md` に記録する。
