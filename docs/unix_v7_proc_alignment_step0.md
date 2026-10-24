# UNIX V7 proc 整合 Step 0 設計決定

確定日: 2026-10-24
調査開始 HEAD: `81d3fab`
状態: Step 0 complete、実装は Step 1 以降

## 適用範囲

[引き継ぎ書](unix_v7_proc_alignment_handoff.md)の状態・寿命・異常の契約を確定する。
本書は移行後の仕様であり、現行コードが実装済みであることを示さない。
設計確定時の p_state/p_wait_chan/p_wait_next は Step 1 の改名対象である。
現在の実装状況は引き継ぎ書の Step 表と作業記録を参照する。
以下では移行後の名前 p_stat/p_wchan/p_link を使う。
p_xctx、p_ret、専用 user、固定容量 pool、協調的な実行方式を維持する。
本書作成では本体・試験コードを変更せず、実行試験も行っていない。

## 原典と現行コードから確認した事実

V7 の基準は `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/`。

| 出典 | 確認内容 |
|---|---|
| V7 h/proc.h | SSLEEP=1、SRUN=3、SIDL=4、SZOMB=5。異常専用状態なし |
| V7 sys/slp.c: sleep/wakeup/setrun | 待機対象は p_wchan、睡眠キューにも p_link を使用。起床で SRUN |
| V7 sys/slp.c: swtch | 実行中も SRUN。実行可能と実行中を p_stat で区別しない |
| V7 sys/sys1.c: exit/wait | exit が資源解放後 SZOMB と xp_xstat を設定し、wait が結果回収 |
| MCUFW sys/sys/sched.c | 待機登録、起床状態、backend 報告を統合。古い待機報告による上書きを抑止 |
| MCUFW sys/dev/windows/sched_thread.c | 実行 token と TLS を使用。停止時は待機からエラー復帰して join |
| MCUFW sys/dev/windows/sched_cortn.c、cortn.c | 内部 CORTN_* と固定 stack を使用。resume 前後で guard 検査 |
| MCUFW esh/exe.c | pipeline wrapper が main 戻り値を p_ret に保存し fd close。単一コマンドは直接実行 |
| MCUFW sys/test/sched/test_sched.c | yield 失敗後の正常 return、待機中 release、user 保持と再利用を検証 |

V7 の資源解放・親子関係・wait・MMU を導入せず、状態の意味だけを対応させる。
以下の異常・キャンセル契約は MCUFW 向けの設計決定であり、V7 の仕様ではない。

## 状態値と意味

Step 2 で `enum proc_state` を次の値へ移行する。Step 1 は旧状態分類を維持する。

| 名前 | 値 | 意味 |
|---|---|---|
| SSLEEP | 1 | 有効な実行コンテキストがイベントを待機している |
| SRUN | 3 | 実行準備が完了し、実行可能または実行中 |
| SIDL | 4 | 初期化済み。実行準備前、または実行しない管理専用 proc |
| SZOMB | 5 | entry が通常 return し、結果を公開済み。結果回収待ち |
| PROC_ERROR | 7 | 実行コンテキストの継続不能、または完了前の強制解放 |

0 は有効な初期化状態としない。2、6 は V7 の SWAIT、SSTOP のため転用しない。
未実装の SWAIT、SSTOP、空き proc table 管理は追加しない。
PROC_ERROR は MCUFW 拡張として名前を維持する。
正常な entry return は main の戻り値 0 だけを意味しない。
main が非ゼロや -SYS_EPIPE を返しても、実行機構が健全なら SZOMB となる。
汎用 scheduler entry は void 関数であり、main が存在しない場合もある。

SRUN と p_xctx の非 NULL だけで、対象が停止中か実行中かは判断できない。
実行権は backend が管理する。current は実際の worker 上でのみ照会する。
thread の TLS current を他 thread から照会して実行中判定をしない。
既存の executor が一つずつ実行権を渡す方式を維持する。

## 状態遷移と更新担当

共有 proc の状態・待機情報は kernel lock 内で更新する。
例外は、まだ公開していない proc/user の初期化であり、所有者が排他的に行う。
既存の lock 保持箇所から呼ぶ内部処理で二重 lock を取得しない。

| 契機 | 遷移 | 更新担当・条件 |
|---|---|---|
| proc_init | 未初期化/再利用前 → SIDL | 所有者。旧 backend 解放と fd close 完了後、排他的に初期化 |
| prepare/bind 成功 | SIDL → SRUN | proc_bind_backend が p_xctx とともに公開。backend の起動はまだ許可しない |
| prepare 不足・引数拒否 | 変更なし | sched_prepare/proc_bind_backend。失敗した一時 backend を回収し SIDL を保持 |
| backend 実行・再開 | SRUN → SRUN | sched_resume。実行権を渡す前に状態・backend を検査 |
| 直接コマンド開始 | SIDL → SRUN | exe_execute_commands。current 設定と整合させてから main を呼ぶ |
| 待機登録 | SRUN → SSLEEP | sched_sleep。理由検査後、p_wchan/p_link と一緒に公開 |
| 起床成功 | SSLEEP → SRUN | sched_wakeup。待機情報を解除し backend wake 成功後に公開 |
| 待機から正常復帰 | SRUN → SRUN | sched_sleep。既存の正常起床状態を維持 |
| 待機試行の回復可能な失敗 | SSLEEP/SRUN → SRUN | sched_sleep。待機情報を除去し負の結果を呼出元へ返す |
| 通常 entry return | SRUN → SZOMB | backend 完了処理と portable scheduler の報告統合 |
| 直接コマンド完了 | SRUN → SZOMB | exe_execute_commands。p_ret 保存と fd close 後に公開 |
| wake/resume 不能など | 非異常状態 → PROC_ERROR | 検出した scheduler/backend。未解除の待機情報も除去 |
| 未完了 backend の release | SRUN/SSLEEP → PROC_ERROR | sched_release。p_xctx 切離しと同じ lock 区間で公開 |
| 完了 backend の release | SZOMB → SZOMB | sched_release。通常完了結果を保持 |
| 異常 backend の release | PROC_ERROR → PROC_ERROR | sched_release。異常を保持 |

SIDL の管理用 proc は実行キュー・完了数に含めず、fd 操作後も SIDL のままとする。
単一コマンドの SRUN は p_xctx == NULL のままでよいが、sched_resume は拒否する。
current 設定だけを行う sched_set_current_proc は状態を自動変更しない。
単一コマンドの前後で元の current を必ず復元する。

引数違反、二重 prepare、非 SRUN の resume 等は、操作拒否として負値を返す。
拒否しただけで健全な対象 proc を PROC_ERROR にしない。
SLEEP 理由の拒否は待機登録前に行い、既存状態・キューを変更しない。
prepare の失敗で管理側に必要な fd/user を消去しない。

## 待機登録・起床・報告の優先規則

待機対象の条件検査、p_wchan 設定、p_link 登録、SSLEEP 公開は同じ lock 内で行う。
backend yield は lock 保持で入り、待機時に解放し、復帰時に再取得する。
wakeup は channel の全待機者を外し、各 p_wchan/p_link を NULL にする。
p_link は末尾なら NULL になり得るため、NULL だけで待機中か判定しない。
待機中かどうかの判定には状態と p_wchan を使い、復帰後は I/O 条件を再検査する。

実際の backend 切替が終わる前後に wakeup が入っても、公開状態を巻き戻さない。
sched_backend_resume の戻り値は実行中に起きたことの報告であり、無条件の代入値ではない。
戻り値の型は enum proc_state を維持し、新しい状態や戻り値専用型を追加しない。

| backend 報告 | lock 下の共有状態 | 統合規則 |
|---|---|---|
| SSLEEP | SSLEEP | 待機継続。p_wchan が有効であることを確認 |
| SSLEEP | SRUN | wakeup 済みとして SRUN を保持し、待機を再登録しない |
| SSLEEP/SRUN/SZOMB | PROC_ERROR | PROC_ERROR を保持。正常結果や再開可能状態に戻さない |
| SRUN | SRUN | 実行可能状態を保持 |
| SZOMB | SRUN/SZOMB | 待機登録なしを確認し、通常完了として SZOMB を公開 |
| PROC_ERROR | 任意の有効状態 | fatal 報告を優先し PROC_ERROR を公開 |
| その他の不整合 | 任意 | 再開不能として PROC_ERROR と負の結果を返す |

resume から SIDL、未定義値、解除されていない待機を伴う完了などが報告された場合は、
そのまま公開しない。待機情報を解除して異常扱いとする。
SZOMB 公開後に coroutine の resume 後 guard 検査が失敗する場合も、fatal が優先する。
通常完了の報告で既存 PROC_ERROR を上書きしない。
再初期化以外に PROC_ERROR → SRUN/SIDL/SZOMB の遷移を設けない。

sched_resume は健全な待機・再開・通常完了なら 0、fatal 検出時は -SYS_EINVAL を返す。
sched_sleep 内の操作失敗を entry が処理して通常 return した場合は、
sched_resume 自体は成功し得る。API の負値と proc の terminal 状態を混同しない。

## 異常・キャンセルの分類

| 分類 | 例 | 状態と結果 |
|---|---|---|
| コマンド/I/O の結果 | EOF、EPIPE、コマンドの非ゼロ return | I/O と main の既存契約。通常 return 後は SZOMB、p_ret を採用 |
| 操作の回復可能な失敗 | 安全に呼出元へ戻れる yield 失敗、試験用 yield 失敗注入 | 待機登録を解除して SRUN。sched_sleep は負値を返す |
| backend 継続不能 | stack guard 破損、再開不能、wake 不能、報告の不整合 | PROC_ERROR。再開禁止、owner が cleanup/release |
| 完了前の強制解放 | 開始前、待機中、起床後に release | PROC_ERROR。停止/unwind または stack 破棄後に解放 |

回復可能とは、元の実行 stack で安全に復帰でき、backend の継続可能性が保たれていること。
戻り値が -SYS_EINVAL であることだけで回復可能か判断しない。
backend が継続不能を検出した場合は PROC_ERROR を明示してから返す。
sched_sleep は既存 PROC_ERROR または release による p_xctx 切離しを見たら、
SRUN に戻さず失敗を返す。通常の失敗注入は fatal を設定しない。

現行 sched_sleep は yield 失敗時に一律 PROC_ERROR を設定するが、
現行 test_yield_failure_cleanup はその後の正常 entry return を確認している。
移行後は回復可能な失敗を SRUN に戻すことで、この正常 return の動作を維持する。
この修正は Step 2 に含め、PROC_ERROR を一時的なエラー表示として使わない。
thread の完了処理と coroutine の報告統合には、fatal/cancel を消さない条件が必要になる。

## release と寿命

owner は対象 worker に実行権がない時に、kernel lock を保持せず sched_release を呼ぶ。
worker 自身からの self release、同じ proc の同時 resume/release/再初期化は対象外。
pipeline での fd close と wakeup は既存の cleanup 順序を維持する。
sched_release 自体は fd close や user 初期化を代行しない。

release の順序を次に固定する。

1. kernel lock を取得する
2. backend があり通常完了前なら PROC_ERROR とし、既存 PROC_ERROR は保持する
3. p_wchan/p_link を除去し、backend をローカルに退避して p_xctx を NULL にする
4. kernel lock を解放する
5. backend を解放する。thread は停止通知後の unwind と join、handle close を待つ
6. release が戻った後にのみ、owner が proc/user を破棄または再初期化する

状態と切離しは同じ lock 区間で公開する。状態 0 や SIDL に戻して解放を表現しない。
既に SZOMB の backend を通常解放した場合は SZOMB と p_ret を保持する。
p_xctx が最初から NULL の release は状態を変えない no-op とし、繰返し呼出しも同様。
従って管理用 SIDL と直接実行後 SZOMB はそのまま残る。
ただし別の release が進行中の NULL を no-op で観測し、解放完了と判断してはならない。
owner の逐次呼出しを前提にする。

release 中の thread は古い slot と user を使って unwind できる。
待機失敗の SYS_EINVAL と部分転送状態は元の user で確定する既存動作を維持する。
entry がその後 return しても PROC_ERROR を SZOMB に変えない。
coroutine は待機後の処理を実行せず stack を破棄するため、結果は thread と異なり得る。
両者で揃えるのは取消状態 PROC_ERROR、待機解除、資源寿命、再利用条件である。

release 中は p_addr、u_procp、user の格納領域を保持する。
p_xctx == NULL、SZOMB、PROC_ERROR のいずれも単独では join 完了を意味しない。
native thread の最終状態公開後も終了処理が残り得る。
処理完了結果の参照と、proc/user/stack 資源の再利用を別の時点として扱う。
管理用 proc と直接実行の proc は、同期処理と fd close が完了した後に所有者が破棄する。
破棄した coroutine stack を指す u_base は参照しない。

## p_ret とエラー領域の契約

proc_init は p_ret を 0 にするが、これは未実行の初期値であり成功結果ではない。
main を呼ぶ executor wrapper が、return 直後に戻り値を一度保存する。
単一コマンドも同様とし、fd close 後に完了を公開する。
backend の generic void entry は p_ret を書き換えない。
したがって main がない scheduler 試験では SZOMB でも p_ret をコマンド結果と解釈しない。

main の結果の書込みは worker/直接実行の呼出元だけが行い、SZOMB 公開より先に完了する。
結果の読取りは kernel lock または backend の完了報告による同期後に行う。
owner が逐次実行する既存 executor の報告同期を維持し、並列 polling API は追加しない。
全段が健全に SZOMB となったときだけ、pipeline は最終段の p_ret を返す。
先行段の負の戻り値は各段へ保持し、最終段の結果を置き換えない。

PROC_ERROR の p_ret は、未設定・既存値・unwind 中の main return 値があり得る。
値を診断用に保持してもよいが、通常完了したコマンド結果として採用しない。
取消時に sentinel を上書きせず、追加の result-valid flag も導入しない。
main を実行したか、正常完了したかは executor の経路と terminal 状態で判断する。

u_error と u_r は既存の sys API/I/O 契約に従う。
kernel 内部の sched_* は caller/host user の error/result を無条件に書き換えない。
pipe_sleep などの呼出元が sched_sleep の負値を自分の u_error へ変換する。
fatal/cancel を p_ret や host の u_error へ書いて通知する設計にはしない。
pipeline の prepare/fatal/deadlock 等の戻り値は、既存 executor の公開契約を維持する。

## 後続 Step の実装・検証条件

Step 1 はメンバ名と header 配置のみを変え、状態分類・エラー動作は維持する。
Step 2 で状態統合と、回復可能な失敗の SRUN 復帰、fatal の保持をまとめて実装する。
release の PROC_ERROR 公開も、thread 完了処理と同じ変更単位で導入する。
Step 3 で直接実行・管理用 proc・release の全経路を監査する。
Step 4 で既存試験を更新・補強し、Step 5 で全回帰する。

後続の試験では最低限、次の違いを明示的に検証する。

- yield 失敗注入後は SRUN、entry の通常 return 後は SZOMB
- wake 成立後に古い SSLEEP 報告を統合しても SRUN を保持
- fatal 設定後の entry return/完了報告でも PROC_ERROR を保持
- coroutine の完了後 guard 異常では通常完了結果より fatal を優先
- 開始前/待機中/起床後 release は両 backend とも PROC_ERROR
- SZOMB の通常 release と、backend なし release は状態・p_ret を保持
- thread の unwind 中は user を保持し、release 完了後にだけ再利用
- 単一コマンドは backend なし SRUN → SZOMB、main 戻り値を p_ret に保存
- main の非ゼロ・負値は scheduler fatal と区別し、pipeline 最終段結果を維持

追加の proc メンバ、新しい public API、priority/signal、並列 scheduler は不要。
実装で矛盾が見つかった場合は、原因と契約への影響を記録して本書を更新する。
既存の状態値だけを置換して、この契約を実装済みと扱わない。
