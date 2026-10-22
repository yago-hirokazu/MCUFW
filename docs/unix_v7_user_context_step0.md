# UNIX V7 user 構造体導入 Step 0 設計決定

確定日: 2026-10-22

基準 HEAD: `5676189`

対象: [user 構造体導入 引き継ぎ書](unix_v7_user_context_handoff.md)

各実行単位の user を `proc.p_addr` で参照する既存基盤を維持し、fd テーブルと
I/O 状態を user に移す。内部処理は V7 の関数・状態受渡しに近づけ、公開 sys API
の呼出し形式と既存の正常動作を維持する。Step 0 の成果は本設計決定であり、
本体・テストのコード変更は Step 2 以降で行う。

## 現行実装の基準

開始時の追跡ファイルに未コミット差分はなかった。未追跡の cscope ファイル、
STM32 tree、Windows DLL は既存物として変更対象外とする。

以下は導入済みのため再実装しない。

- `struct proc` の `struct user *p_addr`
- `proc_init(struct proc *proc, struct user *user)` による両者の初期化と bind
- `current_user()` と `#define u (*current_user())`
- current なしの coroutine 用 base user と thread 用 TLS base user
- `exe_proc.p_user` と、単一コマンド実行時の current 設定・復元
- sleep/wakeup をまたぐ user の保持と host/worker の分離試験

未導入部分は `u_ofile`、I/O 状態、結果領域、`u_procp` と、これらを使う
file・inode・pipe の処理。引き継ぎ書の Step 1 は既存導入済みとして扱い、
次に実装する段階は Step 2 とする。

## V7 原典との対応

原典パスは `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/` からの
相対パス。以下を設計根拠とする。

| 原典とシンボル | 観察した動作 | MCUFW での決定 |
|---|---|---|
| `h/proc.h` の `p_addr`、`h/user.h` | proc と user を分離して参照 | 直接ポインタ `p_addr` を維持 |
| `sys/fio.c` の `getf/ufalloc/falloc` | current の `u_ofile`、`u_error`、`u_r` を使用 | 同じ受渡しに移行 |
| `sys/sys2.c` の `rdwr` | I/O 状態を準備し、要求量と残量から結果を計算 | 公開 sys API がこの役割を担う |
| `sys/trap.c` の system call entry | 開始時に error を消去し、終了時は error を結果より優先 | trap の代わりに公開 API で変換 |
| `sys/rdwri.c` の `readi/writei/iomove` | user の転送位置・残量・offset を更新 | 同一アドレス空間内のコピーで実現 |
| `sys/pipe.c` の `pipe/readp/writep` | inode/file を確保し、user を使って転送・待機 | 固定容量と既存 scheduler の範囲で踏襲 |
| `sys/fio.c` の `closef`、`sys/iget.c` の `iput` | 参照数を減らし、最終 close で peer を起こして解放 | 現行の参照数・wait channel 契約を維持 |
| `sys/alloc.c`、`sys/iget.c` | disk 資源不足と in-core table 不足を区別 | disk のない現行構成では既存 ENFILE を維持 |

V7 の writep は残量をローカル変数に保持し、`u_count` を区間ごとに使い直す。
また書込単位によって `PIPSIZ` を超える場合を許す。MCUFW では固定 buffer の
範囲内に制限し、全要求の残量を関数終了時に正しく復元する。V7 の数値表現、
MMU、trap、signal、disk block 管理まで再現するものではない。

## user の所有関係と寿命

`struct proc` に user 実体を埋め込まず、必ず `p_addr` 経由で参照する。
実体は呼出し元が固定領域または十分な寿命を持つ局所領域に置く。

| 実行場面 | 実体と寿命 |
|---|---|
| executor のコマンド | 既存の `exe_proc.p_user`。所属 executor の寿命内で保持 |
| pipe 接続の一時 owner | `owner` と `owner_user` を同じ呼出し内で保持し、fd 解放後に終了 |
| scheduler 単体テスト等 | proc と対になる呼出し元所有の user 配列・局所変数 |
| current のない host | 既存の backend base user。thread では TLS |

`proc_init(proc, user)` は初期化前の user を参照せず全体を初期化し、`p_addr` を
設定する契約を維持する。`u_procp` 導入時に逆参照も設定する。初期化済みの
実行単位を再利用する場合は、全 fd の解放、backend 停止・unwind・join 完了後に
限って再初期化する。再 bind 用 API や動的確保は追加しない。

backend のない proc にも user を割り当てる。backend slot の寿命に user を従属
させない。`sched_release()` は backend を解放しても user を解放・ゼロ化しない。
`u_procp` は基底 user では NULL とし、基底 user に fd はインストールしない。

同一 user の実行中に外部 sys API を再入させない。管理 helper と終了処理は
外部 sys API を呼ばず、進行中の user I/O 状態を上書きしない。将来の再入対応は
別の設計とする。

## メンバと型

以下は最終形の宣言契約。Step 0 ではヘッダへ追加しない。

| メンバ | 型 | 選定理由 |
|---|---|---|
| `u_error` | `int` | 正の既存 `SYS_E*` を保持。host errno と独立 |
| `u_procp` | `struct proc *` | 所有する proc。基底 user は NULL |
| `u_ofile` | `struct file *[NOFILE]` | 現行 `p_ofile` と同じ容量・参照所有権 |
| `u_base` | `unsigned char *` | byte 単位のコピーとポインタ更新 |
| `u_count` | `size_t` | 現行公開 API の要求量と整合 |
| `u_offset` | `ptrdiff_t` | 負の offset を検出でき、対象の pipe 位置を表現可能 |
| `u_r.r_val1`、`r_val2` | 各 `ptrdiff_t` | 転送量と fd を格納し、公開 API の戻り値と整合 |

`u_r` は名前付きのメンバを持つ最小の struct とする。今回使わない offset/time
返却の union や匿名 struct は追加しない。標準型 `size_t`、`ptrdiff_t` は使用するが、
新しい typedef は定義しない。

`user.h` は `<stddef.h>` と `param.h` を取り込み、file/proc は前方宣言とする。
`sched.h` は user の前方宣言を維持できる。ヘッダの相互 include を避ける。

`i_size` と `f_offset` の既存 `size_t` は維持する。`u_offset` を buffer 添字へ
変換する前に `0 <= u_offset <= PIPSIZ` を検査する。`PIPSIZ <= PTRDIFF_MAX` を
ビルド時に確認し、API の正の要求量は `count <= PTRDIFF_MAX` に制限する。
offset 加算は実際の容量で制限し、符号変換・加算の overflow を発生させない。

`sys_write()` の `const void *buf` は維持する。user への設定箇所だけで明示的に
ポインタを変換し、B_WRITE では source としてのみ使用する。読取専用バッファを
変更しないことを試験する。NULL のゼロ長要求でポインタ演算しない。

`u_pofile`、MMU/trap 関連メンバ、資格情報、ディレクトリ情報、`p_size` は追加しない。

## 内部 API と管理 API

### current user に作用する内部処理

| 宣言 | 結果と失敗 |
|---|---|
| `struct file *getf(int fd)` | file または NULL。失敗時 `u_error = SYS_EBADF` |
| `int ufalloc(void)` | 空き fd と `u_r.r_val1`。失敗時 -1 と `SYS_EMFILE` |
| `struct file *falloc(void)` | fd に登録した file。失敗時 NULL と `SYS_EMFILE/ENFILE` |
| `void pipe_create(void)` | 両端 fd を `u_r.r_val1/r_val2` に設定 |
| `void readp(struct file *fp)` | user で転送し、待機・EOF・error を処理 |
| `void writep(struct file *fp)` | user で転送し、待機・部分転送・error を処理 |
| `void readi(struct inode *ip)` | user の位置・残量を更新 |
| `void writei(struct inode *ip)` | user の位置・残量と inode size を更新 |
| `void iomove(void *cp, size_t count, int flag)` | buffer と user 間をコピーして状態を更新 |
| `void plock(struct inode *ip)` | inode lock を取得。取得できない場合は `u_error` |
| `void prele(struct inode *ip)` | lock 解放と必要な wakeup |
| `void closef(struct file *fp)` | file 参照を一つ消費し、最終参照で endpoint を解放 |

通常の内部処理は kernel lock 保持中に呼ぶ。内部成功では error を消さない。
原典で結果のある getf/ufalloc/falloc は戻り値を維持し、結果のない I/O は明示的な
void とする。`pipe_create` は host の `pipe()` との名前衝突を避ける MCUFW 名。

現行の `plock()` の負の戻り値は正の `u_error` に移す。呼出し側は error を確認し、
取得できなかった lock に `prele()` を実行しない。backend なしの競合は EAGAIN、
sleep 自体の失敗は既存と同じ EINVAL。pipe の空・full による待機前には inode lock を
解放する。kernel lock は wait 登録から backend の unlock/wait まで保持し、
復帰後は kernel lock 保持状態で条件を再検査する。

`iomove()` は B_READ で buffer から user、B_WRITE で user から buffer へコピーする。
既存 `buf.h` の B_READ と B_WRITE の値を維持する。
呼出し側は cp と count が有効な buffer 範囲内にあることを確認する。
iomove 側は方向、残量、ポインタ、offset 加算を検査し、不正なら EINVAL として
状態を更新しない。引数にないコピー先領域の長さまで推測して検証しない。
コピー成功時だけ base/count/offset を同じ量だけ更新する。
readi は負の offset、offset > i_size、i_size > PIPSIZ を EINVAL とする。
offset == i_size は転送 0。writei は負の offset、offset > PIPSIZ、
i_size > PIPSIZ を EINVAL とし、固定容量内の部分転送を維持する。

### 別 proc を操作する管理処理

既存の `fd_install()`、`fd_install_at()`、`fd_close()`、`close_all()`、
`pipe_for_proc()` は、対象 proc を明示する管理 API として維持する。
既存 `getf(proc, fd)` の管理用途は `getf_for_proc(proc, fd)` に改名する。
管理 API は current を付け替えず、`proc->p_addr->u_ofile[]` を操作する。

fd の検索・install・close は対象の I/O 状態と error/result を変更せず、
従来どおり戻り値で管理側へエラーを通知する。V7 型の current API と、内部の
fd 検索・file 割当・参照数操作を共有し、同じ確保処理を二重実装しない。

`pipe_for_proc()` は実行前の準備用であり、対象に進行中の I/O がないことを前提とする。
current 型 `pipe_create()` と共通の、対象 user を明示する確保処理を利用する。
必要な共通 helper は file/pipe 内部 API として宣言する。対象 user の error/result を
準備・設定して結果を負の error と fd 配列へ変換するが、呼出し側 user は変更しない。
失敗時は pipe 構築で確保した資源だけを戻し、出力 fd 配列は変更しない。

`ialloc()` は既存の引数なし・inode 戻り値を維持し、失敗時に current user の
error を設定する。別 user 向け pipe 構築では同じ inode 確保ロジックを使う
明示 error 出力の helper を設け、呼出し側 user を汚さない。
`iput()` と `closef()` の正常な後始末は既存 error を消さず、追加の割当も行わない。
現行の pipe/buffer 操作は kernel lock で直列化されるため、closef/iput に新たな
待機可能処理を加えない。V7 closef の plock と disk 側 iput の全面再現は今回の
user 移行に含めず、終了処理で管理対象以外の user 状態を変更しない。

## I/O 状態と公開 API の結果

公開 `sys_read/sys_write/sys_pipe/sys_close` のシグネチャは維持する。
各 API の開始で current user の error と result を準備し、終了時に正の error を
負の戻り値へ変換する。下位関数では成功時に error を消さない。

read/write は開始時の要求量を保持し、関数終了時の `u_count` を全要求の残量にする。
`u_r.r_val1 = 要求量 - u_count` とする。途中エラーでも実際の転送済み量を保持し、
外部へ error を返す場合も buffer や inode への既存の副作用は巻き戻さない。

writep は V7 のローカル残量と区間転送という構成を踏襲してよいが、各区間を
`PIPSIZ - i_size` 以下に制限する。区間終了後やエラー脱出時に未着手分を含む残量を
復元する。`u_offset` は現在の inode I/O 位置であり、要求全体の転送量に使わない。

公開 API の検証順は次とする。

1. user の error/result と、read/write の初期状態を準備する。
2. read/write の count が 0 なら、fd・buffer・current proc・peer を調べず 0。
3. 正の count で NULL buffer、または count > PTRDIFF_MAX なら EINVAL。
4. fd とアクセス方向を確認する。read/write は current proc なしも EBADF。
5. pipe/close は current proc なしを EINVAL。pipe の NULL fd 配列も EINVAL。
6. 検証後に転送・待機または確保・解放する。

基底 user は buffer I/O や起動処理に使えるが、sys fd API のための隠れた proc には
しない。sys_pipe で返す fd は成功時のみ配列へ書く。

### エラーと部分転送

| 状態 | `u_error` | 公開結果 |
|---|---|---|
| 通常転送で n byte 完了 | 0 | n |
| 空、最終 writer なし | 0 | 0 |
| backend あり、空で writer あり | 待機中は保持 | read 待機後に再検査 |
| backend あり、full | 待機中は保持 | write 待機後に再検査 |
| backend なし、空または full、転送 0 | EAGAIN | `-SYS_EAGAIN` |
| backend なし、write が n byte 転送後に full | 0 | n |
| reader なし、または write の途中で reader close | EPIPE | `-SYS_EPIPE` |
| inode lock 競合、backend なし | EAGAIN | `-SYS_EAGAIN` |
| sleep 失敗・停止による unwind | EINVAL | `-SYS_EINVAL` |
| 転送中にその他の error | 対応する正の error | 転送済み量より error を優先 |

最終二つのエラー経路も、完了済み量と残量は保持する。EPIPE を短い成功 write へ
変える POSIX 仕様の追加は行わない。V7 の trap による error 優先と現行 MCUFW の
戻り値に合わせる。backend なしの full による短い成功 write は現行の例外として
維持するが、inode lock 競合や sleep 失敗による短い成功へ一般化しない。

### 代表例

例は `PIPSIZ = 256`、有効な方向の fd と十分な呼出し元バッファを前提とする。

| 呼出しと状況 | 戻り値 | 終了時の残量と結果 |
|---|---|---|
| read/write、不正 fd と NULL、count 0 | 0 | count 0、val1 0、error 0 |
| read/write、NULL、count 1 | -EINVAL | count 1、val1 0 |
| read/write、不正 fd、count 1 | -EBADF | count 1、val1 0 |
| write 300、backend なし、空の pipe | 256 | count 44、val1 256、error 0 |
| 上記の full pipe に再度 write 44 | -EAGAIN | count 44、val1 0 |
| read 300、100 byte の data あり | 100 | count 200、val1 100 |
| read 1、空、writer なし | 0 | count 1、val1 0 |
| write 300、256 byte 完了して待機後に reader close | -EPIPE | count 44、val1 256 |
| write 300、backend あり、peer が読み続ける | 300 | count 0、val1 300 |

表中の -EINVAL 等は `-SYS_EINVAL` 等の略記。エラー時の val1 は診断用の転送量であり、
成功の戻り値として使わない。

## 確保失敗と後始末

| 不足する資源 | 決定する error |
|---|---|
| 当該 user の fd | EMFILE |
| global file entry | ENFILE |
| in-core pipe inode | ENFILE |
| inode 用 buffer を確保できない | ENFILE |
| backend slot、native thread 作成 | 既存どおり ENOMEM |
| bread の buffer busy/枯渇 | 既存どおり EAGAIN |

inode と buffer 不足は Step 0 では新しい error に分離しない。現行 sys_pipe の結果と
既存試験を維持するため。V7 の disk ENOSPC を storage のない構成へ追加しない。

pipe は inode、read file/fd、write file/fd の順で構築する。途中失敗時は自身が
取得した資源だけを戻し、最初の失敗 error を保持する。両端が完成するまで不完全な
endpoint を実行単位へ公開しない。fd 参照の複製は f_count、endpoint の寿命は
file から inode の参照で管理する既存契約を維持する。

通常終了・準備失敗・待機中解除では既存の close、wakeup、unwind、join の順序を
維持し、全 worker が user に触れなくなるまで実体を再利用しない。
kernel lock 下で join しない。同期 driver の現在の bio 契約は変更せず、将来の
非同期完了では b_error に記録して要求元が user へ取り込む。

## 後続 Step の変更範囲

- Step 2: user に `u_ofile` と `u_procp` を追加し、proc_init と stub を対応。
  全 `p_ofile` 参照を `p_addr->u_ofile` へ移す。外部 API はまだ変更しない。
- Step 3: 結果領域 `u_r` を追加し、fio の current API と管理 API を分け、
  pipe_create と確保 helper を導入。
  fd/file/inode/buffer 枯渇と rollback を検証する。
- Step 4: base/count/offset と iomove を導入し、inode I/O を移行する。
  必要な変換 helper はこの段階だけのものと明記する。
- Step 5: readp/writep/sys2 の結果・待機処理を一体で切り替える。
  不要になった `pipe_io_status` と互換経路を削除する。
- Step 6～7: error・寿命・資源再利用を検証し、全回帰と現行ガイドを更新する。

新しい本体 C ファイルは必須としない。sys/sched.c、sys/fio.c、sys/pipe.c、
sys/rdwri.c、sys/sys2.c の責務に沿って配置する。既存 Makefile の user stub 対応は
導入済みなので、旧引き継ぎ書の記述だけを理由に再追加しない。

## Step 0 の確認結果

原典と現行の確保、転送、待機、close、inode 解放、user 寿命を静的に確認した。
新規コード・新規テストは追加していない。既存試験の実行結果は下記に記録する。

| 試験 | 構成 | 結果 |
|---|---|---|
| header | C/C++、単独/順序入替え、O0/O2 | passed |
| sys file | O0/O2 | passed |
| sys scheduler | cortn/thread、O0/O2、既定/NBUF=3 | passed |
| pipeline executor | cortn/thread、O0/O2 | passed |

試験は MSYS2 MINGW64 shell 経由で実行し、全 script が正常終了した。
scheduler は script 内で各構成を clean し、header/file/pipeline executor は
外側の EXIT trap で対象 OUTDIR/TARGET の make clean を実行し、正常終了した。
pipeline executor のビルドには既存の unused parameter/function、char 添字、
snprintf の切詰め可能性などの警告がある。文法異常系の試験では lex/parser の
診断も出力される。今回の source 変更による警告ではなく、範囲外として修正しない。
リポジトリ直下から次を実行した。

```sh
bash src/core/sys/test/header/run_test.sh
bash src/core/sys/test/file/run_test.sh
bash src/core/sys/test/sched/run_test.sh
bash src/core/esh/test/pipeline_exec/run_test.sh
```

変更のない Step 0 では shell 本体・parser・全 bio suite の再試験までは行わず、
実装変更後に引き継ぎ書の全回帰条件を適用する。
