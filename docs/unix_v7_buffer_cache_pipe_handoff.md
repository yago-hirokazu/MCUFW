# UNIX V7風buffer cacheによるpipe格納方式 実装引継ぎ書

作成日: 2026-10-11
対象リポジトリ: `C:\usr\src\MCUFW`
最終更新時ブランチ: `feature/mrunsv`
最終更新時HEAD: `6e8ac0f add .md files`

## 1. 次タスクの目的

現在のanonymous pipeは、各`struct esh_pipe_inode`が固定長ring bufferを直接
所有している。次タスクではpayload格納先をUNIX V7風buffer cacheへ移し、
将来の通常ファイルと物理block deviceに再利用できる基盤を追加する。

採用するV7由来の中心構造は次のとおり。

- 固定長payload配列`buffers[][]`
- 固定長header配列`buf[]`
- `struct buf`
- device・blockとの対応関係を保持する双方向b-list
- 再利用可能bufferをLRU順に保持する双方向av-list
- `binit()`、`getblk()`、`geteblk()`、`notavail()`、`brelse()`を中心とする
  buffer lifecycle

ただし、V7が`av_forw`と`av_back`をdevice I/O queue用フィールドへ流用する設計は
採用しない。将来のblock device I/O queueは`b_io_next`と
`struct bdev_queue`で明示する。

UNIX V7から採用するportableなsourceとheaderは`src/core/sys/`へ配置する。
既存`src/core/esh/`にはshell、scheduler、既存pipeとの接続処理だけを残す。

最初の実装範囲には、USB memory、Flash、通常ファイル、directory、mount、
free-block管理、実I/Oを含めない。Windows上でbuffer cache単体と既存pipeを
接続し、既存pipeの外部挙動を維持するところまでとする。

## 2. 現在の実装

主なファイルは次のとおり。

| ファイル | 現在の役割 |
|---|---|
| `src/core/esh/include/esh_pipe.h` | pipe inode、ring、I/O結果 |
| `src/core/esh/pipe.c` | pipe生成、read/write、EOF/EPIPE、待機 |
| `src/core/esh/include/esh_file.h` | fd table、file object |
| `src/core/esh/file.c` | file pool、参照数、close |
| `src/core/esh/include/esh_sched.h` | process、wait channel |
| `src/core/esh/sched.c` | sleep/wakeupのportable契約 |
| `src/core/esh/test/pipe/` | pipe単体・資源枯渇試験 |
| `src/core/esh/test/sched/` | 待機・wakeup・競合試験 |
| `src/core/esh/test/pipeline_exec/` | pipeline結合試験 |

現在の所有関係:

```text
struct esh_proc
  -> struct esh_fd_table
       -> struct esh_file
            -> struct esh_pipe_inode
                 -> struct esh_pipe_buffer
                      -> pb_data[256]
```

現在の`struct esh_pipe_buffer`は次を所有する。

```c
unsigned char pb_data[ESH_PIPE_BUFFER_SIZE];
size_t pb_read;
size_t pb_write;
size_t pb_count;
```

制約はpipe最大3本、1本256 byte、pipeline最大4段である。パイプ本体はすでに
Windows coroutine backendとthread backendの両方で動作する。今回の変更は
scheduler、parser、executor、stdio routingの意味を変更しない。

## 3. 維持する既存仕様

buffer格納方式を変更しても、次を維持する。

- `esh_read()`と`esh_write()`の公開signature
- 0 byte操作はfdやpeerを検査せず0
- 空かつwriterありのreadは待機
- 空かつwriterなしのreadはEOFの0
- 満杯かつreaderありのwriteは待機
- readerなしのwriteは`-ESH_EPIPE`
- 最終writer close後も残存dataを読み切ってからEOF
- `esh_write()`は待機を挟んでも正常時は要求全体を転送
- coroutine/threadで同じ結果
- pipeline全体のstatusは最後のcommandのstatus
- 固定長poolを使用し、必須の動的確保を追加しない
- stderrとdebug logをpipeへ混入させない

現在のring動作も最初の移行では維持する。V7のpipeは線形offsetと`i_size`を使い、
readerがwriter位置へ追いついた時点でoffsetとsizeを0へ戻す。これは現在のringと
空き領域の再利用時期が異なるため、V7式offsetへの変更は別の仕様変更として扱う。

## 4. V7ソースで確認した事実

一次資料は次のローカルsourceである。

- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/h/buf.h`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/main.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/dev/bio.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/dev/rk.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/pipe.c`
- `.agents/skills/ancient_unix/references/unix_v7/v7/usr/sys/sys/rdwri.c`

### 4.1 `buffers[][]`と`buf[]`

V7は`main.c`で次のpayload領域を持つ。

```c
char buffers[NBUF][BSIZE+BSLOP];
```

`conf/c.c`はheader poolを持つ。

```c
struct buf buf[NBUF];
struct buf bfreelist;
```

`binit()`は各`buf[i].b_un.b_addr`を`buffers[i]`へ対応付ける。payloadとheaderを
別配列にするため、将来DMA alignmentやtarget別配置属性をpayload側だけへ付けられる。

### 4.2 b-listとav-list

V7の各bufferは通常、二つの双方向listへ関係する。

```text
b_forw / b_back    deviceごとのb-list
av_forw / av_back  再利用可能bufferのav-list
```

主要invariant:

- bufferは常に一つのb-listへ所属する
- `(b_dev, b_blkno)`の検索はdeviceのb-listを走査する
- `B_BUSY == 0`のbufferだけがav-listへ所属する
- `B_BUSY != 0`のbufferはav-listへ所属しない
- `getblk()`はcache hitなら対象bufferをav-listから外す
- cache missならav-list先頭の古いbufferを別blockへ再関連付けする
- `brelse()`は`B_BUSY`を落とし、通常はav-list末尾へ戻す
- `B_AGE`ならav-list先頭へ戻し、早期再利用を促す

V7はhash tableを使用せずdevice listを線形検索する。MCUFWの小さな固定poolでも
最初は同じ方式とし、計測なしにhashを追加しない。

### 4.3 V7 buffer cacheの呼出し経路

read missの基本経路:

```text
bread(dev, blkno)
  -> getblk(dev, blkno)
  -> cache missならB_READを設定
  -> bdevsw[major(dev)].d_strategy(bp)
  -> device queue
  -> hardware completion interrupt
  -> iodone(bp)
  -> iowait(bp)が再開
```

writeの基本経路:

```text
getblk()/bread()
  -> payload更新
  -> bdwrite(bp)                    delayed write
     または bwrite(bp)             immediate write
  -> d_strategy(bp)
  -> device queue
  -> iodone(bp)
  -> brelse(bp)
```

V7の`struct buf`はheaderだけでI/O要求も表す。別のrequest objectは作らない。

### 4.4 V7 pipeとbuffer cache

V7 pipeは専用ring bufferではない。root device上に匿名inodeを割り当て、通常の
inode I/O経路を使用する。

```text
pipe()
  -> ialloc(pipedev)
  -> read側fileとwrite側file
  -> shared inode

readp()/writep()
  -> readi()/writei()
  -> bmap()
  -> bread()/getblk()/bdwrite()
  -> buffer cache
```

V7の`PIPSIZ`は4096 byte、`BSIZE`は512 byteである。これらの値をMCUFWへそのまま
持ち込まない。MCUFWでは現在の256 byte/pipeと固定pool上限を維持する。

### 4.5 V7 device queueのフィールド流用

V7の`buf.h`には次のmacroがある。

```c
#define b_actf   av_forw
#define b_actl   av_back
#define b_active b_bcount
#define b_errcnt b_resid
```

RK driverでは`struct buf rktab`をqueue headerとして使い、request buffer同士を
`bp->av_forw`で連結する。

```text
rkstrategy(bp) -> rktabへenqueue
rkstart()      -> rktab.b_actfを開始
rkintr()       -> 完了requestを外す -> iodone(bp) -> 次を開始
```

これはbusy bufferがav-listに存在しない期間だけ同じpointerを別用途へ使える、
というV7のmemory節約である。MCUFWではfieldの意味が状態により変わる設計を避ける。

## 5. MCUFW向け目標構造

以下は実装提案であり、現時点ではまだsourceへ追加されていない。

### 5.1 配置

`src/core/sys/`には次のsubdirectoryが作成済みである。

```text
src/core/sys/
  conf/
  dev/
  h/
  sys/
```

V7の配置を基準に、最初の実装では次へ置く。

```text
src/core/sys/h/buf.h             struct buf、flag、定数、API宣言
src/core/sys/h/conf.h            struct bdevsw、device table宣言
src/core/sys/dev/bio.c           buffer cache本体
src/core/sys/conf/conf.c         buf[]、bfreelist、device table、sentinel
src/core/sys/test/bio/           buffer cache単体試験
```

将来V7由来のgeneric file/inode/pipe処理を採用する場合は
`src/core/sys/sys/`へ配置する。物理device driverのportableなqueue処理は
`src/core/sys/dev/`、Windows・STM32固有I/Oは各target directoryへ置く。

```text
src/core/sys/h/       V7 usr/sys/h相当の公開header
src/core/sys/dev/     V7 usr/sys/dev相当のbuffer・block device処理
src/core/sys/sys/     V7 usr/sys/sys相当のgeneric kernel処理
src/core/sys/conf/    fixed tableとdevice configuration
```

既存の`src/core/esh/pipe.c`と`src/core/esh/include/esh_pipe.h`は最初の移行では
移動しない。これらは`buf.h`を利用するconsumerとして変更する。source配置は
V7を踏襲するが、historical sourceを無変更でcopyせず、PDP-11依存部を除いた
modern Cとして実装する。

各Makefileには`src/core/sys/h`相当のinclude search pathと
`src/core/sys/dev/bio.c`、`src/core/sys/conf/conf.c`を明示的に追加する。
source内のincludeは既存MCUFW方式に合わせて`#include "buf.h"`、
`#include "conf.h"`とし、V7の`../h/`相対pathはcopyしない。

### 5.2 定数と固定配列

初期値は現在のpipe容量を維持する。

```c
#define BSIZE  256
#define NBUF   3

unsigned char buffers[NBUF][BSIZE];
struct buf buf[NBUF];
struct buf bfreelist;
```

実際のheaderではmacro定義順による循環includeを避けること。望ましい方向は、
buffer cache側に`BSIZE`と`NBUF`を置き、`esh_pipe.h`が`buf.h`をincludeして
`ESH_PIPE_BUFFER_SIZE`を`BSIZE`へ対応付けることである。将来NBUFだけをtarget
configurationで増減できるようにする。

`NBUF >= ESH_PIPE_MAX`はcompile-time checkを入れる。最初のpipe実装はlive pipe
1本につきbufferを1個保持するため、この関係が必要になる。

### 5.3 `struct buf`

V7の名前とlist構造を維持し、PDP-11固有fieldを除く案:

```c
struct buf {
	unsigned int b_flags;
	struct buf *b_forw;
	struct buf *b_back;
	struct buf *av_forw;
	struct buf *av_back;
	struct buf *b_io_next;
	unsigned int b_dev;
	unsigned long b_blkno;
	unsigned char *b_addr;
	size_t b_bcount;
	size_t b_resid;
	int b_error;
};
```

最初に必要なflag:

```c
#define B_WRITE   0x0000U
#define B_READ    0x0001U
#define B_DONE    0x0002U
#define B_ERROR   0x0004U
#define B_BUSY    0x0008U
#define B_WANTED  0x0010U
#define B_AGE     0x0020U
#define B_ASYNC   0x0040U
#define B_DELWRI  0x0080U
```

`b_xmem`、PDP-11 UNIBUS map、physical user I/O、swap用flag、V7 union memberは
追加しない。必要になるまで`struct filsys *`や`struct dinode *`も持たせない。
project規約に従い`typedef`は追加しない。

### 5.4 b-listのhead

V7同様、`struct buf`をsentinelとして使える。

```c
struct buf bfreelist;  /* av-list head兼NODEV b-list head */
static struct buf pipe_tab;
```

`pipe_tab.b_forw/b_back`がpipe pseudo-deviceのb-listを構成する。
`bfreelist.av_forw/av_back`がglobal av-listを構成する。sentinelの未使用fieldを
I/O状態へ流用しない。

pipeごとのblock keyは次を推奨する。

```text
b_dev   = PIPEDEV
b_blkno = esh_pipe_tableのindex
```

同じinode slotを再利用すると同じcache keyになるため、allocation時にpayloadと
ring metadataを必ず初期化する。

### 5.5 pipe inode

payloadだけを`struct buf`へ移し、ring位置はpipe inodeに残す。

```c
struct esh_pipe_inode {
	unsigned char pi_used;
	unsigned char pi_refs;
	unsigned char pi_readers;
	unsigned char pi_writers;
	struct buf *pi_buf;
	size_t pi_read;
	size_t pi_write;
	size_t pi_count;
	struct esh_wait_chan pi_read_chan;
	struct esh_wait_chan pi_write_chan;
};
```

read/writeのpayload参照は`inode->pi_buf->b_addr[index]`へ変更する。
`struct esh_pipe_buffer`とその`pb_data[]`は移行完了後に削除する。

### 5.6 最初のpipe buffer lifecycle

物理storageを持たない最初の段階では、live pipeのdataをcacheからevictできない。
V7のように`B_DELWRI` bufferをav-listへ戻すと、再利用前に書き出す保存先がない。

そのため最初の実装では次の契約を使う。

```text
pipe inode allocation
  -> getblk(PIPEDEV, pipe_index)
  -> B_BUSYのままinode->pi_bufに保持
  -> pipe read/writeは同じbuffer payloadを使用
  -> reader/writerがともに0
  -> ring metadataとpayloadを無効化
  -> brelse(bp)
  -> av-listへ復帰
```

この方式ではlive pipe bufferはb-listには所属するがav-listには所属しない。
これはV7の`B_BUSY` invariantと一致する。違いは、V7の通常I/Oよりbusy期間が長く、
pipe lifetime全体になる点である。

pipe生成途中でfile/fd allocationに失敗した場合も`brelse()`まで巻き戻す。
`esh_pipe_inode_discard()`と最終`esh_pipe_endpoint_close()`の両方にbuffer解放経路が
必要になる。二重解放を避けるため、解放前にlocalへ退避し`pi_buf = NULL`とする。

## 6. 明示的device queue

device queueはblock driverへ提出済みで、まだ完了していない`struct buf`要求の
待ち行列である。b-listやav-listとは別物である。

```c
struct bdev_queue {
	struct buf *dq_head;
	struct buf *dq_tail;
	struct buf *dq_active;
	unsigned int dq_errcnt;
};
```

`struct buf`側は`b_io_next`だけをqueue linkに使う。

```text
dq_head ... dq_tail  開始待ちrequest
dq_active            現在hardware/driverが処理中のrequest
b_io_next             pending queue内の次request
```

状態ごとのlist所属:

| 状態 | b-list | av-list | device queue | `B_BUSY` |
|---|---|---|---|---|
| 再利用可能cache | 所属 | 所属 | なし | 0 |
| callerが保持 | 所属 | なし | なし | 1 |
| I/O開始待ち | 所属 | なし | pending | 1 |
| I/O実行中 | 所属 | なし | active | 1 |
| 完了後caller保持 | 所属 | なし | なし | 1 |
| `brelse()`後 | 所属 | 所属 | なし | 0 |

禁止事項:

- `av_forw`、`av_back`をdevice queueに使わない
- `b_bcount`をcontroller active flagに使わない
- `b_resid`をretry counterに使わない
- 一つのbufferを同時にpendingとactiveへ置かない
- `B_BUSY == 0`のbufferをdevice queueへ置かない

block device switchもV7の`d_strategy`と`d_tab`を保ち、queueだけを明示する。

```c
struct bdevsw {
	int (*d_strategy)(struct buf *bp);
	struct buf *d_tab;
	struct bdev_queue *d_queue;
};
```

`d_tab`はb-listのsentinel、`d_queue`はdriver I/O状態である。initial pipe
pseudo-deviceは`d_tab = &pipe_tab`を持つが、I/Oを行わないため`d_strategy`と
`d_queue`は使用しない。物理deviceでは`d_strategy(bp)`へ渡す時点でbufferは
`B_BUSY`であり、driver completionが最終的に`iodone(bp)`を一度だけ呼ぶ。

最初のpipe-only段階ではdevice I/Oがないため、このqueueを通さない。
queue実装はbuffer cache/pipe移行後の独立milestoneとし、mock asynchronous driverで
enqueue、start、completion、error、retryを試験してからUSB/Flashへ接続する。

## 7. MCUFWでのbuffer cache API方針

V7の関数名は可能な範囲で維持する。

| V7名 | 最初のMCUFW実装 |
|---|---|
| `binit()` | fixed poolと全sentinelを初期化 |
| `getblk()` | device/block検索、cache取得、再関連付け |
| `geteblk()` | device未関連の空buffer取得。単体試験で使用 |
| `notavail()` | av-listから外し`B_BUSY`を設定 |
| `brelse()` | `B_BUSY`解除、av-listへ戻しwaiterを起こす |
| `clrbuf()` | `BSIZE`を0、`b_resid = 0` |
| `incore()` | b-list上のdevice/block存在確認 |
| `bread()` | block device導入まで未実装または`-ESH_ENOSYS` |
| `bwrite()` | block device導入まで未実装または`-ESH_ENOSYS` |
| `iowait()` | asynchronous device導入時に接続 |
| `iodone()` | asynchronous device導入時に接続 |

最初の公開signature案:

```c
void binit(void);
struct buf *getblk(unsigned int dev, unsigned long blkno);
struct buf *geteblk(void);
void brelse(struct buf *bp);
int incore(unsigned int dev, unsigned long blkno);
void clrbuf(struct buf *bp);
```

`notavail()`と個別list操作helperは`bio.c`の`static`関数とする。最初の
`getblk()`は取得不能時に`NULL`を返す。pipe生成側はこれを生成失敗へ変換し、
構築済みinode/file/fdを逆順に戻す。

V7の`getblk()`はbusy hitやfree buffer枯渇時に`sleep()`する。最初のpipe生成は
process実行開始前にも呼ばれるため、bufferがなければ待機せず生成失敗へ戻す。
将来のfilesystem用blocking取得は、次を満たす別wrapperまたは明示result APIを
追加する。

- condition検査とwait登録を同一lock区間で行う
- coroutine backendではpending/resumeする
- thread backendでは同じobservable resultを返す
- wakeup後にconditionを再検査する
- global `u.u_error`を使わずreturn値またはout parameterでerrorを返す

`esh_file_system_init()`は所有中資源がない状態で`binit()`も呼ぶ。既存同様、
live process/file/pipeがある状態でpool全体を再初期化してはならない。

## 8. 実装順序

### Step 0: baseline確認

変更前に少なくとも次をMSYS2 MINGW64 shell経由で実行する。

```sh
cd /c/usr/src/MCUFW/src/core/esh/test/pipe
bash ./run_test.sh
cd ../sched
bash ./run_test.sh
cd ../pipeline_exec
bash ./run_test.sh
```

`run_test.sh`はcoroutine/thread、O0/O2の組合せを持つ。PowerShellから
MinGW gccを直接起動しない。

### Step 1: buffer cache単体

1. `src/core/sys/h/buf.h`と`src/core/sys/h/conf.h`を追加
2. `src/core/sys/dev/bio.c`を追加
3. `src/core/sys/conf/conf.c`へ`buf[]`、`bfreelist`、`pipe_tab`、
   initial device tableを追加
4. `bio.c`へ`buffers[]`、`binit()`、list helper、`notavail()`、
   `getblk()`、`geteblk()`、
   `brelse()`、`incore()`、`clrbuf()`を実装
5. `src/core/sys/test/bio/`でlist invariantとLRU順を検査

最初から`src/core/esh/pipe.c`へ埋め込まず、`src/core/sys`だけでlist操作を
単体試験できるようにする。

### Step 2: pipe allocationへ接続

1. `esh_file_system_init()`から`binit()`を呼ぶ
2. ESH関連Makefileへ`src/core/sys/h`とsys sourceを追加
3. `esh_pipe.h`から`buf.h`をincludeする
4. `esh_pipe_inode_alloc()`でpipe table indexを求める
5. `getblk(PIPEDEV, index)`でbufferを取得
6. buffer取得失敗ならinode allocationを完全に巻き戻す
7. `pi_buf`へbufferを所有させる
8. discardと最終closeで`brelse()`する

### Step 3: payload移行

1. `pb_read/pb_write/pb_count`を`pi_read/pi_write/pi_count`へ移す
2. `pb_data[]`参照を`pi_buf->b_addr[]`へ移す
3. wrap、partial read/write、満杯・空判定を維持
4. `struct esh_pipe_buffer`を削除
5. `ESH_PIPE_BUFFER_SIZE`利用箇所を互換macroまたは`BSIZE`へ整理

### Step 4: 回帰・failure cleanup

既存suiteに加え、次を確認する。

- buffer cache初期化後、全NBUFがb-listとav-listへ一度ずつ所属
- 同じ`(dev, blkno)`のcache hitで同じ`struct buf *`を返す
- busy bufferはav-listに存在しない
- `brelse()`後だけav-listへ戻る
- cache miss時にav-list先頭が再関連付けされる
- 3本のpipeがそれぞれ異なるbufferを所有
- pipe poolまたはbuffer pool枯渇時に部分構築資源が残らない
- fd/file allocation失敗時にbuffer使用数も元へ戻る
- close順序がread先行・write先行の両方でもbufferを一度だけ返す
- 0/1/256/257/2089 byteの既存転送が両backend、O0/O2で成功
- EOF、EPIPE、早期consumer終了、binary/NUL転送が不変
- 各test終了時にfile、pipe、busy buffer使用数が0

### Step 5: device queue単体

pipe移行と分けて次を実装する。

1. `src/core/sys/h/buf.h`へ`b_io_next`を定義
2. `src/core/sys/h/conf.h`へ`struct bdev_queue`を定義
3. `src/core/sys/dev/`へFIFO helperを実装
4. `b_io_next`によるpending queue
5. `dq_active`へのstart遷移
6. mock completionから`iodone()`
7. error/retry上限
8. empty、1件、複数件、完了順、二重enqueue拒否の試験

### Step 6: 将来の物理block device

- Windows USB: OS I/O adapterをtarget層へ置く
- STM32外部Flash: read/program/erase state machineをtarget層へ置く
- STM32内部Flash: erase単位、program alignment、実行中code領域との競合を扱う
- portableなblock queueと共通driver契約は`src/core/sys/dev/`へ置く
- device構成とswitch tableは`src/core/sys/conf/`へ置く
- logical block requestは`struct buf`のままdriver queueへ渡す
- erase/program分割やwear levelingはFlash translation層内部の別状態として扱う
- hardware/OS completionは`iodone(bp)`へ収束させる

raw Flashは上書き可能なdisk blockではない。buffer cacheより下にtranslation層を
置き、erase-before-write、power-loss、bad block、wear levelingをbuffer cacheへ
漏らさない設計判断が別途必要になる。

## 9. 実装時の注意点

- `b_forw/b_back`と`av_forw/av_back`を操作するhelperを分ける
- sentinelを含むcircular listの空条件は`head->next == head`
- removal前に対象がそのlistへ所属することをdebug buildで検査できるとよい
- `B_BUSY`変更とav-list操作を同じcritical sectionで行う
- 現在は`esh_kernel_lock()`がpipe状態を保護する。ISR対応時はthread lockを
  そのままISRから呼ばず、target別critical section契約を設計する
- `binit()`による全消去はlive ownerがない場合だけ許可する
- `memset(struct buf, 0, ...)`でlist linkを破壊しない
- bufferをreleaseした後に`pi_buf`へ触れない
- `getblk()`が返したbufferは必ず一度だけ`brelse()`する
- 既存の未追跡fileやbuild出力を削除・一括追加しない
- coding ruleに従い、新しい`typedef`を使わない
- C sourceのcommentは冠詞を省いた電報style、一文ならperiodなし、80 column以内

## 10. 今回意図的に実装しないもの

- V7 disk filesystemの完全再現
- superblock、directory、mount、link、permission
- disk inode形式とon-disk互換性
- `bmap()`のdirect/indirect block mapping
- delayed writeの実I/O
- read ahead
- raw/physical I/Oとswap
- signalとしての`SIGPIPE`
- V7のglobal `u`、trap system call、MMU copyin/copyout
- device queue field alias
- 回転disk向けcylinder sort

これらの省略後も、buffer association、busy/available状態、二つの双方向list、
fixed pool、将来のstrategy/iodone境界は保つ。

## 11. 完了条件

最初の実装taskは次をすべて満たした時点で完了とする。

1. pipe payloadが`struct esh_pipe_buffer.pb_data`ではなく`buffers[][]`に存在
2. payloadを指すheaderが`buf[]`の`struct buf`
3. V7由来source/headerが`src/core/sys/`に配置されている
4. `src/core/esh/`にはsys実装を重複して置いていない
5. 各bufferがb-listへ常時所属
6. 非busy bufferだけがav-listへ所属
7. live pipe bufferが`B_BUSY`で保持され、最終closeでav-listへ戻る
8. `av_forw/av_back`をI/O queueへ流用していない
9. 生成途中失敗とcloseでresource leak、double releaseなし
10. buffer cache単体試験が成功
11. 既存pipe/scheduler/pipeline suiteが両backend、O0/O2で成功
12. 物理I/O未実装部分がstubまたは未公開で明確
13. 変更文書に実測した試験結果と未確認事項を記録

## 12. 作業ツリーの注意

今回変更した追跡済みfileは本書だけであり、変更はコミットしていない。
`src/core/sys/conf/`、`dev/`、`h/`、`sys/`は作成済みだが現在は空である。
Gitは空directoryを追跡しないため、実装taskでsource/headerを追加した時点から
追跡対象になる。

多数の既存未追跡fileがあるため、`git add .`、一括clean、未追跡file削除を
行わない。特に`.agents/`、coding style資料、STM32 tree、test生成物、`.bak`、
Windows DLL/exeは別作業由来である。

## 13. 新しいタスクへ渡す依頼文

> `C:\usr\src\MCUFW\docs\unix_v7_buffer_cache_pipe_handoff.md`を読み、
> Step 0からStep 4までを実装してください。UNIX V7由来の`buffers[][]`、
> `struct buf`、b-list、av-list、`binit/getblk/brelse`のinvariantを維持し、
> V7から採用するsource/headerは`src/core/sys/`の`conf/`、`dev/`、`h/`、
> `sys/`へ役割別に配置してください。buffer cacheは`h/buf.h`、`h/conf.h`、
> `dev/bio.c`、`conf/conf.c`へ分け、`src/core/esh/`には接続変更だけを残してください。
> 現在のring型pipeの外部挙動、coroutine/thread両backendを変更しないでください。
> device queueでは`av_forw/av_back`を流用せず、Step 5以降は今回未着手として
> 残してください。まずAGENTS.md、現在のbranch/HEAD、dirty tree、baseline試験を
> 確認し、既存未追跡fileを変更・削除・一括追加しないでください。
