# UNIX V7方式のRAMブロックデバイス実装 引継ぎ書

## 現行APIへの更新（2026-10-22）

Step 0〜6とレビュー修正の記録は当時の仕様として保存する。その後のbio変更により、
本文中の旧signature・error処理・書戻し失敗時の契約には次の差分がある。

- bread(dev, blkno)からint *error引数を除去した
- bwrite、bdwrite、bawrite、bflush、geterror、strategyは戻り値を返さない
- geterrorはB_ERROR/b_errorからcurrent userのu_errorへ正のerrorを設定する
- 呼出元が操作前にu_errorを0にする。成功時は以前のerrorを消さない
- proc.p_addrとcurrent_user()で実行単位ごとにu_errorを分離する
- bioは有効な同期strategyを前提にd_strategyを直接呼ぶ。bio入口でのENOSYS拒否はない
- error付きbufferのbrelseはb_devをNODEVにする。dirty保持・再試行保証はない
- bflushは戻り値でerrorを集約しない
- reassociateはassign_buffer_to_blockへ改名し、I/O補助fieldの0初期化を除去した
- queue型はstruct bdev_io_queue、定数prefixはBDEV_IO_QUEUEである

現在のAPI・使用例は[RAM block I/O](ram_block_device.md)を優先する。
以下の試験結果は当時の記録であり、今回の文書整備では再実行していない。

作成日: 2026-10-16

対象: `C:\usr\src\MCUFW`

確認時HEAD: `28a835b`（レビュー指摘修正の開始時）
状態: Step 0〜6とレビュー指摘2件の修正完了。修正記録はSection 19を参照

## 1. 目的と到達点

MCUFWでUNIX V7方式のブロックデバイスI/Oを利用可能にする。
デバイス番号`dev`のmajorでドライバ、minorでドライバ内の装置を選択し、
最初のドライバとして固定RAM領域を読み書きするRAMブロックデバイスを追加する。

V7の名前、責務分割、要求と完了の流れを採用する。PDP-11用ディスクドライバや
V7 kernel全体の移植、V7ディスクイメージとの互換性は今回の目標に含めない。

最初の到達点はStep 3の完了、すなわち`bread(dev, blkno, &error)`と`bwrite(bp)`による
RAMブロックの読み書きである。Step 4で既存pipeとの共存を確認し、Step 5で
遅延書込み、Step 6で全体の統合を完了する。

## 2. 別チャットでの開始方法

次の依頼文を使用できる。Xは進捗表から選ぶ。

```text
docs/unix_v7_ram_block_device_handoff.mdを読み、Step Xを実施してください。
開始時に現行コードと前Stepの完了条件を確認してください。
担当Stepの実装と必要な試験を行い、引継ぎ書へ結果と次Stepへの注意事項を
追記してください。後続Stepは先取りしないでください。
```

各チャットは最初に次を実施する。

1. ルートの`AGENTS.md`と`docs/coding_style_summary_linux_kernel.md`を読む
2. 本書、担当Step、既存の関連引継ぎ書を読む
3. `git status --short`、現在のHEAD、branchを確認する
4. 現行コードとの差分を調べ、前Stepの完了を確認する
5. 関連試験のbaselineを取得し、担当Stepを`active`へ更新する
6. 完了条件を満たしたら結果と変更ファイルを記録して`done`へ更新する

既存変更、未追跡ファイル、試験生成物を削除・巻戻し・変更へ混入させない。
commitは依頼の範囲に従い、未commitなら記録欄にその旨を書く。
仕様案の通常の実装判断はStep 0で根拠とともに確定する。目的を変える判断や
解消できない矛盾がある場合に限り、論点を整理して利用者へ確認する。

## 3. 進捗表

| Step | 内容 | 状態 | 完了commitまたは作業状態 |
|---|---|---|---|
| 0 | 仕様とbaselineの確定 | done | 開始HEAD a3a6893、本書を含むcommitに記録 |
| 1 | dev指定と要求契約の整備 | done | 開始HEAD 3c50a9e、本書を含むStep 1 commitに記録 |
| 2 | RAMドライバの単体実装 | done | 開始HEAD a2f6171、本書を含むStep 2 commitに記録 |
| 3 | breadとbwriteの実装 | done | 開始HEAD 0054ec9、本書を含むStep 3 commitに記録 |
| 4 | 資源と排他とpipe共存 | done | 開始HEAD 6fc0b54、本書を含むStep 4 commitに記録 |
| 5 | 遅延書込みと書戻し | done | 開始HEAD 4de9278、本書を含むStep 5 commitに記録 |
| 6 | Windows統合と全回帰 | done | 開始HEAD 0acf602、本書を含むStep 6 commitに記録 |

状態は`pending`、`active`、`done`、`blocked`を使用する。
失敗試験や未解決事項がある状態を`done`としない。

## 4. 現行コードと不足部分

| ファイル | 確認済みの内容 |
|---|---|
| `src/core/sys/h/buf.h` | dev分解マクロ、範囲検査付きmakedev、BSIZE=256、NBUF=5 |
| `src/core/sys/h/conf.h` | bdevsw、bdev_queue、dev検査、要求検査、提出、queue API |
| `src/core/sys/conf/conf.c` | 固定table、pipeとRAMのsentinel、登録済みRAM entry |
| `src/core/sys/dev/ramdisk.c` | 固定RAM領域、raminit、ramstrategy、同期コピーと完了 |
| `src/core/sys/dev/bio.c` | cache取得・解放、bread/bwrite、遅延書込み、flush、iodone |
| `src/core/sys/dev/bdev.c` | enqueue、start、complete、error/retry |
| `src/core/sys/sys/alloc.c` | pipe inodeごとにgetblkで取得しbufferを保持 |
| `src/core/sys/h/sched.h` | kernel lock、wait channel、sleep/wakeup API |
| `src/core/sys/h/sys_error.h` | host errnoから独立したerror定義 |

`bread()`、`bwrite()`とRAM driverは実装済み。実際の非同期I/O待機は未実装。
buffer I/O APIはBDEV_SYNC driverだけへ提出し、未対応driverはENOSYSで拒否する。
`iodone()`は`B_DONE`を設定するが、同期要求の待機者を起床させない。
`getblk()`と`geteblk()`はdirty候補を書戻してから再利用する。
書戻し失敗時は内容と関連付けを保持してNULLを返し、bflushでerrorを取得できる。
`getblk()`はbusy hitや空きなしでも待機せず`NULL`を返す。

既存pipeは寿命の間bufferを`B_BUSY`で保持する。既定構成は3本のpipeが3個を使い、
残る2個をRAM I/Oで利用できる。専用予約はなく、競合・枯渇時はEAGAINを返す。

関連文書:

- `docs/unix_v7_buffer_cache_pipe_handoff.md`
- `docs/unix_v7_generic_sys_migration_handoff.md`
- `docs/bio_reassociate_explanation.md`

過去文書の計画・配置より現行コードを優先して確認する。

## 5. V7との対応と資料

歴史的実装の確認には`ancient-unix`スキルを使用する。
一次資料の基点は`.agents/skills/ancient_unix/references/unix_v7/v7/`。

| 資料 | 確認するsymbolと責務 |
|---|---|
| `usr/sys/h/param.h` | major、minor、makedev、NODEV、BSIZE |
| `usr/sys/h/conf.h` | bdevsw、d_open、d_close、d_strategy、d_tab |
| `usr/sys/h/buf.h` | buffer状態とI/O要求 |
| `usr/sys/dev/bio.c` | bread、bwrite、bdwrite、bawrite、getblk、geteblk、iowait、iodone、bflush |
| `usr/sys/dev/rk.c` | strategy、minor解釈、queue、start、interrupt完了 |

V7はmajorでswitchを選び、minorの意味はdriverが決める。
cache keyは`(dev, blkno)`であり、同じmajorの異なるminorも区別する。
V7のBSIZEは512だが、MCUFWは当面256を維持する。

```text
bread(dev, blkno) / bwrite(bp)
  -> buffer cache
  -> bdevsw[major(dev)].d_strategy(bp)
  -> minor(bp->b_dev)でRAM装置選択
  -> RAMコピー
  -> iodone(bp)
```

V7のglobal `u`、trap、MMU、割込み禁止命令をコピーしない。
errorは明示的に返し、待機が必要になればMCUFWのschedulerへ接続する。
V7のqueue用field流用も採用せず、既存`b_io_next`と`struct bdev_queue`を使う。

## 6. 初期仕様案と不変条件

以下は計画時の提案。Step 0での確定内容はSection 11を優先する。

### Step 0で確定する推奨値

- `PIPEDEV=makedev(0, 0)`を維持する
- RAM driverのmajorを1とし、minorをRAM装置番号とする
- 試験用構成を2装置、各16ブロックとする。BSIZE=256なら保存領域は計8 KiB
- 装置数と容量は固定構成とし、必須のheap allocationを追加しない
- dev保持型は現行`unsigned int`を維持し、有効major/minorを各8 bitとする
- 範囲外major/minorやNODEVを切捨て・丸めで別装置へ変換しない
- 初期driverは1要求1ブロック、`b_bcount == BSIZE`だけを許可する
- RAMコピーはstrategy呼出し中に完了する

以上は確定仕様ではない。Step 0で採否と理由を記録し、以後のStepへ引き継ぐ。
devマクロの互換性を変える場合は全consumerを確認する。生成時の引数範囲検査と
生成済みdevの検査を区別し、無効入力が有効devへ化ける経路を残さない。

### 全Stepで維持する条件

- RAM保存領域は`buffers[][]`と別配列であり、cache追出し後も内容を保持する
- `binit()`でRAM内容を消さない。装置resetは別操作とする
- live buffer、file、pipe、進行中I/Oがある状態で全体初期化しない
- 装置reset時は関連cacheを無効化するか、利用者不在とcache破棄を前提にする
- majorは共通層で検査し、minorと装置容量はRAM driverで検査する
- offset計算前に容量を検査し、乗算・加算overflowを防ぐ
- 要求受付後は成功・失敗とも一度だけ完了する
- driverがbufferを完了した後、解放済みbufferへアクセスしない
- `B_BUSY`、b-list、av-list、device queueの既存不変条件を維持する
- read完了後のbufferはcaller所有、同期write完了後は解放する
- 全blockを書かない場合はread-modify-writeが必要。getblkだけでは旧内容を保証しない
- errorコードは`sys_error.h`の方式へ合わせ、host errnoを直接流さない

初期範囲にpathname、特殊ファイル、mount、filesystem、物理storage、DMA、
read-ahead、shell公開コマンドは含めない。`d_open`と`d_close`は特殊ファイル等の
利用経路が必要になった段階で追加し、空の互換枠だけを先に増やさない。

## 7. 実装Stepと完了条件

### Step 0 仕様とbaselineの確定

1. 現行sourceと上記V7 sourceの呼出し・完了・解放経路を確認する
2. Section 6の値、公開API、戻り値、error、buffer所有権を確定する
3. strategyの受付拒否と受付後失敗を区別する
4. buffer数とRAM予算、初期化順、lock前提を記録する
5. 関連baseline試験を実行する

完了条件: 後続StepがAPI契約を推測せず実装でき、baseline結果が記録される。

### Step 1 dev指定と要求契約の整備

1. dev範囲とswitch参照の共通検査を整備する
2. RAM major用のb-list sentinelとtable構成を整備する
3. `d_strategy`未登録、NODEV、不正majorを安全に拒否する
4. 必要な独立error定義を追加する

driver未実装でもbuildと既存試験を通す。未登録entryを呼び出さない。
minorの有効性はdriverが判断し、cache層へRAM固有知識を入れない。

完了条件: 不正devで配列外参照せず、異なるminorのcache keyが独立する。

### Step 2 RAMドライバの単体実装

配置案は`h/ramdisk.h`、`dev/ramdisk.c`、`test/ramdisk/`とする。
以下のpathはすべて`src/core/sys/`からの相対path。

1. 固定RAM保存領域と初期化APIを追加する
2. RAM driverとqueueをswitchへ登録する
3. `ramstrategy()`でdev、block、転送長、bufferを検査する
4. enqueue、start、B_READに応じたコピー、completeを実装する
5. 正常時b_resid=0、失敗時のb_residとb_errorを契約どおり設定する
6. immediate completionとB_ASYNC時の解放を検証する

完了条件: bread未実装でもdriver単体でread/writeでき、境界とerror試験が通る。

### Step 3 breadとbwriteの実装

1. `dev/bio.c`と`h/buf.h`へAPIを追加する
2. breadのcache hitではstrategyを呼ばず、missでread要求を提出する
3. bwriteで旧B_READ、B_DONE、B_ERROR、B_DELWRI等を適切に整理する
4. error情報を解放前に退避し、定めた所有権で返却・解放する
5. cache追出し後の再読込みでRAM保存を確認する

Step 3は即時完了driverを対象とする。未完了要求をspinで待たない。
後から完了するdriverへの対応は別途wait契約が必要であり、この段階で対応済みと
主張しない。iowaitを設ける場合も対応範囲を明示する。

完了条件: dev指定の公開APIから読み書きでき、異常時もbufferが回収される。

### Step 4 資源と排他とpipe共存

1. 最大pipe数と同時I/O数からNBUFを決め、設定箇所を統一する
2. cacheとqueueのlock責務、caller側の前提を定める
3. 既存kernel lockは非再帰であるため、二重lock経路を作らない
4. busy hit、buffer枯渇の結果を明示し、無期限の再試行を避ける
5. coroutine/threadでpipe併用と資源回収を確認する

初期RAMは即時完了なので、非同期driver用の待機実装は必須としない。
待機を追加する場合は条件検査とwait登録を同じlock区間で行い、再開後に再検査し、
iodoneとbrelseから必要なwaiterを起床する。process外の呼出し規則も定める。

完了条件: 最大pipe構成との併用、枯渇、競合で破損・leak・deadlockがない。

### Step 5 遅延書込みと書戻し

1. bdwrite、必要なbawriteとflush APIを契約とともに追加する
2. getblkとgeteblkでdirty候補を書戻してから再探索する
3. write失敗時のdirty dataの保持・破棄方針を確定する
4. flush対象dev、全dev指定、busy buffer、完了とerror集約を定義する
5. 全buffer dirty、同じblockの再取得、失敗後再試行を試験する

bdwriteだけを先行実装しない。V7のbflushは非同期提出する経路を持つため、
MCUFWのflushが提出完了と保存完了のどちらを保証するか明記する。
同期errorの返却だけでは遅延書込みの失敗を扱えないため、通知方法も決める。

完了条件: dirty bufferを未保存のまま再関連付けせず、flushと失敗試験が通る。

### Step 6 Windows統合と全回帰

1. 関連Makefileへdriverと必要なincludeを追加する
2. 初期化順とRAM resetの条件を確認する
3. Section 8の全回帰を実行する
4. メモリ使用量、API使用例、既知の制約と試験結果を記録する

完了条件: Windows buildと既存shell動作を維持し、全Stepの記録を完了する。
STM32実機移植はこの後の作業とする。

## 8. 試験手順と観点

GCCは必ずMSYS2 MINGW64 shell経由で使用する。
PowerShellから`C:\msys64\mingw64\bin\gcc.exe`を直接実行しない。
以下はMINGW64 shell内で実行する。各directoryの存在とscript内容を開始時に確認する。
AGENTS.mdの絶対ルールに従い、成功・失敗とも試験後はmake cleanを実行し、
今回生成した*.d、*.o、*.exeが残っていないことを確認する。
OUTDIR/TARGETを変えてbuildした構成は、同じ値をmake cleanにも渡す。

```sh
cd /c/usr/src/MCUFW/src/core/sys/test/bdev
rc=0
bash ./run_test.sh || rc=$?
for opt in O0 O2; do
    make clean OUTDIR="_output_$opt" TARGET="test_bdev_$opt.exe" || exit 1
done
test "$rc" -eq 0
```

上記はbdevの例。同様にsysのheader、table、bio、ramdisk、file、pipe、schedと、
eshのpipe、sched、cortn、pipeline、pipeline_exec、exe、parを影響範囲に応じて実行する。
schedとeshのpipe/pipeline_execではbackendと最適化の全構成をcleanする。
ramdisk/run_test.shはEXIT trapと構成ごとのmake cleanを持つ。

新規RAM試験は同様のrun_test.shでO0/O2を検証する。
backend対応suiteはcoroutine/thread両方の結果を記録する。
Windowsは`src/target/windows`でmake、main.exeを起動し、以下を確認する。

```text
sam1
sam2 1 10 100
sam3 -a -b string
```

試験を広げる順は変更箇所の単体試験、影響するsuite、Step 6の全回帰とする。

必須観点:

- devのmajor/minor往復、許容最大値、範囲外、NODEV、未登録entry
- 同じblock番号で異なるminorへ書いても混ざらない
- block 0、最終block、容量外、極端に大きいblock番号
- 不正転送長、失敗時のerrorとresid、二重enqueue拒否
- cache hitでdriverが再実行されず、追出し後はRAMから復元できる
- B_ASYNC即時完了後の二重解放・解放後アクセスがない
- binitだけではRAM内容が消えず、reset後は古いcacheを読まない
- 全buffer busy、全buffer dirty、pipe最大数での併用
- 試験終了時にbusy buffer、queue要求、file、inodeが残らない
- EOF、EPIPE、binary/NUL転送、早期consumer終了の既存動作を維持する

書込み直後に同じcacheを読む試験だけではRAM driverの保存動作を検証できない。
必ずcacheから追い出して再読込みする。live資源がない試験ではcacheだけを
再初期化してRAMの保持を確認してもよい。

## 9. 見通しと実装上の注意

既存devマクロ、switch、queueを利用できるため、基本I/Oの中心はRAM driverと
bread/bwriteの追加になる。作業量が増えやすいのは排他、資源枯渇、dirty処理である。
Step 3完了時点で基本経路を評価し、その結果でStep 4以降の範囲を再確認する。

- typedefを追加しない
- header guardは`__NAME_H__`形式とする
- sourceの節構成は`src/core/esh/tty.c`に合わせる
- コメントとcommit messageは冠詞を可能な限り省く
- 一文コメントは末尾periodなし、コメント記号とindent込み80 column以内
- STM32、物理storage、filesystem全体への拡張を先取りしない

## 10. Step完了記録

各Step完了時に以下をコピーして追記する。

```text
Step:
実施日:
開始HEAD:
完了commitまたは未commit:
変更ファイル:
確定した仕様と理由:
実行した試験と結果:
未実行試験と理由:
残課題:
次Stepへの注意事項:
```

## 11. Step 0で確定した仕様

確定日: 2026-10-17。開始HEADは`a3a6893`、branchは`feature/mrunsv`。
本Sectionは実装契約であり、API追加自体はStep 1以降で行う。
Step 0ではsource、header、Makefileを変更しない。

### 11.1 dev番号と配置

| 項目 | 確定値 |
|---|---|
| dev保持型 | unsigned int。typedefを導入しない |
| 有効な符号化 | major/minor各8 bit、上位bitは0 |
| 無効値 | NODEV。符号化が有効でも未登録装置は使用不可 |
| pipe | major 0、PIPEDEV=0Uを維持 |
| RAM driver | major 1、RAM_MAJOR=1U |
| RAM装置 | minor 0と1、RAM_NDEV=2 |
| 装置容量 | 各RAM_NBLK=16ブロック |
| ブロックサイズ | BSIZE=256を維持 |
| 保存領域 | 2×16×256=8192 byte、固定配列 |
| RAM配置 | src/core/sys/dev/ramdisk.c、h/ramdisk.h |

majorはdriver種類、minorはそのdriver内のRAM装置番号を表す。
major 1のentryに専用b-list sentinelと1個のqueueを持ち、両minorで共有する。
同じb-list内でもcache keyのdev全体を比較し、装置間を分離する。

Step 0確認時のmakedevはminorの範囲外bitがmajorへ混入する。
Step 1でmakedevを同名static inline関数へ変更し、unsigned intの引数を
一度ずつ評価する。各引数が255を超えればNODEVを返し、範囲検査後にshift/ORする。
現行sourceに定数式としてのmakedev利用はない。PIPEDEVの定数0Uは維持し、
今後もmakedevの結果をenumやstatic initializerの定数式として使用しない。
負値を渡した場合もunsigned変換後の範囲検査で拒否する。
呼出し前に狭い型へ変換して失われたbitの検出は保証しない。

major/minorは有効devの分解に使い、入力検査の代用にしない。
共通層はNODEV、上位bit、不正majorをswitch参照前に検査し、参照後にd_tabを検査する。
getblkはpipe用にも使うためd_strategyがNULLでも取得を許可する。
I/O提出時にはさらにd_strategyの存在を検査し、未登録なら-SYS_ENODEVを返す。
minorと容量の有効性はdriverが検査する。

### 11.2 公開APIと所有権

```c
/* h/conf.h: Step 1 */
int bdev_strategy(struct buf *bp);

/* h/ramdisk.h: Step 2 */
void raminit(void);
int ramstrategy(struct buf *bp);

/* h/buf.h: Step 3 */
struct buf *bread(unsigned int dev, unsigned long blkno, int *error);
int bwrite(struct buf *bp);
```

breadの第3引数は、V7のglobal u.u_errorを明示的に置換するために追加する。
既存fallocのout引数方式と同様、ポインタ返却とerrorを分離する。
errorは必須であり、NULLなら資源を取得せずNULLを返す。
正常時は*error=0、失敗時は負のSYS errorを設定する。

| APIと結果 | 所有権と後始末 |
|---|---|
| bread成功 | B_BUSYのbufferを返す。callerがbrelseまたはbwriteする |
| bread失敗 | NULLを返す。途中取得bufferは内部で解放する |
| bwrite正常またはI/O失敗 | 有効な同期要求bufferを消費し、内部でbrelseする |
| bwrite契約違反 | NULL、非B_BUSY、B_ASYNC、queue所属を拒否。所有権と状態を変更しない |
| bdev_strategy受付 | driverへI/O処理を委譲。callerは完了までbufferを再利用しない |
| bdev_strategy拒否 | 負のerrorを返す。完了・解放せず、callerのbufferを維持する |

bwriteは0または負のSYS errorを返す。解放前に結果を退避する。
同期I/O提出失敗も、契約を満たすbufferなら消費して解放する。
破棄対象bufferはB_ERRORを設定してcacheから無効化する。
callerは失敗時もbwrite後のbufferを再解放・再参照しない。
queue所属は同じkernel lock区間で全登録queueのpending/activeを検査する。
B_BUSYだけでは提出可能性を判定しない。検査はNBUFで上限を設け、循環・破損を
検出した場合は要求を変更せず-SYS_EINVALで拒否する。

getblk/geteblkの既存signatureと非待機動作は維持する。
busy hitと空きなしはNULL、breadからは-SYS_EAGAINへ変換する。
Step 5前のdirty候補による取得不能も-SYS_EAGAINとし、spin retryしない。
breadはdevの共通検査を先に行い、不正major等を枯渇と混同しない。

breadのcache hit条件はB_DONEかつB_ERRORなしとする。ただしStep 5で追加した
失敗dirty bufferはB_DONEとB_DELWRIを維持し、保存前のcache内容を返す。
getblkはblock内容を初期化しないので、全blockを書き換える場合だけ直接使用できる。
部分更新はbreadで旧内容を取得してからbwriteする。

### 11.3 strategyと完了の契約

既存int型d_strategyを維持し、戻り値0は受付、負値は受付拒否を意味する。
I/O自体の成功・失敗は戻り値0の場合でもbuffer状態で判断する。
bdev_strategyは共通検査後にdriverを呼ぶだけで、拒否を自動完了させない。

受付拒否はNULL、非busy、既存queueへの二重提出、共通dev検査失敗等である。
拒否時はqueueとbufferのflag/linkを変更せず、iodoneも呼ばない。
driverが受付後に検出した不正minor、容量外、転送長不正、NULL b_addrは
要求失敗として完了する。RAM内容とread先bufferを変更せず、戻り値は0とする。

RAM要求は1block固定で、b_bcount==BSIZEだけを許可する。正常時b_resid=0。
失敗時はB_ERRORを設定し、b_errorに正のSYS error、b_residにb_bcountを設定する。
完了は成功・失敗とも一度だけ行い、queueを外してからiodoneする。
無効な要求をqueueへ入れない経路はdriverから直接iodoneしてよい。

RAMは受付要求をstrategyが戻るまでに完了し、pending I/Oを残さない。
bread/bwriteが使うproduction driverは、この即時完了契約に限定する。
mock非同期driverを使う既存queue試験は残すが、bread/bwriteには接続しない。
将来の非同期driver登録前にwait/resume契約を追加する必要がある。
未完了要求を解放してerror返却する処理や、spin待機で代用してはならない。

raw strategyではB_ASYNCも処理する。iodone内でbrelseされるので、完了後は
driverと呼出し側のどちらもbufferを参照しない。既存queue_completeの返却pointerも
完了の識別以外に使用しない。bwriteは同期専用で、Step 5の追加APIはSection 17参照。

### 11.4 errorの割当

SYS_ENODEVとSYS_EIOをsys_error.hの既存enum末尾へ追加し、既存番号を変えない。
負のerrorは関数結果、正のerrorはb_errorに格納する。

| 条件 | SYS error |
|---|---|
| NODEV、不正符号化、不正major、未登録strategy、不正minor | SYS_ENODEV |
| block番号がRAM_NBLK以上 | SYS_EIO |
| 転送長不正、NULL payload、所有権違反 | SYS_EINVAL |
| busy hit、buffer枯渇、二重提出 | SYS_EAGAIN |
| driverがB_ERRORを返したがb_errorが0 | SYS_EIO |

capacity検査はb_blkno<RAM_NBLKを先に行い、その後固定配列のblockを選ぶ。
巨大なblock番号をbyte offsetへ乗算してから比較しない。
RAM容量外はEOFではなくI/O errorとし、短縮転送は行わない。

### 11.5 初期化とメモリ予算

初回起動はswitchが静的に構成された状態でraminit、binit、finitの順とする。
raminitはRAM保存領域だけをzero clearし、cacheとfile tableを操作しない。
binitだけを再実行した場合はRAM内容を維持する。
WindowsのeshinitはStep 6でraminit/binit/finitの順へ接続した。

raminitは起動・試験のquiescent初期化専用とする。実行中reset APIは追加しない。
再初期化は全process停止、全file/inode解放、queue空、busy bufferなし、
未保存dirty dataなしの状態に限定し、raminitの直後にbinitする。
cache有効のままRAMだけを消して利用を再開する経路を公開しない。

Step 1〜3ではNBUF=3を維持した。Step 4で共存構成の既定値を5へ変更した。
NINODE=3とPIPELINE_MAX=4は維持し、最大3本のpipeに加えて2個のI/O bufferを
保持できる構成とする。予約poolは分けず、枯渇時はEAGAINとする。
小容量の上書き構成は維持するが、その構成では最大pipeとRAM I/Oの併用を保証しない。
NBUFのdefault定義はbuf.hの一箇所とし、translation unit間で値を変えない。

payload予算はRAM8192+cache1280=9472 byte。
別途、5*sizeof(struct buf)、3個のlist sentinel、1個のbdev_queueが必要である。
既存file/inode/scheduler領域とstackはこの数値に含まない。
alignmentとpointer幅によるheader容量はStep 4でsizeofとbuild結果から記録する。
STM32の最終配置・容量は移植時の別判断とする。

### 11.6 lockと実装順序

cache、switch、queue操作はcallerが既存kernel lockで直列化する。
低層getblk/brelse/strategy/iodoneおよびbread/bwriteは内部で再lockしない。
callerはlock取得から要求・payload操作・解放まで保護する。RAMコピーは256 byteで
即時完了し、この区間でsleep/yieldしない。所有中payloadを別callerへ公開しない。
初期化と単体試験は他の実行主体がいない場合だけlock省略可とする。
process外でもlockを取得して使用でき、current_procを必須としない。
ISRからの提出・完了は今回の範囲外。

Step 1〜3はこの前提を文書化し単一実行主体で検証する。
Step 4でthread/coroutineのcallerが契約を守ることを試験する。
非再帰kernel lockなので「必要なら内部でlockする」動的な判定は追加しない。

switchへRAM関数を登録するStep 2からconf.cがdriver symbolを参照するため、
conf.cをlinkする各Makefileへramdisk.cと必要なbdev.cを同じStepで追加する。
Makefile整備をStep 6まで延期して中間Stepをlink不能にしない。
Step 6はWindows起動接続と全体確認を担当する。

Step 5のdirty失敗保持、flush保証、error通知はSection 17で確定する。
Step 0の同期RAM契約を維持し、非同期driverの待機機構は追加しない。

### 11.7 Step 1の着手範囲

次チャットはStep 1だけを実施する。対象候補はh/buf.h、h/conf.h、
h/sys_error.h、dev/bdev.c、dev/bio.c、conf/conf.cと関連単体試験である。
RAM用sentinelと未登録entryは追加してよいが、ramstrategyや保存配列は追加しない。
dev分解はV7方式を維持し、不正makedev入力、switch境界、未登録strategy、
minor別cache key、二重提出拒否、拒否時の状態不変を試験する。
既存pipeはstrategyがNULLのままgetblkを使い続けられることを確認する。

Step 1の完了後、進捗表とSection 10へ結果を記録し、Step 2を別チャットへ渡す。

## 12. Step 0完了記録

実施日: 2026-10-17

開始HEAD: `a3a6893 Record final regression verification`

branch: `feature/mrunsv`

完了状態: 文書更新のみ。本書を含むcommitに記録

変更ファイルは本書のみ。source、header、Makefileは変更していない。
既存の未追跡文書、.agents、STM32 tree、生成物はcommit対象へ含めていない。
試験scriptによる通常の再buildと結果ファイル生成は実施した。
終了時の`git status --short --untracked-files=no`は空で、追跡済みファイルに変更なし。
本書はStep 0完了時点では未追跡であり、後から文書単独でcommitする。

### 確定した仕様と根拠

- V7 dev/bio.cのbread/bwriteからstrategy、iodone、解放までを確認した
- V7 dev/rk.cでminorによる装置選択とqueueからの完了経路を確認した
- V7 h/param.h、h/conf.h、h/buf.hで番号形式、switch、状態を確認した
- MCUFWのbio、bdev、conf、alloc、iget、fio、rdwri、schedulerとcallerを確認した
- global uの代用としてbreadのerror out引数を採用した
- bwriteは同期専用、RAMは即時完了とし、wait実装を今回の到達点から分離した
- pipe最大3本とI/O buffer2個を根拠に、Step 4でNBUF=5とする方針を確定した
- 各minorのデータ分離を検証するためRAM装置を2個とした
- 初期の固定容量と境界試験を簡単にするため各16block、計8 KiBとした

### baseline結果

実行環境はMSYS2 MINGW64、`/mingw64/bin/gcc`、GCC 16.1.0
（Rev5, Built by MSYS2 project）。PowerShellからMSYSTEM=MINGW64を指定して
`C:/msys64/usr/bin/bash.exe -lc`を起動し、そのshell内でscriptを実行した。
shellのsandbox内起動は出力なく失敗したため、承認されたsandbox外実行で確認した。

| 対象directory | scriptと構成 | 結果 |
|---|---|---|
| src/core/sys/test/header | run_test.sh、C/C++、O0/O2 | exit 0、all tests passed |
| src/core/sys/test/table | run_test.sh、O0/O2 | exit 0、all tests passed |
| src/core/sys/test/bio | run_test.sh、O0/O2 | exit 0、all tests passed |
| src/core/sys/test/bdev | run_test.sh、O0/O2 | exit 0、all tests passed |
| src/core/sys/test/file | run_test.sh、O0/O2 | exit 0、all tests passed |
| src/core/sys/test/pipe | run_test.sh、O0/O2 | exit 0、all tests passed |
| src/core/sys/test/sched | run_test.sh、cortn/thread × O0/O2 | exit 0、all tests passed |
| src/core/esh/test/pipe | run_test.sh、cortn/thread × O0/O2 | exit 0、all tests passed |
| src/core/esh/test/sched | run_test.sh、cortn/thread × O0/O2 | exit 0、all tests passed |
| src/core/esh/test/cortn | run_test.sh、O0/O2 | exit 0、all tests passed |
| src/core/esh/test/pipeline | run_test.sh、O0/O2 | exit 0、all tests passed |
| src/core/esh/test/pipeline_exec | run_test.sh、cortn/thread × O0/O2 | exit 0、all tests passed |
| src/core/esh/test/exe | run_test.sh、既定build、test1〜10とgolden比較 | exit 0、all tests passed |
| src/core/esh/test/par | run_test1.sh、既定buildとgolden比較 | exit 0、test pass |
| src/core/esh/test/par | run_test2.sh、既定buildとgolden比較 | exit 0、test pass |

全15項目が成功し、外側の一括実行もexit 0。
script実装どおりの構成を実行しており、exe/parをO0/O2両構成実施とは数えない。
logはMSYS2の`/tmp/mcufw_step0_sys_<suite>.log`、
`/tmp/mcufw_step0_esh_<suite>.log`、`/tmp/mcufw_step0_par1.log`、
`/tmp/mcufw_step0_par2.log`に保存した。一時logなので永続的な参照先とはしない。

既存esh buildにはunused、format、strncpy等の警告がある。
pipelineとpipeline_execには異常入力に対するlex/parser診断も出力されるが、
各scriptは成功している。今回それらのsource修正は行っていない。
非対話shell起動時のstty警告は試験終了コードへ影響していない。

未実行: Windows main.exeのbuildとsam1/sam2/sam3実行、STM32実機試験、
新規RAM試験。Windows統合はStep 6、新規RAM試験はStep 2以降の担当である。
Step 0は設計と既存試験のbaseline取得を完了した。

### 次Stepへの引継ぎ

次はStep 1。Section 11を確定仕様として実装する。
makedevの範囲検査、getblk用のdev検査とI/O提出用検査の区別、
受付拒否時の状態不変、SYS errorの既存値維持が中心となる。
Step 0完了時点で阻害要因はない。遅延書込みと非同期driverの追加契約は後続範囲。

## 13. Step 1実施記録

実施日: 2026-10-17

開始HEAD: `3c50a9e Document RAM block device plan and Step 0 baseline`

branch: `feature/mrunsv`

作業状態: 実装と関連回帰試験完了。本書を含むStep 1 commitに記録

### 実装内容

- buf.hのmakedevをstatic inline関数にし、引数の単一評価と8 bit範囲検査を追加
- conf.cへRAM予約用sentinelとmajor 1の未登録entryを追加
- bdevsw_lookup(dev)で符号化、major範囲、d_tabを共通検査
- bio.cのcache取得とincoreもbdevsw_lookupを使用
- bdev_strategy(bp)で所有状態、dev、strategy登録、全登録queueを検査して提出
- 不正queueはEINVAL、既存queue内の二重提出はEAGAINで拒否し、完了させない
- queue走査をNBUFで制限し、循環、head/tail不整合、active重複等を拒否
- SYS_ENODEV=9、SYS_EIO=10を末尾へ追加し、既存error番号を維持

bdevsw_lookupの配置はconf.cとした。既存のbio/confだけをlinkする試験やtargetに
bdev.cへの新たなlink依存を作らず、cache取得と提出で同じ検査を再利用できる。
この関数はstrategyやminorの有効性を検査しない。PIPEDEVとRAM予約entryの
cache取得は可能だが、strategy未登録のI/O提出はENODEVとなる。

bdev_queue_enqueueの既存0/-1契約は変更していない。
bdev_strategyの負のSYS error契約と混同しないこと。
低層APIは内部lockを追加せず、caller直列化というStep 0の前提を維持した。
任意の不正addressを検証する機構はなく、queue pointerは有効なstruct bufを指す
前提で、走査可能なqueue構造の不整合を検出する。

RAM driver、保存配列、raminit、bread/bwrite、NBUF変更は追加していない。

### 変更ファイル

- src/core/sys/h/buf.h
- src/core/sys/h/conf.h
- src/core/sys/h/sys_error.h
- src/core/sys/conf/conf.c
- src/core/sys/dev/bdev.c
- src/core/sys/dev/bio.c
- src/core/sys/test/bio/test_bio.c
- src/core/sys/test/bdev/test_bdev.c
- src/core/sys/test/header/test_headers.c
- src/core/sys/test/header/run_test.sh
- docs/unix_v7_ram_block_device_handoff.md

### 試験内容

変更前: sysのheader、table、bio、bdev、file、pipeを各run_test.shで実行し、
全6 suiteがexit 0。既定scriptのO0/O2構成を使用した。

追加した試験:

- major/minorの全65536組の往復、範囲外と負値、引数の単一評価
- NODEV、上位bit、不正major、NULL d_tabのcache取得拒否
- 同じblock番号の異なるminor間でpayloadが混ざらないこと
- 未登録strategy、NULLと非busy bufferの拒否
- activeとpending双方、他majorのqueueからの二重提出拒否
- queue循環、容量上限、head/tail、非busy要素、active重複の拒否
- 拒否前後でbuffer pool、payload、list head、queueが変化しないこと
- driver受付拒否と、受付後B_ERROR完了の区別
- major 1へのdispatchと、minor 255をdriverまでそのまま渡すこと
- buf.h/conf.hのC/C++単独includeと既存error番号の維持

変更後の結果:

| suite | 構成 | 結果 |
|---|---|---|
| sys/header | C/C++、O0/O2、include順と単独include | exit 0 |
| sys/table | O0/O2 | exit 0 |
| sys/bio | O0/O2、追加dev試験を含む | exit 0 |
| sys/bdev | O0/O2、追加提出試験を含む | exit 0 |
| sys/file | O0/O2 | exit 0 |
| sys/pipe | O0/O2 | exit 0 |
| sys/sched | cortn/thread × O0/O2 | exit 0 |
| esh/pipe | cortn/thread × O0/O2 | exit 0 |
| esh/pipeline_exec | cortn/thread × O0/O2 | exit 0 |

全9 suiteでall tests passedを確認した。bdevはmajor 1とpending二重提出の
追加試験後にもO0/O2で再実行して成功した。
環境はStep 0と同じMSYS2 MINGW64のGCC 16.1.0。
実行は承認済みのsandbox外bash login shell経由とし、MAKEFLAGS=-j4を指定した。
logは一時領域の`/tmp/mcufw_step1_before_<suite>.log`、
`/tmp/mcufw_step1_after_<suite>.log`、
`/tmp/mcufw_step1_after_esh_<suite>.log`に保存した。
既存eshの警告と異常入力に対するparser診断はあるが、scriptはすべて成功した。

git diff --checkとsourceのtab幅8による80 column検査も成功した。
Windows mainのbuild・対話試験、exe/parの再実行、STM32実機試験は未実行。
今回はdev/queueとpipeを直接利用する関連suiteを確認し、全回帰はStep 6で行う。

### 次Stepへの注意事項

Step 2でRAM driverとqueueをmajor 1へ登録する。現在のd_strategy/d_queueはNULL。
RAM_MAJOR等のRAM固有定義はStep 2のramdisk.hで追加する。
全minorは同じentryとsentinelを共有し、有効minor 0/1の判定はdriverで行う。
試験のsetup/teardownでswitch entryを差し替える箇所は、RAM driver登録後に
production entryを正しく保存・復元するよう見直すこと。
conf.cがdriver symbolを参照するStep 2で、関連Makefileのlink対象も同時に更新する。

## 14. Step 2実施記録

実施日: 2026-10-17

開始HEAD: `a2f6171 Add block device validation and request dispatch`

branch: `feature/mrunsv`

作業状態: 実装と関連回帰試験完了。本書を含むStep 2 commitに記録

### 実装内容

- h/ramdisk.hへRAM_MAJOR=1、RAM_NDEV=2、RAM_NBLK=16とAPIを追加
- dev/ramdisk.cへ独立した8192 byteの固定保存領域とramqueueを追加
- conf.cのmajor 1にramstrategy、専用sentinel、ramqueueを登録
- raminitは保存領域だけをclearし、cacheとqueueを操作しない
- ramstrategyはenqueue、start、転送、completeの順で同期処理する
- minorとblock、転送長、payloadを検査してからRAM配列へアクセスする
- 受付後の失敗もB_ERROR/b_error/b_residを設定して一度だけ完了する
- B_ASYNCはiodoneで解放し、driverはcomplete後にbufferを参照しない
- headerのC/C++単独include試験にramdisk.hを追加
- 新規test/ramdiskにO0/O2試験と、失敗時にもcleanするrun_test.shを追加

raw ramstrategyにも二重提出拒否の契約があるため、Step 1の提出前検査を
bdev_request_check(bp)へ切り出した。検査は状態を変更せず0または負のSYS errorを
返す。bdev_strategyとramstrategyが同じ検査を呼び、全登録queueを確認する。
共通entryから呼ぶ場合は検査が二度行われるが、raw呼出しにも同じ所有権検査を
適用する。既存bdev_strategyの受付・拒否契約は変更していない。

RAM queueが他の要求を保持する場合はEAGAINで拒否する。
今回の同期契約では通常その状態にならず、受付要求が未完了のまま返る経路を作らない。
raminitの呼出しはquiescent状態に限定し、その後binitしてから利用を再開する。
Windows eshinitへのraminit接続は計画どおりStep 6へ残した。

### buildと既存試験の変更

conf.cがramstrategy/ramqueueを参照するため、conf.cをlinkする12個のMakefileへ
ramdisk.cと必要なbdev.cを追加した。table試験にもiodoneを提供するbio.cを追加した。
対象はsysのtable/bio/bdev/file/pipe/sched、eshのpipe/sched/pipeline_exec/exeと、
Windows通常/TDM-GCCである。

bio試験はRAM登録済みの期待値へ更新した。
bdev試験はsetupでswitch全entryを保存し、mock実行後に復元する。
RAM entryを恒久的にNULLへ戻す従来の試験前提を解消した。

新規ファイル:

- src/core/sys/h/ramdisk.h
- src/core/sys/dev/ramdisk.c
- src/core/sys/test/ramdisk/Makefile
- src/core/sys/test/ramdisk/run_test.sh
- src/core/sys/test/ramdisk/test_ramdisk.c

既存変更は上記Makefile、conf.c、bdev.c、conf.h、bio/bdev/header試験と本書。
開始前からあった利用者の.gitignoreとAGENTS.mdの変更は編集していない。

### 試験とclean

変更前: sys/header、table、bio、bdev、file、pipeの全6 suiteをO0/O2で実行し、
すべてexit 0。各構成のOUTDIR/TARGETを指定したmake cleanもexit 0。

新規RAM試験の観点:

- 初期RAMのzero、両装置の全blockへのbinary書込みと再読込み
- 全32blockを書いてcacheを追い出し、binit後にも保存内容が残ること
- 同一block番号の装置分離、隣接block分離、先頭と最終block
- minor 2/255、block 16/ULONG_MAX、NULL payload
- 転送長0/1/BSIZE-1/BSIZE+1/size_t最大値の拒否
- 失敗したreadでpayloadを変更せず、失敗したwriteでRAMを変更しないこと
- 古いB_DONE/B_ERROR/B_DELWRIを残した要求の成功時整理
- B_ASYNC成功・失敗後の解放、error/resid、queue空、pool全回収
- raw strategyのNULL/非busy/不正major、同一と他queueへの二重提出拒否
- raminitがcacheを変更せず、続くbinit後にRAMのzeroを読み直せること

sys/bdevのStep 1回帰では拒否時のbuffer/payload/list/queue不変も再確認する。
新規ramdisk/run_test.shは構成ごとのcleanとEXIT trapを実装した。
既存scriptは実行側で各構成をcleanし、script失敗時もcleanする手順を使用した。
試験はMSYS2 MINGW64 login shell経由で実行し、PowerShellからgccを直接起動していない。
一時logは`/tmp/mcufw_step2_before_<suite>.log`と
`/tmp/mcufw_step2_after_<suite>.log`に保存した。

変更後の結果:

| suite | 構成 | 試験とclean |
|---|---|---|
| sys/ramdisk | O0/O2 | ともにexit 0 |
| sys/header | C/C++、O0/O2、単独includeとinclude順 | ともにexit 0 |
| sys/table | O0/O2 | ともにexit 0 |
| sys/bio | O0/O2 | ともにexit 0 |
| sys/bdev | O0/O2 | ともにexit 0 |
| sys/file | O0/O2 | ともにexit 0 |
| sys/pipe | O0/O2 | ともにexit 0 |
| sys/sched | cortn/thread × O0/O2 | ともにexit 0 |
| esh/pipe | cortn/thread × O0/O2 | ともにexit 0 |
| esh/sched | cortn/thread × O0/O2 | ともにexit 0 |
| esh/pipeline_exec | cortn/thread × O0/O2 | ともにexit 0 |
| esh/exe | 既定build、test1〜10とgolden比較 | ともにexit 0 |

全12 suiteが成功した。各構成のmake cleanもすべてexit 0。
生成対象のOUTDIR/TARGET計62 pathを確認し、残存0だった。
以前からある別構成の生成物は今回のclean対象へ混ぜていない。
git diff --check -- docs srcと、変更C/headerのtab幅8での80 column検査も成功した。
既存eshの警告・異常入力に対するparser診断はあるが、試験失敗はない。
Windows mainのbuild・対話試験、TDM-GCC toolchainのbuild、par、STM32実機は
未実行。Windows Makefileのsource追加は反映し、全target検証はStep 6へ残す。

### 次Stepへの注意事項

次はStep 3のbread/bwrite。RAM側は要求ごとにB_DONE/B_ERROR/B_DELWRIを整理するが、
利用側もread/writeのflag、b_bcount、所有権を正しく設定すること。
read成功のbufferはcaller所有、bwriteの同期完了は内部解放という契約を守る。
共通I/O提出はbdev_strategy、dev検査はbdevsw_lookupを使用できる。
raminitを通常のcache取得やbinitから呼ばない。保存内容が消えるためである。
BSIZE=256、NBUF=3、既存pipeの寿命中保持、callerによるkernel lockは維持した。
共存時のNBUF変更はStep 4、遅延書込みはStep 5、起動接続と全回帰はStep 6。

## 15. Step 3実施記録

実施日: 2026-10-17

開始HEAD: `0054ec9 Add synchronous RAM block device driver`

branch: `feature/mrunsv`

作業状態: 実装と関連回帰試験完了。本書を含むStep 3 commitに記録

### V7原実装との対応

AGENTS.mdの「可能な限りオリジナルの実装を踏襲すること」に従い、bundled V7の
usr/sys/dev/bio.cにあるbread、bwrite、iowait、geterrorを確認した。

- breadはgetblkで取得し、B_DONEのcache hitならstrategyを呼ばず返す
- cache missはB_READとBSIZEを設定してstrategyへ提出する
- bwriteはB_READ/B_DONE/B_ERROR/B_DELWRI/B_AGEをclearしてBSIZEを設定する
- 同期writeの完了後はerrorを取り出してからbrelseする
- geterrorはB_ERRORを調べ、b_errorが0ならEIOへ置き換える

上記の処理順と責務を維持した。準備・検査・提出をprepare_read、prepare_write、
sync_device、sync_io、io_errorへ分け、何をする処理か関数名で示した。
エラー解釈はV7名のstatic geterrorを使用する。

MCUFW向けの差分:

- global u.u_errorの代わりにbreadの必須error out引数とbwriteの戻り値を使用
- read失敗時は途中取得bufferを解放してNULLを返す
- busy hitとpool枯渇は待機せずEAGAINを返す
- bwriteのB_ASYNCは所有権違反として拒否し、raw strategyのB_ASYNCは維持する
- trap、spl、sleep/wakeupを追加せず、現在の同期driverだけを扱う

### 同期driverの契約

conf.hのstruct bdevswへd_flags、同期完了を表すBDEV_SYNCを追加し、RAM entryに
BDEV_SYNCを設定した。pipe entryのflagは0で、strategyも未登録のままとした。
これは待機実装がない段階で未完了要求を誤って解放しないためのMCUFW固有の構成情報。
V7のbdevswそのもののfieldではない。

bread/bwriteはstrategy未登録ならENODEV、BDEV_SYNCなしならENOSYSとし、
driverへ提出する前に拒否する。raw bdev_strategyはd_flagsで制限しないため、
既存の非同期mockとqueue試験はそのまま動作する。

BDEV_SYNCは「受付要求をstrategyが戻るまでに完了する」というdriverの保証である。
sync_ioは提出が受付された場合にB_DONEをassertで検査してからgeterrorを呼ぶ。
assertはdriver契約違反の開発時検出であり、非同期I/Oの待機処理ではない。
非同期driverへBDEV_SYNCを設定してはならない。実際の待機・起床・再開は未実装。

### 所有権とエラー処理

dev登録検査と所有権検査を分離するため、既存検査からbdev_buffer_checkを切り出した。
この関数はbusy状態と全登録queueを検査し、状態を変更せず0または負のSYS errorを返す。
bdev_request_checkは従来のdev/strategy検査後に同じ関数を呼ぶ。

bwriteはNULL、非busy、B_ASYNC、queue所属、不正queueを前処理より前に拒否する。
その場合、bufferとpayload、free list、queueを変更せずcallerへ返す。
queue所属はEAGAIN、不正queueとその他の所有権違反はEINVALとする。

所有権が有効なら、V7同様にwrite用flagを整理し、提出拒否またはI/O失敗の場合も
bufferを消費する。errorは解放前に退避し、B_ERRORを設定してcacheを無効化してから
brelseする。未登録/未対応deviceで失敗してもB_DELWRIを残して再利用を妨げない。
callerはbwrite完了後のbufferを再解放・再参照しないこと。

bread成功時は*error=0、callerがB_BUSY bufferを所有する。
失敗時はNULLと負のSYS errorを返し、取得済みbufferを回収する。
error引数NULLの場合は取得もI/Oも行わずNULLを返す。

### 変更ファイル

- src/core/sys/dev/bio.c
- src/core/sys/dev/bdev.c
- src/core/sys/h/buf.h
- src/core/sys/h/conf.h
- src/core/sys/conf/conf.c
- src/core/sys/test/bio/test_bio.c
- docs/unix_v7_ram_block_device_handoff.md

NBUF、pipe、scheduler、RAM保存領域の実装は変更していない。
開始前からあった利用者の.gitignoreとAGENTS.mdの変更は編集していない。

### 試験内容

変更前: sys/header、table、bio、bdev、ramdisk、file、pipeの全7 suiteをO0/O2で
実行し、試験と各構成のmake cleanがすべてexit 0。

bioへ追加した観点:

- 公開bwriteで両装置の全32blockを書き、binit後にbreadで全byteを検証
- cache hitではread strategyを再実行しないこと
- breadによる部分更新後、bwriteとcache破棄を経ても未更新byteを維持すること
- strategyへ渡すread/write flag、転送長、error/residの初期化
- 容量外、巨大block、不正minor、NULL error引数、未登録/不正dev
- B_ERRORの具体的errorとb_error=0時のEIO fallback
- driver受付拒否とI/O失敗時の回収、cache無効化、失敗後の再読込み
- BDEV_SYNCなしではcallbackを呼ばずENOSYSを返すこと
- busy hit、全buffer保持、dirty候補によるEAGAINと状態維持
- bwriteのB_ASYNC、非busy、pending/active、他queue、不正queueの拒否
- 所有権拒否でbuffer/payload/free listが変化しないこと
- 有効所有bufferの失敗時は解放され、poolとqueueが回収されること

変更後の結果:

| suite | 構成 | 試験とclean |
|---|---|---|
| sys/bio | O0/O2、新規I/O試験を含む | ともにexit 0 |
| sys/header | C/C++、O0/O2、単独includeとinclude順 | ともにexit 0 |
| sys/bdev | O0/O2 | ともにexit 0 |
| sys/ramdisk | O0/O2 | ともにexit 0 |
| sys/table | O0/O2 | ともにexit 0 |
| sys/file | O0/O2 | ともにexit 0 |
| sys/pipe | O0/O2 | ともにexit 0 |
| sys/sched | cortn/thread × O0/O2 | ともにexit 0 |
| esh/pipe | cortn/thread × O0/O2 | ともにexit 0 |
| esh/sched | cortn/thread × O0/O2 | ともにexit 0 |
| esh/pipeline_exec | cortn/thread × O0/O2 | ともにexit 0 |
| esh/exe | 既定build、test1〜10とgolden比較 | ともにexit 0 |

全12 suiteが成功した。bioは書込み前処理の最終調整後にもO0/O2で再実行して成功。
実行はMSYS2 MINGW64 login shell経由とし、PowerShellからgccを直接起動していない。
既存eshの警告と異常入力に対するparser診断はあるが、試験失敗はない。
logは`/tmp/mcufw_step3_before_<suite>.log`、
`/tmp/mcufw_step3_after_<suite>.log`に保存した。

成功・失敗時とも構成を指定してmake cleanする手順を使用し、今回の全cleanがexit 0。
生成対象のOUTDIR/TARGET計62 pathを確認して残存0だった。
git diff --check -- docs srcと、変更C/headerのtab幅8での80 column検査も成功した。
Windows mainのbuild・対話試験、TDM-GCC toolchain、par、STM32実機は未実行。
今回の関連回帰以外の全体確認はStep 6へ残す。

### 次Stepへの注意事項

次はStep 4の資源・排他・pipe共存。現在のAPIはcallerがkernel lockで直列化し、
低層関数は内部でlockせず、sleep/yieldもしない。
最大3本のpipeとI/O用buffer2個の構成に合わせてNBUFの既定値を5へ変更する。
今回のbio試験はNBUFに追従するが、全既存試験の固定値前提も確認すること。

Step 5で遅延書込みを実装すると、dirty候補がEAGAINとなる今回のbaseline試験は
書戻し成功・失敗を検証する試験へ更新する必要がある。
raw B_ASYNCは使用可能だが、公開bwriteの非同期対応はまだない。
BDEV_SYNCだけで非同期driverを動かさず、待機機構を追加してから対応範囲を広げる。

## 16. Step 4実施記録

実施日: 2026-10-17

開始HEAD: `6fc0b54 Add synchronous buffer reads and writes`

branch: `feature/mrunsv`

作業状態: 実装と関連回帰試験完了。本書を含むStep 4 commitに記録

### 資源とlockの確定

buf.hの既定NBUFを3から5へ変更した。BSIZE=256、NINODE=3、PIPELINE_MAX=4は
維持し、最大3本のpipeと、同時保持できるRAM I/O buffer2個の予算とした。
NBUFの既定値は引き続きbuf.hの一箇所だけで定義する。
compile optionによるNBUF=3の上書きも維持し、同一buildでは全TUへ同じ値を渡す。
poolは分割・予約せず、実際に空きがなければgetblkはNULL、breadはEAGAINとなる。

lock境界は既存のcaller方式を維持し、buf.h、conf.h、sched.hで対象を明記した。
kernel lockはcacheのb-list/av-list、B_BUSY、queue、pipe/file/inode、wait条件を保護する。
cache取得からpayloadの参照・更新、提出、解放までcallerが同じlockで保護する。
低層APIは内部でlockしないので、すでにlockを持つcallerから再帰lockしない。
初期化と他の実行主体がない単体試験だけはlock省略可とする。

確認したproductionの呼出し経路:

- esh/exe.cのpipe接続・owner解放・各process close_allはkernel lock区間内
- sys/pipe.cのsys_pipeはlockを取得してpipe_for_procへ接続
- sys/sys2.cのsys_read/sys_write/sys_closeはlock区間内で低層file/pipe処理を呼ぶ
- alloc.cのgetblk、iget.cのbrelseは上記callerのlockを継承する
- Windows esh_adapt.cのbinit/finitはlive資源のない起動時初期化

低層buffer I/Oはcurrent_procを必要とせず、process外のcallerもlockを取得して使える。
本Stepでsleep/wakeup、非同期I/O待機、ISR対応は追加していない。
busy hitや枯渇は即座にEAGAINとし、所有状態を変えず、内部で無期限retryしない。

### V7との対応

bundled V7 usr/sys/dev/bio.cのgetblkで、busy bufferとfree list枯渇時に
sleepして再探索する経路と、splによるlist保護を確認した。
MCUFWでは従来方針どおりcallerのkernel lockで保護し、取得不能を明示的に返す。
buffer/list/queueとpipeの実装本体は変更せず、資源上限と呼出し規約の確認に留めた。

### 共存試験

sys/test/schedへ以下を追加した。

1. process外のcallerがkernel lockを保持して3本のpipeへ異なるmarkerを書く
2. 既定構成では残り2 bufferをRAM I/Oで同時保持する
3. busy hit・全buffer保持・追加pipe生成が失敗しても所有資源が変わらないことを確認
4. RAMへの書込みとcache再利用、I/O失敗を経てもpipe markerを読み戻せることを確認
5. 全bufferをRAM要求で保持した状態のpipe生成失敗と、解放後の生成成功を確認
6. 最大4段・3 pipeのbinary転送中に、各段が同じRAM blockのcounterを更新する
7. EOFまで転送でき、counterが各段の更新回数合計と一致することを確認
8. thread構成ではnative thread2本をeventで同時開始し、各500回counterを更新する

native thread試験はschedulerのrun tokenに依存しない実際の並行実行である。
各threadのcurrent_procはNULLのまま、3本のpipeを保持しながらRAMを更新する。
read-modify-write中にSwitchToThreadを呼び、lockで保護された区間の競合を促す。
2本をjoinしてからcounter=1000、pipe marker、全資源回収を確認する。
native threadを起動するコードはWindows thread試験だけにあり、portable coreへは
追加していない。

RAM strategyの試験wrapperでkernel_is_lockedを検査する。
cache取得・更新・提出・解放でcallerのlockが維持され、二重lockで停止しないことを
実際のthread backendでも確認する。試験終了時にはfile/inode/busy bufferが0で、
b-list/av-listの双方向link、全bufferの単一所属、queue空も確認する。

小容量NBUF=3では、3本のpipeだけでpoolを使い切り、RAM I/OはEAGAINとなることと、
pipe dataと資源の回収を検証する。RAM用2 bufferが必要なpipeline/native並行試験は
この構成では実施しない。

### メモリ予算

Windows MINGW64でsizeofを測定した。O0/O2と両backendで同じ結果だった。

| 項目 | NBUF=5のbyte数 |
|---|---:|
| RAM保存領域 2装置×16block×256 | 8192 |
| cache payload buffers | 1280 |
| buffer header 5×sizeof(struct buf) | 440 |
| sentinel 3×sizeof(struct buf) | 264 |
| RAM queue sizeof(struct bdev_queue) | 32 |
| 上記小計 | 10208 |
| bdevsw 2 entry | 64 |
| nblkdev | 4 |
| sizeofの合計 | 10276 |

sizeof(struct buf)=88。NBUF=3ではpayload=768、header=264であり、
既定値変更による増分は512+176=688 byte。
linker alignment/padding、file/inode table、scheduler、stack、試験専用領域は
上記合計に含まない。STM32の実際のheader幅・配置は移植時に測定する。

### 変更ファイル

- src/core/sys/h/buf.h
- src/core/sys/h/conf.h
- src/core/sys/h/sched.h
- src/core/sys/test/sched/Makefile
- src/core/sys/test/sched/run_test.sh
- src/core/sys/test/sched/test_sched.c
- docs/unix_v7_ram_block_device_handoff.md

開始前からある利用者の.gitignoreとAGENTS.mdの変更は編集していない。
変更前はsys/header/table/bio/bdev/ramdisk/file/pipeの7 suiteをO0/O2で実行し、
試験と各構成のmake cleanがすべてexit 0。

schedのrun_test.shは、既定値とEXTRA_CPPFLAGS=-DNBUF=3の各構成について、
coroutine/thread × O0/O2の計8構成を実行する。
各構成終了時のmake cleanと、失敗時にもcleanするEXIT trapを追加した。
試験はMSYS2 MINGW64 login shell経由で実行し、PowerShellからgccを直接起動しない。
一時logは`/tmp/mcufw_step4_before_<suite>.log`と
`/tmp/mcufw_step4_after_<suite>.log`へ保存した。

変更後の結果:

| suite | 構成 | 試験とclean |
|---|---|---|
| sys/sched | NBUF=5/3 × cortn/thread × O0/O2、計8構成 | ともにexit 0 |
| sys/header | C/C++、O0/O2、単独includeとinclude順 | ともにexit 0 |
| sys/table | O0/O2 | ともにexit 0 |
| sys/bio | O0/O2 | ともにexit 0 |
| sys/bdev | O0/O2 | ともにexit 0 |
| sys/ramdisk | O0/O2 | ともにexit 0 |
| sys/file | O0/O2 | ともにexit 0 |
| sys/pipe | O0/O2 | ともにexit 0 |
| esh/pipe | cortn/thread × O0/O2 | ともにexit 0 |
| esh/sched | cortn/thread × O0/O2 | ともにexit 0 |
| esh/pipeline_exec | cortn/thread × O0/O2 | ともにexit 0 |
| esh/exe | 既定build、test1〜10とgolden比較 | ともにexit 0 |

全12 suiteで成功し、sys/schedでは8回のall tests passedを確認した。
全構成のmake cleanが成功し、生成対象のOUTDIR/TARGET計70 pathの残存は0。
既存eshの警告と異常入力に対するparser診断はあるが、試験失敗はない。
git diff --check -- docs srcと、変更C/headerのtab幅8での80 column検査も成功した。
Windows mainのbuild・対話試験、TDM-GCC toolchain、par、STM32実機は未実行。
今回の関連回帰以外の全体確認はStep 6へ残す。

### 次Stepへの注意事項

次はStep 5の遅延書込みと書戻し。dirty bufferの保持・再利用、flushの完了保証、
書込み失敗後の保持と通知方法をStep開始時に確定する。
V7のbdwrite/bawrite/getblk/geteblk/bflushをまとめて確認してから実装すること。
現在の公開bwriteは同期専用で、B_ASYNCは拒否する。raw strategyのB_ASYNCは有効。
失敗dirty bufferをB_ERRORのまま通常brelseへ渡すとdev関連が失われるため、
dirty dataの保持方針とerror後処理を一組で設計する。
本Stepのlock区間を維持し、遅延書込みを導入しても未完了要求を解放しないこと。

## 17. Step 5 実施記録

開始HEAD: `4de9278`

作業状態: 実装と関連回帰試験完了。本書を含むStep 5 commitに記録

### V7との対応と追加契約

根拠は同梱V7 `usr/sys/dev/bio.c` のbdwrite、bawrite、getblk、geteblk、
bflush、brelse、iodone。bdwriteのB_DELWRI/B_DONE設定と解放、dirty候補を
書戻してから再探索する流れ、devまたはNODEVによるflush対象選択を踏襲した。

V7のbawrite/bflushは非同期提出を行う。MCUFWは既存のBDEV_SYNC契約に限定し、
書込み完了と結果取得を済ませてから解放する。公開bawriteは非同期driverには
未対応であり、B_ASYNCを立てず、同期完了の結果を返す。raw strategyの
B_ASYNC/iodone経路は変更していない。完了後に解放済みbufferを参照しない。

V7のerror時の関連付け破棄はdirtyデータの保持と両立しないため、dirtyについて
保持する契約を追加した。これに伴いbrelseはB_ERRORかつB_DELWRIの場合に
dev/block関連付けを維持する。保存成功後だけdirtyを落として再利用可能にする。

| API | 契約 |
|---|---|
| bdwrite(bp) | 所有bufferをB_DELWRI/B_DONEにして解放。I/Oせず0を返す |
| bawrite(bp) | 同期提出・完了・解放し、0または負のerrorを返す |
| bwrite(bp) | 従来の同期契約を維持。既存dirtyの失敗時は内容と関連付けを保持 |
| bflush(dev) | 指定devのdirtyを保存。NODEVなら全dev。成功0は対象dirtyなしを保証 |
| getblk/geteblk | 空き先頭のdirtyを書戻して再探索。保存前に再関連付けしない |

bdwriteはNULL、非busy、B_ASYNC、queue所属、壊れたqueue、および未対応devを
状態変更なしで拒否する。dev検査は既存の共通登録・同期能力検査であり、minorと
block容量の検証はdriver提出時に行う。bdwrite後にbufferを参照・再解放しない。
bawriteも所有権違反は状態変更なしで拒否する。有効な所有bufferでのI/O失敗は
bufferを解放しつつdirtyとして保持する。ただし提出前のdev共通検査で失敗した
非dirty bufferは、新たにdirtyへ遷移させず無効化して解放する。既存dirtyの場合は
登録や同期能力の問題でも内容と関連付けを保持する。初回bwriteの非dirty I/O失敗は
従来どおり無効化する。再初期化で未保存dirtyを捨てる使い方は禁止する。

書戻し失敗はB_DELWRI/B_DONE/B_ERROR、正のb_errorと未転送b_residを残す。
getblkで同じblockを再取得でき、breadはこのdirty cache内容を返す。その際の
error出力は0であり、過去の保存失敗はb_error/B_ERRORで確認する。readを再提出して
未保存内容を上書きしない。bwrite、bawrite、bflushで再試行できる。

flushはbuffer配列をNBUF回だけ走査し、空いている対象dirtyを各1回書戻す。
flushとcache追出しは内部writeback helperで取得と解放を対応させる。queue不整合等の
事前検査失敗時も、内部で取得したbufferはdirtyのまま空きリストへ戻す。
対象busy dirtyは保持して-SYS_EAGAINを返す。他のbufferの処理は続け、
保存errorをbusy由来のEAGAINより優先する。複数errorは先に観測した非EAGAINを
返す。個別の失敗情報は各dirty bufferにも残る。全てbusyなら待機せず戻る。
PIPEDEVなどstrategyなしのdevを明示指定すると-SYS_ENODEVを返す。

getblk/geteblkの再探索は最大NBUF+1回。書戻しに失敗すると直ちにNULLで戻り、
breadからは-SYS_EAGAINになる。呼出し側はbflushで保存errorを取得して対処できる。
自動retry loop、非同期error callback、background workerは追加していない。
成功した書戻しbufferは通常の空きリスト末尾へ解放するため、全buffer dirty時は
最大NBUF件を保存してから先頭を再利用する。V7の非同期B_AGE操作は採用しない。

全APIとpayload操作は既存どおりcallerのkernel lock区間で行う。
flushのコピー量は最大NBUF*BSIZE、既定値では1280 byte。内部lock、sleep、
heap確保、待機queue、永続ストレージ、構造体memberや固定領域の追加はない。

### 変更ファイルと試験

- `src/core/sys/dev/bio.c`: 遅延書込み、同期書戻し、flush、dirty失敗保持
- `src/core/sys/h/buf.h`: 公開APIと完了・所有権契約
- `src/core/sys/test/bio/test_bio.c`: dirty遷移、全dirty再利用、dev選択、失敗・再試行
- `src/core/sys/test/bio/run_test.sh`: 各構成と失敗終了時のmake clean
- `src/core/sys/test/sched/test_sched.c`: 最大pipe保持中のdirty追出しとflush
- 本書: 進捗、契約と実施結果

専用試験はbdwriteでI/Oしないこと、dirty cache hit、busy dirtyのEAGAIN、
minor別flushと全flush、cache再初期化後の保存内容、全dirtyからのgetblk/geteblk、
driver受付拒否、完了error、dirtyのbwrite失敗と再取得、再試行成功、
所有権違反時の状態不変、双方向listとqueueの不変条件を確認する。
Step 4の容量試験も遅延書込みで実行し、既定NBUF=5で3本のpipeを保持したまま
残り2bufferをdirtyにし、追出し・flush後もRAM内容とpipe markerが一致することを
確認する。同期I/Oのpipeline・native thread試験も維持する。

| suite | 構成 | 結果 |
|---|---|---|
| sys/header | C/C++、通常・逆順include、単独header、O0/O2 | pass |
| sys/table、bio、bdev、ramdisk、file、pipe | 各O0/O2 | pass |
| sys/sched | NBUF=5/3、cortn/thread、O0/O2の8構成 | pass |
| esh/pipe、sched、pipeline_exec | 各cortn/thread、O0/O2 | pass |

全11 suiteで成功。sys/schedはdirty共存を追加した最終版で8構成を再確認した。
試験後は各Makefileのmake cleanを実行し、生成対象66 pathの残存0を確認した。
git diff --checkと変更C/headerのtab幅8・80 column検査も成功した。
試験logはMSYS2の
`/tmp/mcufw_step5_*.log`、dirty共存の最終版は
`/tmp/mcufw_step5_sys_sched_dirty.log`に記録した。
着手前のbaseline再実行は行わず、比較元はStep 4の記録とcommitとした。
Windows mainの起動接続・対話試験と全体回帰はStep 6、STM32実機は後続作業。

### 次Stepへの注意事項

次はStep 6のWindows起動接続と全回帰。raminit/binit/finitの初期化順を接続し、
Windows build・対話コマンド、parser/executorを含む全体回帰を行う。
dirty導入後もbinitはflushを行わないため、再初期化前にbflush(NODEV)==0かつ
全利用者停止を保証する。RAMの揮発性、busy時EAGAIN、BDEV_SYNC限定は維持する。
本Stepは非同期driverの待機・error通知基盤やSTM32実機移植を先取りしない。

## 18. Step 6 実施記録

開始HEAD: `0acf602`

作業状態: Windows統合、対話試験、全体回帰試験完了。本書を含むStep 6 commitに記録

### 起動接続とbuild

Windowsのeshinitをraminit、binit、finit、ttyinit、promptの順にした。
他の利用者が起動する前に保存領域、cache、file/inode tableを初期化する。
同梱V7 `usr/sys/sys/main.c` のmainがbinitをiinit/root取得より先に呼ぶ構成を
参考とし、RAM保存領域の初期化はMCUFW固有の前処理として追加した。

eshinitはcold start専用である。実行中の再初期化やRAM reset commandは追加しない。
quiescent状態で再呼出しする場合も、全file/inode解放、queue空、busy bufferなし、
未保存dirtyなしを事前に保証する。binit単独ではRAM内容を消さない。

Windows MakefileはStep 2からbdev.c/ramdisk.cをlink済みであり、追加のsource登録は
不要だった。MINGW64ではDLLコピー元をshellで使えるforward slashにした。
OPTの既定値は-O0のまま、OPT=-O2で最適化構成も選べるようにした。
backendまたはOPT切替前にはmake cleanする。同じOUTDIRのobjectを流用しない。
cleanは選択TARGETとOUTDIRだけを削除し、無関係なexeと既存runtime DLLは残す。

```sh
cd /c/usr/src/MCUFW/src/target/windows
make clean
make -j4 SCHED_BACKEND=cortn OPT=-O0
./main.exe
# sam1 / sam2 1 10 100 / sam3 -a -b string / echo hello | cat
# Ctrl-Dで終了
make clean
make -j4 SCHED_BACKEND=thread OPT=-O2
./main.exe
make clean
```

GCCはMSYS2 MINGW64 shell経由だけで実行する。PowerShell直起動は行わない。

### 起動関数を含む結合試験

esh/pipeline_execのMakefileに実際のWindows esh_adapt.cを追加した。
test_startup_ramは実際のeshinitを呼び、両minorの最終blockへ異なるpatternを
bdwriteし、flushする。binit単独の後は全byteが保持されることを確認する。
全利用者の停止・全資源解放後にeshinitを再度呼び、両minorの全blockがzeroに
戻ることと、cacheに残った古いpatternを返さないことを確認する。
以降は従来のpipeline/parser/executor結合試験をそのまま実行する。

### API使用例

minor 0のblock内1 byteを更新し、同じdevのdirtyを保存完了までflushする例。
coreの呼出し側がkernel lockを取得し、payloadへのアクセスと解放まで保持する。
部分更新ではgetblkでなくbreadを使い、残りのbyteを維持する。

```c
#include "buf.h"
#include "ramdisk.h"
#include "sched.h"
#include "sys_error.h"

int ram_update_byte(unsigned long block, size_t offset, unsigned char value)
{
	struct buf *bp;
	unsigned int dev = makedev(RAM_MAJOR, 0);
	int error;

	if (offset >= BSIZE)
		return -SYS_EINVAL;
	kernel_lock();
	bp = bread(dev, block, &error);
	if (!bp)
		goto out;
	bp->b_addr[offset] = value;
	error = bdwrite(bp);
	if (error)
		brelse(bp);
	else
		error = bflush(dev);
out:
	kernel_unlock();
	return error;
}
```

bdwriteの成功後はbuffer所有権を失うので、元のbpを参照・解放しない。
同じlock区間で即時に1blockを書きたい場合は、bdwrite/bflushの代わりに
`error = bwrite(bp)`を呼ぶ。書込みAPIの完了後に追加のbrelseを行わない。
flush失敗時は保持されたdirtyを再取得して確認し、bflushで保存を再試行する。
失敗を解消するためのbinit/raminitは未保存内容を捨てるので使用しない。

### メモリと資源上限

MINGW64でsys/schedの実測値を再確認した。Step 4/5から固定領域の追加はない。

| 対象 | byte |
|---|---:|
| RAM保存領域: 2 unit * 16 block * 256 | 8192 |
| cache payload: NBUF=5 * BSIZE=256 | 1280 |
| cache header: 5 * sizeof(struct buf)=88 | 440 |
| 空き/pipe/RAM list sentinel: 3 * 88 | 264 |
| RAM I/O queue | 32 |
| bdevsw: 2 entry | 64 |
| nblkdev | 4 |
| sizeofによる合計 | 10276 |

linker padding、file/inode、scheduler、stack、CRTなどは上記合計に含まない。
Windows main.exeのGNU sizeによる-O2計測は以下。host PEのsection値であり、
STM32のlinker配置・stack使用量や必要RAMを保証する値ではない。

| backend | text | data | bss | 合計 |
|---|---:|---:|---:|---:|
| cortn | 127040 | 960 | 288672 | 416672 |
| thread | 126648 | 976 | 24448 | 152072 |

NINODE=3、NFILE=12、NOFILE=12、PIPELINE_MAX=4を維持する。
最大3本のpipeが3bufferを保持し、既定NBUF=5では残り2bufferをRAM I/Oと共用する。
専用予約poolはない。NBUF=3もbuild可能だが、最大pipe保持中のRAM I/OはEAGAIN。

### 試験結果

着手前にsys/bioのO0/O2 baselineを再実行し、成功とmake cleanを確認した。
最終版の全体回帰は以下。全てMSYS2 MINGW64で実行した。

| suite | 構成 | 結果 |
|---|---|---|
| sys/header | C/C++、include順序と単独include、O0/O2 | pass |
| sys/table、bio、bdev、ramdisk、file、pipe | 各O0/O2 | pass |
| sys/sched | NBUF=5/3、cortn/thread、O0/O2の8構成 | pass |
| esh/pipe、sched、pipeline_exec | 各cortn/thread、O0/O2 | pass |
| esh/cortn、pipeline | 各O0/O2 | pass |
| esh/exe | 既定build、test1〜10とgolden比較 | pass |
| esh/par | run_test1.sh、run_test2.shとgolden比較 | pass |
| Windows main | cortn/thread、O0/O2の4構成 | pass |

Windows mainは4構成とも端末で起動し、# promptを確認してsam1、sam2 1 10 100、
sam3 -a -b string、echo hello | catを実行した。期待する引数・option表示とhelloの
pipe転送を確認し、Ctrl-Dでexit 0終了した。各構成の終了後にmake cleanした。
試験logはMSYS2 `/tmp/mcufw_step6_*.log`に記録した。

legacy suiteではMAKEFLAGSでOUTDIR=_output_step6を指定し、既存_outputを保護した。
parに既存のtest_par.exeがあったため退避し、試験後のclean完了後に復元した。
復元前後のSHA256一致を確認した。
既存Windows DLLのSHA256は試験前後で一致した。

全15 suiteとWindows本体4構成が成功した。各make clean成功後に今回の生成対象
76 path（OUTDIR/TARGET）を確認し、残存0を確認してから既存par実行ファイルを
復元した。既存_outputは変更していない。git diff --checkと、変更Cのtab幅8での
80 column検査も成功した。.gitignoreとAGENTS.mdの既存変更は本Stepに含めない。

変更対象はWindows esh_adapt.c、Windows Makefile、esh/pipeline_execのMakefileと
test_pipeline_exec.c、および本書の5 file。Step 0〜6の完了条件を満たした。

### 完成範囲と制約

devのmajor/minorでRAM blockを指定でき、同期read/write、cache、遅延書込み、
flushと失敗時のdirty保持を利用できる。Windows shell起動へ初期化を接続した。
RAMは揮発性であり、cold startで内容が消える。物理ストレージは使用しない。
BDEV_SYNC driverだけを公開buffer I/O APIの対象とし、真の非同期I/O待機、
background flush、busy buffer待機は未実装。詳細契約はSection 11と17を参照する。
V7ディスクイメージ互換、完全なfilesystem、MMU/trap syscallは実装しない。
STM32実機、Keil、他toolchainの検証は今回の完了条件に含めず、後続作業とする。

## 19. コードレビュー指摘の修正

開始HEAD: `28a835b`

作業状態: 2件の修正、再発防止試験、関連回帰試験完了。本書を含む修正commitに記録

### 書込み対象外devによるcache枯渇

修正前は、geteblkで取得したNODEV bufferをbawriteすると、提出前のENODEVでも
新しいB_DELWRIを付けて解放していた。NBUF個をこの状態にするとflushできない
dirtyだけが残り、正常なRAM I/Oやpipe用のcache取得もできなくなっていた。

write_bufferはdev共通検査をprepare_writeより先に行い、検査に失敗したときは
元々dirtyだったbufferだけを保持対象とする。未dirtyのNODEV、PIPEDEV、strategyが
未登録またはBDEV_SYNCでないdeviceへの要求は、errorを返しつつ無効化・解放する。
bawriteの所有権違反時の状態不変、およびbdwriteのdev拒否時の所有権維持は変えない。
登録を戻せば再試行できる既存dirtyや、提出可能なdeviceでのI/O失敗は引き続き保持する。

### 内部書戻しの事前検査失敗によるbusy残留

修正前はbflushとavailable_bufferがnotavailした後、queue不整合を検出して
bawriteが状態変更なしで戻ると、B_BUSYのbufferが取得者不在のまま残っていた。
queueを修復しても同じblockを取得できず、flushもEAGAINになっていた。

公開APIの事前検査をwrite_buffer_checkへ分け、write_bufferは検査済みの所有bufferを
必ず消費する処理とした。内部writebackは空きbufferを取得し、事前検査で失敗した
場合にもbrelseする。bflushとgetblk/geteblkのdirty追出しはこのhelperへ統一した。
検査失敗だけではpayload、dev/block、dirty/DONE、保存error情報を変更しない。
再度取得できたbufferは、queue修復後に通常のflushで保存できる。

### 再発防止試験

sys/bioへ以下を追加した。修正前は追加試験が失敗し、修正後はO0/O2で成功した。

- NODEV、PIPEDEV、strategyなし、同期能力なしへのbawriteを各NBUF*2回試行
- 各失敗後に新規dirtyがなく、全NBUF個と正常RAM readを利用できること
- 登録不備によるbawrite失敗でも、既存dirtyの全byteとdev/blockが維持されること
- 登録復元後のflushとcache再初期化後のreadで、保存済みpatternを確認
- RAM queueと別majorの登録queueの不整合を、それぞれ注入
- dev指定flush、全dev flush、geteblk、getblkによる追出しの4経路で拒否を確認
- 拒否後の空きlist、busy数、device list、payloadとdriver未提出を確認
- queue修復後の同じblockの再取得、flushと全byteの保存内容を確認

変更対象はdev/bio.c、h/buf.h、test/bio/test_bio.cと本書。

### 検証結果

| suite | 構成 | 結果 |
|---|---|---|
| sys/bio | 新規14 caseを含む全試験、O0/O2 | pass |
| sys/header | C/C++、include順序と単独include、O0/O2 | pass |
| sys/bdev、ramdisk、file、pipe | 各O0/O2 | pass |
| sys/sched | NBUF=5/3、cortn/thread、O0/O2の8構成 | pass |
| esh/pipeline_exec | 実際の起動関数を含むcortn/thread、O0/O2の4構成 | pass |

関連8 suiteで成功した。今回の変更箇所に対する回帰範囲とし、Step 6で実施済みの
Windows本体の対話試験、legacy parser/executor等の全体回帰は繰り返していない。
各make clean後に生成対象46 pathの残存0を確認した。git diff --checkと変更C/headerの
tab幅8・80 column検査も成功した。.gitignoreとAGENTS.mdの既存変更は含めない。
修正前の失敗はMSYS2 `/tmp/mcufw_ram_fix_before.log`、修正後の試験は
`/tmp/mcufw_ram_fix_after_*.log`に記録した。
