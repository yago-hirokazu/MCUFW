# RAMブロックデバイスとbuffer I/O

更新日: 2026-10-24

## 配置と資源

現行ソースは`src/core/sys/dev/bio.c`、`bdev.c`、`ramdisk.c`と
`src/core/sys/conf/conf.c`。公開宣言は`src/core/sys/h/buf.h`、`conf.h`、
`ramdisk.h`、`user.h`にある。

| 項目 | 既定値・役割 |
|---|---|
| `BSIZE` | 256 byte |
| `NBUF` | 5（pipeとRAM I/Oで共有） |
| `PIPEDEV` | major 0、cache用sentinelのみ。strategyはNULL |
| `RAM_MAJOR` | 1、`ramstrategy`と`BDEV_SYNC`を登録 |
| `RAM_NDEV` | 2（minor 0、1） |
| `RAM_NBLK` | 16 block/device |
| RAM payload | 2 × 16 × 256 = 8192 byte |
| cache payload | 5 × 256 = 1280 byte |

上記byte数は管理構造体やstackを含まない。物理storageと通常ファイルは扱わない。
device I/O queueは`struct bdev_io_queue`と`b_io_next`を使い、cacheのb-list、
av-listとは分離する。RAM driverは提出された要求を関数復帰までに完了する。

## 初期化と所有権

Windowsの`src/core/esh/target/windows/esh_adapt.c`にある`eshinit()`は
`raminit(); binit(); finit();`の順に初期化する。`raminit()`はRAM内容を消去し、
`binit()`はcache・device list・queue、`finit()`はfile・inode tableを初期化する。
使用中のbuffer、fd、実行中processがある状態では再初期化しない。
RAM内容を保持してcacheだけ再構築する試験では`raminit()`を呼ばない。

cache、queue、所有中payloadの操作は呼出元がkernel lockで保護する。
初期化と単独実行の試験は停止状態で行う。取得したbusy bufferは呼出元が所有し、
`brelse()`、`bwrite()`、`bdwrite()`、`bawrite()`へ渡した後は使用しない。

## 現行APIとerror

| API | 動作と結果 |
|---|---|
| `getblk(dev, blkno)` | cache hitまたは再割当て。busy hit・枯渇はNULL |
| `geteblk()` | device未関連のbusy bufferを取得。枯渇はNULL |
| `bread(dev, blkno)` | busy bufferを返す。取得不能はNULLと`SYS_EAGAIN` |
| `bwrite(bp)` | blockを書き込み、bufferを解放。戻り値なし |
| `bdwrite(bp)` | `B_DELWRI\|B_DONE`を設定して解放。提出せず、戻り値なし |
| `bawrite(bp)` | `B_ASYNC`を設定して書込み。戻り値なし |
| `bflush(dev)` | availableなdirty bufferを提出。busyは除外、戻り値なし |
| `geterror(bp)` | `B_ERROR`なら正のerrorを`u.u_error`へ設定 |

`bread()`に`int *error`引数はない。driver strategyも戻り値を返さず、
`B_ERROR`、`b_error`、`b_resid`、`iodone()`で結果と完了を通知する。
呼出元は操作前に`u.u_error = 0`とする。成功は以前のerrorを消さず、
`B_ERROR`があり`b_error == 0`なら`geterror()`は`SYS_EIO`を設定する。

`iodone()`はbufferの完了と必要な解放だけを扱い、完了を実行したcontextの
`u_error`を変更しない。要求元が自身のcurrent userで`geterror()`を呼び、
bufferのerrorを取り込む。現行のbread/bwriteは同期strategyの復帰後に収集する。
別contextでの完了試験はこの分離を確認するもので、非同期driverの追加ではない。
I/O失敗でも`bread()`が非NULLのbusy bufferを返した場合は必ず解放する。

`u`は`(*current_user())`マクロであり、current procの`p_addr`から専用userを
選ぶ。currentなしではbackendのbase userを使う。pipe APIの転送量・負のerrorと
buffer I/Oの正の`u_error`は異なる契約である。

`bio.c`の`bread()`/`bwrite()`は`bdevsw[major(dev)].d_strategy`を直接呼ぶ。
有効なdevice tableと同期strategyは呼出元の前提であり、NULL strategyの
`PIPEDEV`へblock I/Oを提出しない。未登録majorの安全な拒否や非同期driverの
自動判定をbio入口へ期待しない。`bdev_strategy()`には別途提出検査がある。

## 遅延書込みと再利用

`getblk()`と`geteblk()`はdirtyな再利用候補を`bawrite()`へ渡してから探索し直す。
`bflush(NODEV)`は全device、指定devならそのdeviceのavailable dirty bufferを
対象にする。実際の非同期I/O待機、busy buffer待機、background flushは未実装。
`B_ASYNC`は同期driverの完了時解放に使用し、復帰後も転送が続く意味ではない。

error付きbufferの`brelse()`は`b_dev`を`NODEV`にしてcache keyを無効化する。
書戻し失敗時のdirty内容保持や再試行を保証する旧契約は現行bioにはない。
遅延・async書込みの全errorを`bflush()`の戻り値で集約することもできない。

## 使用例

次は有効なRAM deviceのblock 0を読み、先頭byteを変更して書き戻す例。
初期化済み、kernel lock未取得の文脈で使用する。

```c
#include "buf.h"
#include "ramdisk.h"
#include "sched.h"
#include "user.h"

struct buf *bp;
int error;

kernel_lock();
u.u_error = 0;
bp = bread(makedev(RAM_MAJOR, 0), 0);
if (bp) {
	if (!u.u_error) {
		bp->b_addr[0] = 0x5a;
		bwrite(bp);
	} else {
		brelse(bp);
	}
}
error = u.u_error;
kernel_unlock();
```

試験入口は`src/core/sys/test/{header,table,bdev,ramdisk,bio}`、共存・user分離は
`src/core/sys/test/sched`と`src/core/esh/test/pipeline_exec`にある。
実行とcleanの注意は[回帰試験](pipe_regression.md)参照。
