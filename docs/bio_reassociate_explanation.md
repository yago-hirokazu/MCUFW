# bio.cのbuffer再割当て処理

作成日: 2026-10-12
更新日: 2026-10-22

対象: `src/core/sys/dev/bio.c`の`assign_buffer_to_block()`。
旧名は`reassociate()`。文書ファイル名は既存の参照先として維持する。

## 呼出し関係

```text
getblk() / geteblk()
  -> notavail(): av-listから外しB_BUSYを設定
  -> dirty候補ならbawrite()して探索を再開
  -> assign_buffer_to_block(): b-listとcache keyを変更
```

`getblk()`のcache hitでは再割当てしない。busy hitまたは空きなしならNULLを返す。
呼出元はkernel lockを保持する。停止状態の初期化・単独試験は例外とする。

## 現行実装

```c
static void assign_buffer_to_block(struct buf *bp, struct buf *head,
                                   unsigned int dev, unsigned long blkno)
{
	b_list_remove(bp);
	b_list_insert(head, bp);
	bp->b_flags = B_BUSY;
	bp->b_dev = dev;
	bp->b_blkno = blkno;
}
```

1. `b_list_remove()`で旧deviceのb-listから外す
2. `b_list_insert()`で新しいsentinelの直後へ挿入する
3. flagを`B_BUSY`だけにし、新しい割当ての所有を示す
4. `(b_dev, b_blkno)`を新しいcache keyにする

| 呼出し元 | 移動先sentinel | key |
|---|---|---|
| `getblk(dev, blkno)` | `bdevsw[major(dev)].d_tab` | 指定device/block |
| `geteblk()` | `&bfreelist` | `NODEV, 0` |

b-listはdevice別のcache検索、av-listは再利用候補の管理に使う。
PIPEDEVはmajor 0、RAM deviceはmajor 1のb-listに属する。
`b_io_next`を使うdevice I/O queueは別の構造である。

## 変更しない状態

現行helperは`b_bcount`、`b_resid`、`b_error`、`b_io_next`を初期化しない。
旧実装例にあったこれらの0代入は削除されている。I/O状態の設定は提出・driver・
完了処理が担当する。`b_addr`とpayloadも変更しない。
payloadの消去が必要なら呼出元が`clrbuf()`を使い、`b_resid`もそこで0にする。

終了時は新しいb-listに1回だけ所属し、av-listには所属せず、busyである。
このhelper自体はI/Oを提出せず、待機も行わない。

[RAM block I/O](ram_block_device.md)と[pipe core](pipe_core.md)も参照する。