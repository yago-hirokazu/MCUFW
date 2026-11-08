# STM32Cube U5 コーディング規約 要約

この文書は、STM32CubeU5 V1.8.0 の `Drivers`、`Projects`、`Utilities` に含まれる
C 言語のソースコードを調査し、MCUFW の STM32 向けコードで採用する規約として
整理したものです。STMicroelectronics が公開した公式規約の転載ではなく、既存コードから
抽出した慣習を、曖昧さが残らないように規則化したものです。

## 適用範囲

- MCUFW の `src/target/NUCLEO-U575ZI-Q/` 以下に新規作成する C/C ヘッダーへ適用する。
- STM32CubeMX が生成するファイルでは、ユーザーが記述する `USER CODE` 範囲へ適用する。
- STM32Cube、CMSIS、RTOS、ミドルウェアなど、外部から取り込んだコードは整形し直さない。
- ベンダーコードへ最小限の修正を加える場合は、そのファイルの既存スタイルを優先する。

調査対象には ST 製コード、Arm CMSIS、生成コード、第三者ミドルウェアが混在しており、
細部は完全には統一されていません。本規約では主に STM32 HAL、LL、BSP、ST 製 Utility と
CubeMX が生成するプロジェクトコードに共通するスタイルを採用します。

## 主要ルール

### 1. インデント

- インデントには空白2文字を使用する。タブは使用しない。
- ブロックを一つ入るごとに2文字深くする。
- `switch` の `case` と `default` は `switch` の本体より2文字、各ラベルの処理は
  さらに2文字深くする。
- 継続行は、対応する式の開始位置にそろえるか、直前の行より明確に深くする。
- プリプロセッサ命令の `#` は原則として行頭に置く。条件付きコンパイルが関数内に
  現れる場合も同様とする。
- 行末に空白を残さない。
- 1行には一つの文だけを書く。

```c
switch (state)
{
  case SHELL_STATE_READY:
    shell_run(context);
    break;

  default:
    break;
}
```

### 2. 行の長さ

- 原則として1行120文字以内とする。
- 120文字を超える条件式、関数呼び出し、宣言は意味のまとまりで改行する。
- 継続した条件演算子は行頭側に置き、条件の対応関係が見えるようにする。
- URL、変更できない識別子、ログ文字列、レジスタ定義など、分割により可読性や検索性が
  低下するものは例外としてよい。
- コメントやマクロの桁そろえだけを目的として、行を過度に長くしない。

STM32Cube の実コードには120文字を超える行も存在しますが、その多くは長い Doxygen
コメント、マクロ、レジスタ定義、生成コードです。新規コードではこれを一般則にしません。

```c
if ((context->state == SHELL_STATE_RUNNING) &&
    (context->request != SHELL_REQUEST_NONE))
{
  shell_process_request(context);
}
```

### 3. 波括弧と空白

- 関数、制御文、構造体、列挙型の開始波括弧は次の行に置く（Allman 形式）。
- `if`、`for`、`while`、`switch` と `(` の間には空白を1文字置く。
- 関数名と `(` の間には空白を置かない。
- 二項演算子と代入演算子の前後には空白を1文字置く。
- 単項演算子と対象の間には空白を置かない。
- コンマの後には空白を1文字置き、コンマの前には置かない。
- ポインタを表す `*` は変数名側へ付ける。
- 制御文の本体には、単文でも波括弧を付ける。
- `else` は、直前の閉じ波括弧の次の行へ置く。
- 空の無限ループにも波括弧を付け、意図をコメントで示す。

```c
static int32_t shell_start(ShellContext_TypeDef *context)
{
  int32_t status = SHELL_OK;

  if (context == NULL)
  {
    status = SHELL_ERROR_INVALID_PARAM;
  }
  else
  {
    context->state = SHELL_STATE_RUNNING;
  }

  return status;
}
```

### 4. 名前の付け方

- 外部公開する関数は `MODULE_ActionObject` 形式とし、モジュールを示す大文字の接頭辞を
  付ける。例：`ESH_Init()`、`ESH_Execute()`。
- 内部関数は `static` とし、公開関数と混同しない一貫した名前を付ける。
- 型名は UpperCamelCase を基本とし、末尾に `_TypeDef` を付ける。
- マクロ、列挙子、定数は大文字のスネークケースとし、モジュール接頭辞を付ける。
- ローカル変数と構造体メンバーは lower_snake_case を基本とする。
- STM32 HAL API と直接対応する構造体のローカル変数は、HAL の慣習に合わせた名前を
  使用してよい。例：`GPIO_InitTypeDef gpio_init`。
- ハンドルへのポインタには `h`、入力ポインタには `p` など STM32 HAL 由来の接頭辞を
  使用してよいが、単なる型情報の重複になるハンガリアン記法は使用しない。
- 名前はスコープと寿命が広いほど説明的にする。短いループ変数には `i` を使用してよい。
- 略語の大文字・小文字はモジュール内で統一する。既存の周辺機器名
  (`GPIO`、`UART`、`DMA`) は STM32 の表記を維持する。
- 予約識別子を新規定義しない。特に先頭が二つのアンダースコアの名前は使用しない。

### 5. `typedef`

- 公開する構造体、列挙型、関数ポインタには `typedef` を使用する。
- 構造体型と列挙型の名前は `_TypeDef` で終える。
- ハンドル型は、初期化情報、状態、エラー情報、関連資源を一つにまとめる。
- 固定幅整数には `<stdint.h>` の `uint8_t`、`uint16_t`、`uint32_t`、`int32_t` などを
  使用する。幅が重要な値に `int` や `long` を使用しない。
- 真偽値は既存 API との整合に応じて `bool` または専用の列挙型を使用し、意味不明な
  整数フラグを増やさない。
- ポインタ型だけを隠す `typedef` は作らない。ポインタであることを宣言から読み取れる
  ようにする。
- 同じ意味の整数型へ別名を与えるだけの `typedef` は避ける。単位、範囲、抽象化の境界が
  明確になる場合だけ専用型を定義する。

```c
typedef enum
{
  ESH_STATE_RESET = 0U,
  ESH_STATE_READY,
  ESH_STATE_BUSY,
  ESH_STATE_ERROR
} ESH_StateTypeDef;

typedef struct
{
  ESH_StateTypeDef state;
  uint32_t error_code;
  void *user_context;
} ESH_HandleTypeDef;
```

### 6. 関数

- 一つの関数には一つの責務を持たせる。
- 外部へ公開しない関数は `static` にする。
- 読み取り専用のポインタ引数には `const` を付ける。
- 引数を取らない関数は `(void)` と明記する。
- ヘッダーのプロトタイプには引数名も記載する。
- 入力値を使用する前に、`NULL`、列挙値、範囲、状態を検証する。
- ハードウェア制約の表明には既存の `assert_param()` を使用できる。ただし、実行時に
  起こり得るエラーは assert だけに頼らず戻り値で通知する。
- ローカル変数は関数冒頭、または使用する最小スコープの冒頭で宣言し、宣言時に可能な限り
  初期化する。
- 割り込みハンドラー、コールバック、弱定義関数は、フレームワークが要求するシグネチャと
  命名を変更しない。
- 長い関数、深いネスト、同じ判定の反復は、内部関数への分割を検討する。
- 関数定義の間には空行を1行入れる。

### 7. 終了処理の一元化

- 単純な引数エラーや、後始末が不要な失敗は早期 `return` で返してよい。
- 複数の分岐で同じ後始末が必要な関数は、状態変数と共通の終了処理に集約する。
- 資源を段階的に確保する関数では、解放処理の重複を避けるため `goto` を使用してよい。
- `goto` はエラー時の後始末と単一出口への移動だけに限定し、通常の制御フローや後方への
  ジャンプには使用しない。
- ラベルには `error`、`cleanup`、`release_buffer` など、処理内容が分かる名前を付ける。
- 解放は確保と逆の順序で行う。
- 終了処理自体の失敗を無視する場合は、その理由をコメントまたは API 契約で明確にする。

```c
static int32_t device_open(Device_HandleTypeDef *device)
{
  int32_t status;

  status = buffer_allocate(&device->buffer);
  if (status != DEVICE_OK)
  {
    return status;
  }

  status = peripheral_start(device);
  if (status != DEVICE_OK)
  {
    goto release_buffer;
  }

  return DEVICE_OK;

release_buffer:
  buffer_release(device->buffer);
  device->buffer = NULL;
  return status;
}
```

STM32Cube の BSP では、`ret` を初期化し、分岐で更新して末尾から返す形式が多く使われます。
HAL では、後始末が不要な異常をその場で返す形式も使われます。どちらを選ぶ場合も、資源の
所有権と全終了経路を容易に確認できることを優先します。

### 8. コメント

- C89 形式の `/* ... */` を使用し、`//` は使用しない。
- 公開 API、型、マクロには Doxygen 形式のコメントを付ける。
- 関数コメントには最低限 `@brief` を記載し、必要に応じて `@param`、`@note`、
  `@retval` を記載する。
- `@param` には方向が有用な場合、`[in]`、`[out]`、`[in,out]` を付ける。
- `@retval` では、返り得る値と意味を具体的に説明する。単に「status」とだけ書かない。
- 実装コメントは「何をしているか」の逐語訳ではなく、理由、ハードウェア制約、順序制約、
  副作用を説明する。
- 複数行コメントは次の形式とする。
- 古くなったコードをコメントアウトして残さない。履歴管理へ委ねる。
- TODO には、必要な作業と条件を具体的に書く。
- CubeMX の `USER CODE BEGIN` / `USER CODE END` マーカーは移動、削除、改変しない。

```c
/**
  * @brief  Start command execution.
  * @param  context Command shell context.
  * @retval ESH_OK Execution was started.
  * @retval ESH_ERROR_INVALID_PARAM context was NULL.
  */
int32_t ESH_Start(ESH_HandleTypeDef *context);

/*
 * Clear the pending flag before enabling the interrupt to prevent a stale
 * peripheral event from invoking the handler immediately.
 */
```

### 9. プリプロセッサ処理

- ヘッダーにはインクルードガードを設ける。名前はファイル名を大文字化した一意なものにする。
- C++ から参照される公開ヘッダーは `extern "C"` ガードを設ける。
- 機能選択には `#if defined(NAME)` または `#ifdef NAME` を使用し、同一ファイル内で表記を
  統一する。
- 長い条件付きコンパイルの `#endif` には対象条件をコメントする。
- コンパイラや MCU 系列の差異は、可能な限り専用ヘッダーまたはターゲット層へ隔離する。
- マクロ名は大文字のスネークケースとし、モジュール接頭辞を付ける。
- マクロ引数と展開式を括弧で囲み、多重評価による副作用を起こさない。
- 複数文マクロは `do { ... } while (0)` で囲む。
- 型検査、デバッグ容易性、単一評価が重要な処理は `static inline` 関数を優先する。
- 数値定数には型に応じて `U`、`UL` などの接尾辞を付ける。
- `#if 0` で不要なコードを保存しない。
- `#include` は、対応する自モジュールのヘッダー、標準ヘッダー、外部ヘッダー、内部ヘッダーの
  順にグループ化する。各グループ内の順序を一定にする。

```c
#ifndef ESH_PORT_H
#define ESH_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#define ESH_ARRAY_SIZE(array)  (sizeof(array) / sizeof((array)[0]))

#ifdef __cplusplus
}
#endif

#endif /* ESH_PORT_H */
```

### 10. データ構造

- 関連する設定値は `InitTypeDef`、実行状態と資源は `HandleTypeDef` など、役割ごとの構造体へ
  まとめる。
- 公開構造体の各メンバーには、意味、単位、許容範囲、参照する定数群をコメントする。
- 構造体はゼロ初期化してからフィールドを設定する。未初期化の予約領域を残さない。
- 状態や選択肢には、無関係な数値の組み合わせより列挙型を優先する。
- ビットマスクには名前付きマクロを使用し、マジックナンバーを直接記述しない。
- MMIO レジスタ、DMA、割り込みと共有する値には、必要な場所に `volatile` を使用する。
  ただし、`volatile` を排他制御やメモリバリアの代用にしない。
- 所有権、寿命、初期化・破棄の順序を API と型の設計で明確にする。
- ISR と通常処理、または複数実行コンテキストから共有するデータには、クリティカル
  セクション、アトミック操作、キューなど適切な同期方法を定める。
- 公開構造体を変更するときは、ABI、初期化子、永続化形式への影響を確認する。
- メモリ制約がある場合も、異なる意味のデータを安易な共用体やビットフィールドへ
  詰め込まず、移植性とアクセスの安全性を優先する。

### 11. 戻り値

- 処理結果を返す関数は、モジュールで定義した状態型または `int32_t` を使用する。
- STM32 HAL と直接接続する層では `HAL_StatusTypeDef` を使用し、成功を `HAL_OK`、失敗を
  `HAL_ERROR`、`HAL_BUSY`、`HAL_TIMEOUT` で表す。
- BSP または MCUFW の層では、成功を `*_OK` または `*_ERROR_NONE`、失敗を具体的な負の
  エラー値で表す。
- 同じモジュール内で、成功値と失敗値の規則を統一する。
- 述語関数は `bool` を返す。既存 HAL API に合わせる場合は `0U` / `1U` または専用の
  `FlagStatus` を使用してよい。
- ポインタを返す関数は、失敗時に `NULL` を返す。呼び出し側は使用前に検査する。
- 出力値をポインタ引数で返す関数は、戻り値を処理結果の通知に使用する。
- 下位層のエラーを上位層のエラーへ変換するときは、原因を識別するために必要な粒度を失わない。
- エラーを無視する場合は、戻り値を明示的に破棄し、その安全性が自明でなければ理由を書く。
- `void` は、失敗しない処理、失敗を通知できないコールバック、または既定シグネチャに限る。

```c
int32_t ESH_GetState(const ESH_HandleTypeDef *handle,
                     ESH_StateTypeDef *state)
{
  if ((handle == NULL) || (state == NULL))
  {
    return ESH_ERROR_INVALID_PARAM;
  }

  *state = handle->state;
  return ESH_OK;
}
```

## ファイルの構成

STM32Cube のソースに合わせ、原則としてファイル内を次の順で構成します。空の区分を機械的に
残す必要はありません。

1. ファイルヘッダーとライセンス
2. インクルード
3. 非公開の型定義
4. 非公開の定数・マクロ
5. 非公開の変数
6. 非公開関数のプロトタイプ
7. 公開関数
8. 非公開関数

ヘッダーは、公開型、公開定数・マクロ、公開関数プロトタイプの順にします。ハードウェア依存の
定義を移植可能なコア層へ持ち込まず、ターゲット層に閉じ込めます。

## 全体を短くまとめると

- 空白2文字、タブなしの Allman スタイル
- 原則120文字以内、単文でも波括弧を付ける
- 公開名にはモジュール接頭辞、型には `_TypeDef`
- 固定幅整数、明示的な状態型、`const` を活用する
- 公開 API は Doxygen、実装コメントは理由と制約を書く
- マクロより型付き関数を優先し、条件付き処理を局所化する
- 資源の所有権と終了経路を明確にする
- HAL、BSP、MCUFW の各層で戻り値の体系を統一する
- 生成コードと外部コードは整形せず、ユーザー記述部分だけ本規約に従う

## 調査メモ

2026年9月23日に、次のツリーにある `.c` と `.h` を対象として確認しました。

- `Drivers`: HAL 244ファイル、BSP 232ファイルを重点確認
- `Projects`: 5,065ファイル。各ボードの Example、Application、CubeMX 生成コードを確認
- `Utilities`: 90ファイル。GUI、JPEG、LCD、LPBAM、Trace 関連を確認

代表的な参照箇所は次のとおりです。

- `Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c`
- `Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gpio.c`
- `Drivers/STM32U5xx_HAL_Driver/Inc/stm32u5xx_hal_gpio.h`
- `Drivers/BSP/STM32U5xx_Nucleo/stm32u5xx_nucleo.c`
- `Utilities/lpbam/stm32_lpbam_common.c`
- `Projects/NUCLEO-U575ZI-Q/Examples/GPIO/GPIO_IOToggle/Src/main.c`

観察された例外には、CMSIS の自動生成レジスタ定義、フォントデータ、第三者ミドルウェア、
一部の古い Utility、CubeMX テンプレート内の書式の揺れがあります。これらは STM32 向けの
新規コードへそのまま持ち込むべき慣習とは見なしません。
