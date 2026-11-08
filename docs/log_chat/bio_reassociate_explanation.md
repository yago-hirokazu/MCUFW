# `bio.c`の`reassociate()`処理

作成日: 2026-10-12

対象:
`src/core/sys/dev/bio.c`の`reassociate()`

## 質問

`/c/usr/src/MCUFW/src/core/sys/dev/bio.c`の`reassociate()`は、
どのような処理を行っているか。

## 概要

`reassociate()`は、buffer headerを以前のdevice/blockから切り離し、
新しいdevice/blockへ割り当て直す内部関数である。

```c
static void reassociate(struct buf *bp, struct buf *head,
			unsigned int dev, unsigned long blkno)
{
	b_list_remove(bp);
	b_list_insert(head, bp);
	bp->b_flags = B_BUSY;
	bp->b_dev = dev;
	bp->b_blkno = blkno;
	bp->b_bcount = 0;
	bp->b_resid = 0;
	bp->b_error = 0;
	bp->b_io_next = NULL;
}
```

この関数は、次の2か所から呼ばれる。

- `getblk()`: bufferを指定したdevice/blockへ関連付ける
- `geteblk()`: bufferをdevice未関連の空bufferへ戻す

## 前提状態

呼び出し元は、`reassociate()`を呼ぶ前に`notavail()`を実行する。

```text
notavail(bp)
  -> av-listからbpを外す
  -> B_BUSYを設定する
  -> reassociate(bp, ...)
```

したがって、`reassociate()`へ渡されるbufferは次の状態にある。

- 以前のb-listには所属している
- av-listには所属していない
- `B_BUSY`が設定されている

## 処理内容

### 1. 以前のb-listから外す

```c
b_list_remove(bp);
```

bufferは再利用前でも、必ずいずれかのb-listに所属している。

- device未関連なら`bfreelist`のb-list
- pipe blockならpipe deviceのb-list
- 将来block deviceを追加した場合は、そのdeviceのb-list

`b_list_remove()`は`b_forw`と`b_back`をつなぎ替え、`bp`を以前の
b-listから外す。

### 2. 新しいb-listへ挿入する

```c
b_list_insert(head, bp);
```

`head`は移動先b-listのsentinelである。

| 呼び出し元 | `head` |
|---|---|
| `getblk()` | `bdevsw[major(dev)].d_tab` |
| `geteblk()` | `&bfreelist` |

例えば、未関連bufferをpipe blockへ割り当てると、所属は次のように
変化する。

```text
変更前:
NODEV b-list -> bp(dev=NODEV)

変更後:
pipe b-list  -> bp(dev=PIPEDEV, blkno=2)
```

### 3. bufferをbusy状態へ初期化する

```c
bp->b_flags = B_BUSY;
```

以前のblockに対する状態flagを破棄し、新しい割り当てを呼び出し元が
占有している状態で開始する。

処理後のlist所属とbusy状態は次のとおりである。

```text
b-list:  新しいdevice listに所属
av-list: 所属しない
B_BUSY:  設定済み
```

### 4. 新しいcache keyを設定する

```c
bp->b_dev = dev;
bp->b_blkno = blkno;
```

buffer cacheは`(b_dev, b_blkno)`の組をbufferのcache keyとして使用する。

例えば次の呼び出しでは、

```c
getblk(PIPEDEV, 2);
```

bufferは次のkeyへ関連付けられる。

```text
b_dev   = PIPEDEV
b_blkno = 2
```

以後、同じkeyに対する`getblk()`は、このbufferをcache hitとして
見つける。

### 5. 以前のI/O状態を初期化する

```c
bp->b_bcount = 0;
bp->b_resid = 0;
bp->b_error = 0;
bp->b_io_next = NULL;
```

各fieldの意味は次のとおりである。

| field | 初期化の目的 |
|---|---|
| `b_bcount` | 転送予定byte数を未設定へ戻す |
| `b_resid` | 未転送byte数を0へ戻す |
| `b_error` | 以前のI/O errorを消す |
| `b_io_next` | 以前のdevice I/O queueとの関連を消す |

`b_io_next`を明示的にクリアすることで、新しい割り当てが以前のI/O
queue状態を引き継がないようにする。

## 初期化しないもの

`reassociate()`は`b_addr`とpayloadを変更しない。

```text
b_addr      -> 同じbuffers[i]
payload内容 -> そのまま
```

payloadを0で初期化する必要がある場合は、呼び出し元が`clrbuf()`を使う。
buffer cacheでは、blockへの再関連付けとpayloadの初期化を別操作として
扱う。

## UNIX V7との関係

UNIX V7には`reassociate()`という独立した関数はない。V7の`getblk()`と
`geteblk()`内に直接記述されていた次の処理を、MCUFW版で共通helperへ
抽出したものである。

```text
旧b-listから削除
  -> 新b-listへ挿入
  -> B_BUSY設定
  -> b_dev、b_blkno更新
```

MCUFW版ではさらに、将来の明示的device queueに備えて`b_bcount`、
`b_resid`、`b_error`、`b_io_next`も初期化している。

## 処理後のinvariant

`reassociate()`終了後は、次の条件が成立する。

- bufferは新しいb-listへ一度だけ所属する
- bufferはav-listへ所属しない
- `B_BUSY`が設定されている
- `(b_dev, b_blkno)`が新しいcache keyを表す
- device I/O queueには所属しない
- payload領域と`b_addr`は変更されない

