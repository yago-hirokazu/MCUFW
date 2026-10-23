# UNIX V7 user 構造体導入 引き継ぎ書

## 現行コードとの差分（2026-10-24）

Step 0 の確定事項は [Step 0 設計決定](unix_v7_user_context_step0.md) を参照する。
後続実装では同文書の API・型・エラー契約と、下記の現行実装を基準とする。
以下の全面移行計画に残る「設計のみ」「全Step pending」「グローバルu」は
2026-10-20 作成時点の記述であり、現在の状態を示さない。

| 項目 | 現在の実装 |
|---|---|
| user領域 | u_error、u_r、u_procp、u_ofile[NOFILE]、u_base/u_count/u_offset |
| procからの参照 | struct user *p_addrを実装済み |
| 初期化 | proc_init(proc, user)が両者を初期化してp_addrとu_procpを設定 |
| 所有者 | exe_proc.p_user、または呼出元の固定・局所領域 |
| current参照 | current_user()とuマクロを実装済み |
| currentなし | coroutineはbackend base user、threadはTLS base user |
| 単一command | currentを設定して直接実行し、fd close後に元へ戻す |
| 寿命 | backendの停止・unwind・join完了までuserを保持 |
| buffer error | geterror()が正のerrorをu_errorへ設定。成功時は以前の値を保持 |
| fd table | proc.p_addr->u_ofile[]へ移行 |
| inode I/O | readi/writei/iomoveがcurrent userの転送状態を更新 |
| pipe I/O | readp/writep/plockがcurrent userを使い、同じ状態でinode I/O・待機 |
| 公開sys API | error/resultとread/write状態を準備し、残量から結果を計算。errorを優先 |
| file/pipe作成 | current APIと管理APIを分離。結果はu_r、失敗はu_error |
| 後続の検証 | Step 6～7でerror・寿命・資源再利用と全回帰を検証 |

src/core/sys/test/schedにはsleep/wakeup前後とhost/worker間のerror分離、
thread backendのbase user分離を確認する試験がある。
src/core/esh/test/pipeline_execには単一commandのcurrent設定・復元・user再初期化の
試験がある。Step 2～5の変更内容と試験結果は末尾の作業記録を参照する。

後続作業はStep 6のerror・寿命・資源再利用の整合確認から着手する。導入済みの基盤を再実装しない。
[pipe core](pipe_core.md)、[scheduler](pipe_sched.md)、[RAM I/O](ram_block_device.md)参照。

作成日: 2026-10-20
対象: `C:\usr\src\MCUFW`

各コルーチン・スレッドに専用の `struct user` を割り当て、実行単位の切替に
合わせて参照先も切り替える。さらに file・inode・pipe の処理を、可能な限り
UNIX V7 と同じ user 経由の状態受渡しへ移行する。

本書作成時点では設計検討のみ。user 導入に伴うコード変更・試験は未実施。
別タスクは本書と最新のソースを確認して実装を進める。本書の作成依頼自体は、
ソースコード変更やコミットの指示ではない。

## 確定している方針

1. UNIX V7 オリジナルの構造、関数名、責務、処理順序を可能な限り踏襲する。
2. 各実行単位に専用の user 実体を割り当てる。
3. `struct proc` から user への参照は、ユーザー指定により `p_addr` で管理する。
4. MCUFW では `struct user *p_addr` として直接参照する設計とする。
5. 前案の `struct user p_user` を proc に埋め込む方式は採用しない。
6. user 全体を切替のたびにコピーせず、現在の proc の `p_addr` を参照する。
7. 最終的な変更範囲は `u_error` の個別化だけでなく file・inode・pipe を含む。
8. Windows で先に成立させ、portable core と機種依存 backend を分離する。
9. MMU、trap、スワップ、物理ストレージへのアクセスは導入しない。
10. file・inode・buffer の共有と kernel lock による保護を維持する。

V7 の `p_addr` は、常駐時には user 領域を先頭とするコアイメージの位置を
クリック単位で表し、スワップアウト中はスワップ領域の位置を表す。
MCUFW の直接ポインタは、この参照の役割を MMU のない環境へ対応させたもの。
V7 の数値表現やコアイメージ全体の管理を再現するものではない。

## 最初に確認する資料

- [AGENTS.md](../AGENTS.md)
- [Linux kernel coding style 要約](coding_style_summary_linux_kernel.md)
- [scheduler](pipe_sched.md)
- [Windows thread backend](pipe_thread.md)
- [pipe API](pipe_api.md)
- [executor](pipe_executor.md)
- [既存の汎用 sys 移行](unix_v7_generic_sys_migration_handoff.md)
- [直近の bio 変更要求](request_2026-10-20.md)

原典の調査には `ancient-unix` スキルを使用する。下記の同梱 V7 ソースを優先し、
必要な関数の呼出し元、待機、復帰、エラー、解放まで確認する。

基準ディレクトリ:
`.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/`

| 原典 | 確認する内容 |
|---|---|
| `h/user.h`、`h/proc.h` | user と proc の役割、`p_addr`、user メンバ |
| `sys/slp.c`、`conf/mch.s` | `swtch()`、`resume()`、user 領域とスタックの切替 |
| `sys/fio.c` | `getf()`、`ufalloc()`、`falloc()`、`closef()` |
| `sys/sys2.c` | `rdwr()`、`close()`、I/O 状態と呼出し結果 |
| `sys/pipe.c` | pipe 作成、`readp()`、`writep()`、`plock()`、`prele()` |
| `sys/rdwri.c` | `readi()`、`writei()`、`iomove()` |
| `sys/alloc.c`、`sys/iget.c` | inode 確保・解放とエラー伝播 |
| `dev/bio.c` | `geterror()`、buffer エラーと user エラー |

## 現在の実装と既存変更

本書作成時の実装は次の状態。後続タスク開始時に再確認する。

- `src/core/sys/h/user.h` は `int u_error` と `extern struct user u` を定義。
- `src/core/sys/dev/bio.c` がグローバル `struct user u` を定義。
- `struct proc` は `p_ofile[NOFILE]` を保持し、`p_addr` はまだない。
- `fio.c` は `struct proc *` を引数として fd テーブルを操作する。
- `readi()` / `writei()` は offset、buffer、count を引数で受け取る。
- `readp()` / `writep()` は proc、buffer、count を引数で受け取る。
- `sys_read()` / `sys_write()` は転送量または負の `SYS_E*` を返す。
- coroutine backend は単一の current ポインタを保持。
- Windows thread backend は `_Thread_local` の current を保持。
- thread backend も executor が実行権を順番に渡す方式。独立した並列実行は別課題。
- 単一コマンドは executor が直接呼ぶため、current の設定を追加する必要がある。
- パイプ用 inode は一つの buffer を保持する。V7 の全ブロック管理は未導入。

作成時点で次のファイルは既存の未コミット変更を含む。

```text
src/core/esh/test/pipeline_exec/test_pipeline_exec.c
src/core/sys/dev/bio.c
src/core/sys/h/buf.h
src/core/sys/test/bio/test_bio.c
src/core/sys/test/sched/test_sched.c
```

`src/core/sys/h/user.h` は未追跡。ほかにも request 文書、cscope 生成物、STM32 tree、
Windows DLL などの未追跡項目がある。開始時に `git status --short` と対象の diff を
確認し、既存作業を上書き・破棄・一括追加しない。既存変更が今回の導入結果だと
誤認しない。

## user と実行単位の対応

```text
current_proc()
    |
    v
struct proc
    +-- p_state / wait state / p_backend
    +-- p_addr ----------------> struct user
                                   +-- u_error
                                   +-- u_ofile[] --> file --> inode --> buffer
                                   +-- u_base / u_count / u_offset
                                   +-- u_r
                                   +-- u_procp --> owning proc
```

`current_user()` は current の `p_addr` を返す構成を推奨する。
V7 に近い `u.u_error` 表記を維持するため、`u` を
`(*current_user())` とする互換マクロを検討する。グローバルの user 実体は廃止する。
current と current user のポインタを独立に更新する二重管理は避ける。

user 実体の固定領域は、proc の所有者に対応させて確保する案を優先する。
backend slot だけに置くと、backend 準備前の fd 接続や backend を持たない一時的な
`owner` proc を扱えないため、これらの寿命も考慮する。

必要な不変条件:

- 有効な実行中 proc の `p_addr` は有効な専用 user を指す。
- `u_procp` を導入する場合、`proc->p_addr->u_procp == proc` を維持する。
- 同じ user を異なる実行単位が同時に使用しない。
- user は初期化から fd 設定、実行、待機、終了処理まで生存する。
- thread の停止・unwind・join が完了する前に user を解放・再利用しない。
- sleep・wakeup では user を初期化しない。
- coroutine の再開先と current の参照先は、user に触れる前に一致する。
- thread の current は TLS または移植先のタスクローカル機構で管理する。
- fd の解放順序と worker 停止順序は現行の協調 unwind 契約に合わせて確認する。

## user に導入する状態

| メンバ | 役割 |
|---|---|
| `u_error` | 下位処理から上位処理へのエラー伝播 |
| `u_ofile[NOFILE]` | `proc.p_ofile[]` から移す fd テーブル |
| `u_base` | 今回の I/O で次に転送する呼出し元バッファ位置 |
| `u_count` | 現在扱っている転送の残りバイト数 |
| `u_offset` | 現在の inode I/O 位置 |
| `u_r.r_val1`、`r_val2` | 転送量、確保した fd、pipe の両端 fd |
| `u_procp` | 所有する proc への逆参照 |

型は現行 MCUFW の容量・戻り値・ポインタ幅を確認して選定する。V7 の 16 bit 型を
そのまま使わない。count の範囲と外部の `ptrdiff_t` 戻り値との整合、offset の符号、
write 引数の const の扱いを明確にする。新しい typedef は導入しない。

`u_pofile[]` は fd ごとのフラグが必要になるまで保留可能。
MMU 関連情報、trap 引数、signal、資格情報、ディレクトリ探索用状態は今回の
pipe 動作に必要な範囲へ勝手に追加しない。`p_size` も今回の user 参照だけのために
追加しない。

## 関数と責務の移行

| 現行 | 移行後の方向 |
|---|---|
| `getf(proc, fd)` | `getf(fd)` が `u_ofile[]` を参照し、失敗時は `u_error` を設定 |
| `ufalloc(proc)` | `ufalloc()` が fd を返し、`u_r.r_val1` にも設定 |
| `falloc(proc, &fd)` | `falloc()` が file を返し、fd は `u_r`、失敗は `NULL` と `u_error` |
| `readi(ip, &offset, buf, count)` | `void readi(struct inode *ip)`、user を参照・更新 |
| `writei(ip, &offset, buf, count)` | `void writei(struct inode *ip)`、user を参照・更新 |
| `readp(proc, fp, buf, count)` | `void readp(struct file *fp)`、同じ user で inode I/O |
| `writep(proc, fp, buf, count)` | `void writep(struct file *fp)`、同じ user とスタックで待機継続 |
| `pipe_for_proc(proc, fd)` | current user に pipe を構築する V7 型内部処理へ移行 |
| `sys_read/write/pipe/close` | 現在の外部呼出し形式を維持し、内部の user 状態を変換 |

上表は最終形の設計案。原典で戻り値のある関数は、その役割を維持する。
原典で結果を返さない処理は、現代 C では明示的な `void` とする。
外部 API と V7 型内部処理を同名で衝突させず、命名は Step 0 で決める。

`iomove()` は buffer と `u_base` 間をコピーし、実際に転送した分だけ
`u_base`、`u_count`、`u_offset` を更新する。MMU による copyin/copyout は導入しない。
file の `f_offset` は永続的な open file の位置、`u_offset` は今回の I/O の位置。
これらを一つに統合しない。

executor が別 proc の fd を接続・解放する管理操作には、対象 user または proc を
明示する補助関数を残してよい。別 worker の fd を触るために current を一時的に
付け替えない。補助関数と current user を使う通常 I/O の責務を明記する。

## I/O とエラーの契約

- 外部 I/O の開始境界で `u_error` と必要な結果領域を準備する。
- 下位関数の成功時や sleep 復帰時に `u_error` を無条件クリアしない。
- `geterror()` が成功時に既存エラーを保持する性質を維持する。
- buffer のエラーは `b_error` に保持し、要求元が自身の `u_error` に取り込む。
- 将来の割込み・非同期完了から、その時点の current user を更新しない。
- 正常な転送量は要求量と残量の関係で計算し、EOF とエラーを区別する。
- V7 の `writep()` はローカル残量と区間ごとの `u_count` を併用する。
  大きな要求、途中失敗、reader close 時の残量を原典と照合する。
- V7 は書込量によって `PIPSIZ` を超える場合がある。MCUFW の固定 buffer では
  容量を超えないことを優先し、差分を明記する。
- SIGPIPE は既存の signal 基盤がないため導入せず、EPIPE で通知する方向。
- kernel lock と inode lock、wait 登録、unlock、sleep、再検査の契約を維持する。
- 同じ user に対する I/O の再入は、外側の I/O 状態を破壊する。
  通常経路で発生しないことを確認し、必要なら明示的な保存・復元を設計する。

## 実装開始時に決定する事項

次は提案段階であり、ユーザーの確定指示と区別する。通常の実装判断は資料と
現行動作に基づいて決定し、外部動作の変更が必要になる場合は根拠を記録する。

1. user の固定領域の配置、初期化・bind・再利用の API。
   既存 `proc_init()` は proc 全体をゼロ化するため、事前設定した `p_addr` が
   消えないよう初期化順序または引数を見直す。backend なし proc にも対応する。
2. current 未設定時の扱い。単一コマンドには current を設定し、起動処理や
   単体テストには明示的な基底コンテキストを設ける案を優先する。
   基底 user を複数 thread で共有せず、誤った未設定を黙って隠さない。
3. 外部 API での部分転送とエラーの優先順位。V7 と現行動作を比較し、
   完了済み量、`u_count`、EPIPE、戻り値の関係を具体例で決める。
4. ゼロ長 I/O と不正 fd、NULL buffer、backend なしの EAGAIN の互換性。
5. inode 不足と buffer 不足を区別するか、既存エラーへ対応させるか。
6. `u_r` の最小表現と内部 pipe 関数名、`iomove()` の方向指定。
7. 基底 user 取得を backend API に追加するか、別の明示的実行環境で処理するか。

## 変更対象ファイル

以下のパスは `src/core/` からの相対パス。

| 対象 | 変更内容 |
|---|---|
| `sys/h/user.h` | user 定義、current user、互換マクロ、宣言の C/C++ 整合 |
| `sys/h/sched.h`、`sys/sys/sched.c` | `p_addr`、初期化、所有関係、current の整合 |
| `sys/dev/bio.c`、`sys/h/buf.h` | グローバル u 廃止とエラー契約の説明 |
| `sys/sys/fio.c`、`sys/h/file.h` | `u_ofile`、fd/file 確保・参照・解放 |
| `sys/sys/sys2.c` | 外部 API の user 準備と結果変換 |
| `sys/sys/rdwri.c`、`sys/h/inode.h` | user I/O 状態、`iomove()`、関数宣言 |
| `sys/sys/pipe.c`、`sys/h/pipe.h` | pipe 作成、I/O、待機、状態受渡し |
| `sys/sys/alloc.c` | inode 確保失敗時のエラー伝播 |
| `esh/exe.c`、必要なら `esh/include/exe.h` | user 実体の配置、単一コマンド、fd 接続・後始末 |
| `sys/h/sched_backend.h`、`sys/dev/windows/sched_cortn.c`、`sched_thread.c` | 基底 context 方針などに応じて変更 |
| `sys/sys/iget.c` | 解放・エラー保持の整合確認。変更が必要な場合だけ編集 |

コルーチン切替アセンブリは user 参照の導入だけなら変更不要。
新しい本体 `.c` を追加する場合は、そのファイルを使う全 Makefile を更新する。

テストでは `sys/test/{bio,file,pipe,sched}`、`esh/test/{exe,pipe,pipeline_exec}`、
`sys/test/stub/sched_stub.c`、`sys/test/header/test_headers.c` と
`sys/test/header/run_test.sh` を中心に変更する。`p_ofile` や旧シグネチャの参照は
リポジトリ全体で検索する。

`sys/test/{bio,bdev,ramdisk,table}/Makefile` は現状 bio をリンクする一方で
scheduler をリンクしていない。current user のスタブ等を追加する必要がある。
既存 stub 利用先や esh 側 Makefile、Windows 本体も依存関係を確認する。

## 段階別の実装手順

状態は `pending`、`active`、`done` を用い、完了時に実装・試験・差分を追記する。
初回に全範囲を実装する指示があれば各段階を続けて進めてよい。
特定 Step のみを依頼された場合は、その範囲を実施する。

| Step | 作業 | 完了条件 | 状態 |
|---|---|---|---|
| 0 | 原典・既存差分を確認し、未確定の API・型・エラー契約を決定 | [設計決定](unix_v7_user_context_step0.md)に記録 | done |
| 1 | `p_addr` と専用 user、current user、基底 context を導入 | 既存コードに導入済み。Step 0 で現行実装と照合 | done |
| 2 | `p_ofile` を `u_ofile` へ移し、executor と管理 helper を更新 | 接続・close・再利用と参照数が正常 | done |
| 3 | `fio.c` を V7 型へ移行し、pipe 作成経路を適合 | fd/file 枯渇と途中失敗が漏れなく復旧 | done |
| 4 | `rdwri.c` と `iomove()` に user 状態を導入 | 転送量・残量・offset・容量境界が正常 | done |
| 5 | pipe 転送と `sys2.c` を V7 型へ移行 | 待機・部分転送・EOF・EPIPE が正常 | done |
| 6 | inode/bio エラー、終了・unwind・再利用を整合 | 他 user の状態破壊と資源漏れがない | pending |
| 7 | 全回帰・文書更新・生成物 clean | 両 backend の動作と差分を記録 | pending |

Step 間はビルド可能な状態を保つ。Step 3～5 は呼出し関係が連動するため、
必要なら一時的な変換 helper を使うか、まとまった変更単位にする。
最終段階で不要な互換経路を取り除く。

## 試験と完了条件

### 新規に確認する動作

- A/B に異なる `u_error` を設定し、交互の sleep・wakeup 後も保持される。
- `u_base`、`u_count`、`u_offset`、結果領域が別実行単位に上書きされない。
- 同じ fd 番号が別 user では別 file を指せる。
- fd の複製・受渡し・close が file/inode の参照数と整合する。
- buffer 容量より大きな転送で、データの欠落・重複・範囲外アクセスがない。
- 空で writer ありは待機、最終 writer close 後に空なら EOF。
- 満杯で待機、read または close で復帰し条件を再検査する。
- reader 不在時と部分転送後の reader close が決定した EPIPE 契約に従う。
- spurious wakeup、複数 waiter、待機登録直後の wakeup を処理できる。
- fd/file/inode/buffer/user 領域の枯渇、部分準備失敗で資源を回収できる。
- 単一コマンド、backend なし proc、起動処理、単体テストの user が有効。
- thread 終了・join 後の slot/user 再利用で前回の状態を引き継がない。
- ヘッダ単独、順序入替え、C/C++ の既存チェックが通る。

### 実行する既存試験

GCC は必ず MSYS2 MINGW64 shell 経由で実行する。PowerShell から
`C:\msys64\mingw64\bin\gcc.exe` を直接起動しない。

各ディレクトリの現行 `run_test.sh` を確認して実行する。

```text
src/core/sys/test/header
src/core/sys/test/table
src/core/sys/test/bdev
src/core/sys/test/ramdisk
src/core/sys/test/bio
src/core/sys/test/file
src/core/sys/test/pipe
src/core/sys/test/sched
src/core/esh/test/pipe
src/core/esh/test/pipeline_exec
src/core/esh/test/exe
```

scheduler 等の既存 script が提供する coroutine/thread、O0/O2、小容量構成を使う。
parser は `src/core/esh/test/par` で `bash ./run_test1.sh` と
`bash ./run_test2.sh` を実行する。Windows 本体は両 backend でビルドし、
`sam1`、`sam2 1 10 100`、`sam3 -a -b string` を確認する。

試験が終わったら対象の `OUTDIR`・`TARGET` に対応する `make clean` を実行し、
今回生成した `.d`、`.o`、`.exe` を残さない。既存の未追跡生成物を一括削除しない。
既存不具合と今回の退行は区別して記録し、無関係な修正を混ぜない。

## 今回に含めない変更

- `bmap()`、ディスク上の inode、superblock、mount、ディレクトリの完全移植。
- V7 のコアイメージ、MMU、スワップ、trap ABI、signal の再現。
- thread backend の並列 scheduler 化、複数 controller、nested pipeline。
- user 導入と無関係な buffer cache の全面改修。
- STM32 の機種依存 context switch の新規実装。

現行の inode が一つの buffer を保持する設計と、V7 のブロック管理との差は残る。
user 導入が完了してから必要な範囲を別途検討する。

## 後続タスクへの記録欄

各段階の完了時に、次を追記する。

- 完了した Step と変更ファイル。
- user の保存領域、初期化、bind、解放・再利用の具体的な契約。
- V7 と異なる動作、特に部分転送、容量、エラー、待機の扱い。
- 試験コマンド、backend、最適化・容量構成、結果、clean の結果。
- 既存変更との関係、残る問題、次に進める Step。

2026-10-22 Step 0: API・型・所有関係・エラー契約を
[Step 0 設計決定](unix_v7_user_context_step0.md)に確定。
Step 1 の基盤は既存コードに導入済みのため再実装しない。
Step 0 時点の次作業は Step 2 の `u_ofile[]` 移行。Step 0 ではソースコードを変更していない。

## Step 2 作業記録

実施日: 2026-10-23

`struct proc.p_ofile[]` を削除し、`struct user.u_ofile[NOFILE]` へ移した。
userには所有procへの`u_procp`も追加した。実体は従来どおりexecutorまたは呼出し元が
保持し、`p_addr`で参照する。動的確保やbackend slotへの移動は行わない。

`proc_init(proc, user)`はuser全体を初期化し、`p_addr`と`u_procp`を設定する。
standalone試験のscheduler stubにも同じ初期化を追加し、file/pipe試験の初期化は
stubの`proc_init()`を使う。base userはstatic/TLS初期化により`u_procp == NULL`、
fd tableも空のままである。sleep/wakeupやreleaseでuserを初期化しない。

`fio.c`の全fd操作と直接参照する試験を`p_addr->u_ofile[]`へ更新した。
未bindのprocはfd操作で失敗またはno-opとし、NULL userを参照しない。
APIの引数・戻り値、file/inodeの参照数、kernel lockとbackend寿命の契約は維持する。
executorとWindows stdioは既存fd helperを通して新しいtableを使用する。

変更対象はuser/scheduler header、`sys/sched.c`、`sys/fio.c`、scheduler stub、
file/pipe/header/scheduler試験、現行のpipe関連ガイドと本書。
V7型current APIへの変更や`u_r`追加はStep 3、inode I/O状態の移行はStep 4以降とする。

追加した確認:

- 別userの同じfd番号が異なるfileを指す
- file参照を別procへ渡し、元fdのclose後も受取先の参照が残る
- 管理操作の失敗・closeがcurrentや両userのerrorを変更しない
- 初期化・再利用でfd tableを空にし、所有procを設定する
- coroutine/threadのsleep/wakeup前後で所有procとfd tableを維持する
- currentなしのhost/native threadのu_procpはNULLとなる
- p_addrのないprocに対するfd操作で資源を確保せず失敗する

### Step 2 検証結果

MSYS2 MINGW64 shellで既存の`run_test.sh`を実行した。parserは同ディレクトリで
`run_test1.sh`と`run_test2.sh`を実行した。各試験後は対象の`make clean`を実施した。

| 試験 | 構成 | 結果 |
|---|---|---|
| sys header | C/C++、単独/順序入替え、O0/O2 | passed |
| sys file/pipe/bio/bdev/ramdisk/table | O0/O2 | passed |
| sys scheduler | cortn/thread、O0/O2、既定/NBUF=3 | passed |
| esh pipe | cortn/thread、O0/O2 | passed |
| pipeline executor | cortn/thread、O0/O2、既存30秒制限 | passed |
| esh exe | 既存10スクリプト | passed |
| parser | run_test1.sh、run_test2.sh | passed |
| Windows本体 | cortn/thread、sam1・sam2・sam3 | passed |

最初の並行実行でpipeline executorのcortn/O0は`all tests passed`出力後に
timeout終了した。既存の30秒制限を変更せず、他のビルドがない状態で全4構成を
単独再実行し、全構成の正常終了を確認した。ソースを変更せず再実行した結果、
タイムアウトは再現しなかった。

Windows本体はbackendごとに一時OUTDIR/TARGETを指定してビルドし、対話コンソールで
`sam1`、`sam2 1 10 100`、`sam3 -a -b string`を実行した。
両backendで`sample1 applet`、`arg1=1`、`arg2=10`、`arg3=0x00000064`、
`-a option`、`-b option with string`を確認し、Ctrl-Dで正常終了した。
既存のDLLは変更していない。

Step 2完了。次はStep 3の`u_r`追加とV7型file API・pipe作成への移行。
実装コミットはGit履歴を参照する。


## Step 3 作業記録

実施日: 2026-10-23

`struct user`に`u_r.r_val1/r_val2`を追加した。型はStep 0で確定した`ptrdiff_t`、
未使用のunionは導入しない。`proc_init()`の全体初期化で結果も0へ戻る。
userの所有者・寿命と`struct user *p_addr`による参照方法はStep 2から変更しない。

`getf(int fd)`、`ufalloc(void)`、`falloc(void)`をcurrent user用へ移行した。
失敗は正の`u_error`、fd結果は`u_r.r_val1`を使う。getf/fallocはpointer、ufallocは
fdまたは-1を返す。内部成功では以前のerrorを消さない。
空きfd検索とfile割当は共有し、別userの準備用に`falloc_for_user()`を宣言した。

明示proc指定のlookupは`getf_for_proc()`へ改名した。executor、Windows stdio、
管理用途の試験も更新した。fd install/close/close_allと管理用lookupは、対象・
呼出し元のerror/resultを変更しない。sys_read/writeはcurrent用getfを使うが、
転送処理と公開戻り値の全面移行はStep 5で行う。

`pipe_create()`を導入し、両端fdを`u_r`へ設定する。`pipe_for_proc()`は共通の
明示user割当処理を使い、実行前の対象userのerror/resultを準備する。対象が別userなら
currentを変更せず、呼出し元のerror/resultも保持する。inode、read file、write fileの
順に割り当て、途中失敗では今回確保した資源だけを戻す。出力fd配列は成功時だけ更新する。

`sys_pipe()`は入口でcurrent userのerror/resultを初期化し、NULL出力先または
current procなしをEINVALとする。pipe_createのerrorを負の公開戻り値へ変換する。
`ialloc()`は失敗時current userへENFILEを設定する。別user準備用`ialloc_error()`と
同じinode確保ロジックを共有する。fd枯渇はEMFILE、file/inode/buffer枯渇はENFILE。

追加試験はcurrent APIの結果・sticky error、管理操作のerror/result保持、
別userへのpipe作成と失敗時の呼出し元保持、fd配列の成功時のみの更新、
片端確保後のrollback、u_r初期化とsleep/wakeup前後の独立性を確認する。

`u_base/u_count/u_offset`、iomove、inode I/Oの引数変更はStep 4、
readp/writep/plock/sys2の一括移行はStep 5の範囲とし、今回は導入していない。


### Step 3 検証結果

MSYS2 MINGW64 shellから実行し、各試験後は対象の`make clean`を実行した。

| 試験 | 構成 | 結果 |
|---|---|---|
| sys header | C/C++、単独/順序入替え、O0/O2 | passed |
| sys file/pipe | O0/O2、資源枯渇・rollbackを含む | passed |
| sys scheduler | cortn/thread、O0/O2、既定/NBUF=3 | passed |
| esh pipe | cortn/thread、O0/O2 | passed |
| pipeline executor | cortn/thread、O0/O2、既存30秒制限 | passed |
| esh exe | 既存10スクリプト | passed |
| parser | run_test1.sh、run_test2.sh | passed |
| Windows本体 | 既定cortn、sam1・sam2・sam3 | passed |

Windows本体は一時OUTDIR/TARGETでビルドし、対話コンソールで`sam1`、
`sam2 1 10 100`、`sam3 -a -b string`を実行した。`sample1 applet`、
`arg1=1`、`arg2=10`、`arg3=0x00000064`、`-a option`、
`-b option with string`を確認した。


Step 3完了。次はStep 4のu_base/u_count/u_offsetとinode I/O移行。
試験用の一時スクリプトを削除し、生成したd/o/exeの清掃と`git diff --check`を確認した。


## Step 4 作業記録

実施日: 2026-10-23

`struct user`に`unsigned char *u_base`、`size_t u_count`、`ptrdiff_t u_offset`を
追加した。所有者・寿命、`struct user *p_addr`による参照とcurrent選択は変更しない。
`proc_init()`の全体初期化で転送状態もゼロに戻る。

V7の`sys/rdwri.c`を基準に`readi(ip)`、`writei(ip)`をvoidのcurrent用APIへ移行した。
コピーは新設`iomove(cp, count, flag)`へ集約した。B_READ/B_WRITEの値を維持し、
同一アドレス空間のmemcpyで転送する。成功時はbase/offsetを転送量だけ進め、
countを同量減らす。u_rは変更せず、成功時も以前のerrorを消さない。

固定容量内に転送を制限し、コピー前に負/範囲外offset、異常inode size、NULL、
方向、残量、加算overflowを検査する。PIPSIZがPTRDIFF_MAX以下であることを
コンパイル時に確認する。正のinode要求とiomove残量はPTRDIFF_MAX以下に制限する。
不正時はEINVALとして転送状態とpayloadを変更しない。ゼロ長はNULLでも演算しない。
callerがcp/countの有効領域を保証する契約を維持し、未知の領域長は推測しない。

pipe I/Oとsys2の全面移行はStep 5へ残す。Step 4限定のstatic helper
`pipe_inode_io()`でcurrentのbase/count/offset/errorを保存し、inode I/Oを実行して
旧形式の転送量/負のerrorとoffsetへ変換した後、保存値を復元する。currentを変更せず、
user全体のコピーや公開sys APIへの再入は行わない。このhelperはStep 5で削除する。
readp/writepの引数、pipe_io_status、待機と部分転送の処理は維持する。
公開sys_read/writeの全要求残量・u_r・error変換と一律の入口検証もStep 5で行う。

単体試験はinode I/Oの容量・EOF・部分転送・sticky error、iomoveの方向と
読取専用source、guard、無効入力・overflow時の無変更を確認する。
scheduler試験は途中まで進めた転送状態のsleep/wakeup前後の保持、hostとの分離、
初期化・再利用を確認する。pipe bridgeは成功・失敗時の状態復元も検証する。


### Step 4 検証結果

MSYS2 MINGW64 shellから既存のrun_test.shを実行した。parserは同ディレクトリで
run_test1.shとrun_test2.shを実行した。終了時のmake cleanも正常終了した。

| 試験 | 構成 | 結果 |
|---|---|---|
| sys header | C/C++、単独/順序入替え、O0/O2 | passed |
| sys file/pipe | O0/O2、inode/I/O境界・状態保持を含む | passed |
| sys scheduler | cortn/thread、O0/O2、既定/NBUF=3 | passed |
| esh pipe | cortn/thread、O0/O2 | passed |
| pipeline executor | cortn/thread、O0/O2、既存30秒制限 | passed |
| esh exe | 既存10スクリプト | passed |
| parser | run_test1.sh、run_test2.sh | passed |
| Windows本体 | cortn/thread、sam1・sam2・sam3 | passed |

Windows本体は一時OUTDIR/TARGETでビルドし、対話コンソールで`sam1`、
`sam2 1 10 100`、`sam3 -a -b string`を実行した。両backendで
`sample1 applet`、`arg1=1`、`arg2=10`、`arg3=0x00000064`、
`-a option`、`-b option with string`を確認し、Ctrl-Dで正常終了した。
両backendのmake cleanも正常終了した。

Step 4完了。次はStep 5のreadp/writep/plock/sys2の一括移行と、一時bridgeの削除。
一時スクリプトを削除し、生成したd/o/exeの清掃と`git diff --check`を確認した。


## Step 5 作業記録

実施日: 2026-10-23

V7のsys/pipe.cとsys/sys2.cを基準にreadp/writep/plockをvoidのcurrent用APIへ
移行した。u_base/u_count/u_offsetをinode I/Oへ直接引き継ぎ、同じuserとstackで
待機・復帰する。procからの参照は従来どおり`struct user *p_addr`を維持する。
`pipe_io_status`、PIPE_IO_*、一回転送用helperとStep 4の`pipe_inode_io`を削除した。

writepはV7型のローカル残量と区間転送を使い、各区間を空き容量以下に制限する。
実際に書いた量だけ残量を減らし、未着手分を含むu_countを復元してからlockを解放する。
baseはコピー済み量だけ進め、offsetは現在のinode位置を表す。途中エラーでも
転送済み状態は戻さない。backendなしで部分転送後にfullとなる短い成功だけ維持し、
lock競合、sleep失敗、EPIPEでの短い成功には一般化しない。

plockは失敗を正のu_errorへ記録する。callerは失敗時にpreleを呼ばない。
pipeの空/fullではinode lockを解放して待機し、復帰後に再検査する。
backendなしの待機はEAGAIN、sched_sleepの失敗は従来のエラーへ変換する。

sys2の共通rdwrは入口でerror/resultとbase/count/offsetを初期化する。
count 0なら他の引数を検査せず成功。正のcountではNULL/PTRDIFF_MAX、fd/方向の順に
検査する。終了時は要求量-u_countをu_r.r_val1へ設定し、エラーがあれば負の戻り値を
優先する。sys_closeもerror/resultを準備する。公開シグネチャは維持する。

pipe/scheduler単体試験は実際のsys_read/write/closeを使うように更新し、Makefileへ
sys2を追加した。esh側のscheduler試験入口も同じソースを使うためリンクを更新した。
mock sleepでpeer readを行う試験は公開sys APIへ再入せず、呼出し元I/O状態を保持する。
旧bridgeを期待する試験は、公開API終了時の残量・結果・error確認へ更新した。

追加試験はゼロ長/巨大要求/currentなし、backendなしの短いwrite、EOF、
部分転送後のsleep失敗・lock競合、他者のlockを解放しないことを確認する。
両backendの試験ではwrite中の残量とbase、全量転送後の結果、および部分転送後の
reader closeでEPIPEと転送済み量を返すことも確認する。

### Step 5 検証結果

検証完了日: 2026-10-24

MSYS2 MINGW64 shellから試験を実行した。途中で実行セッションが終了したため、
完了結果のないpipeline executor以降の試験とWindows本体を再開した。
試験の制限時間と判定条件は変更していない。

| 試験 | 構成 | 結果 |
|---|---|---|
| sys header | C/C++、単独/順序入替え、O0/O2 | passed |
| sys file/pipe | O0/O2、部分エラー時の残量・lock保持を含む | passed |
| sys scheduler | cortn/thread、O0/O2、既定/NBUF=3 | passed |
| esh pipe | cortn/thread、O0/O2 | passed |
| pipeline executor | cortn/thread、O0/O2、既存30秒制限 | passed |
| esh exe | 既存10スクリプト | passed |
| parser | run_test1.sh、run_test2.sh | passed |
| Windows本体 | cortn/thread、sam1・sam2・sam3 | passed |

Windows本体は一時OUTDIR/TARGETでビルドし、両backendの対話コンソールで
`sam1`、`sam2 1 10 100`、`sam3 -a -b string`を実行した。
`sample1 applet`、`arg1=1`、`arg2=10`、`arg3=0x00000064`、
`-a option`、`-b option with string`を確認し、Ctrl-Dで正常終了した。

Step 5完了。次はStep 6のinode/bioエラーと終了・unwind・再利用の整合確認。
各試験のmake clean後、一時スクリプトを削除した。生成したd/o/exeの残存なし、
旧pipe statusと一時bridgeのソース残存なし、`git diff --check`正常を確認した。
