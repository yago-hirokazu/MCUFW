# log

## 実装範囲

2026-11-03: Windows向けlog_errorとassert_warmを実装する。
log_debug、audit log、STM32移植、既存チェックの一括置換は今後の作業とする。

## ディレクトリ構造

```text
src/core/shell/assert/
    custom_assert.h      warm assert
src/core/shell/log/
    include/
        log_error.h      公開APIとログ構造体
        code_file.h      ファイル番号一覧
        code_error.h     エラーコード一覧
        log_adapt.h      時刻・排他の移植API
    log_error.c          保存・取得・表示
    dev/windows/
        log_adapt.c      Windows時刻・排他
    test/
        Makefile
        test_error.c     模擬時刻による単体テスト
        test_windows.c   Windows時刻・複数スレッドのテスト
        test_clock.c     Windows時刻APIを模擬した周回境界テスト
```

## エラーログ

```c
#define NERRLOG 64

struct log_error {
    uint32_t time;
    uint32_t wrap;
    uint16_t line;
    uint8_t file;
};
```

- timeはWindowsでは32bit、Cortex-Mでは24bitを使用する。
- wrapはtimeの周回数。32bitとして運用し、上限で周回する。
- fileには8bitのファイル番号を保存する。
- lineは検知したソース行番号。65535以下の行番号を対象とする。
- ファイル番号とエラー種類の0は予約する。
- 番号はcode_file.h/code_error.hで管理し、呼び出し箇所追加時に割り当てる。
- typedefは使わない。構造体を永続化用のバイナリ形式とはみなさない。
- errlog[NERRLOG]はlog_error.c内のstatic配列とする。
- 保存件数は64件。満杯の場合は最古から上書きする。
- 初期化以外で動的メモリ確保を必要としない。

## 公開API

```c
int init_error(void);
void error_write(const struct log_error *log);
void error_record(uint8_t file, uint16_t line);
int error_read(uint32_t index, struct log_error *log);
uint32_t error_count(void);
void error_dump(void);
```

- init_error(): 時刻源・排他・保存領域を初期化する。
  成功は0、失敗は-1。利用開始前、他スレッドの呼び出しがない状態で1回呼ぶ。
  初期化失敗時は他のAPIを呼ばない。
- error_write(): 完成済みのログをコピーする。
- error_record(): 現在のtime/wrapを取得し、file/lineとともに保存する。
  fileには呼び出し側がファイル番号を指定する。
- error_read(): 保存中の最古をindex 0としてコピーする。
  成功は0、未記録は-1。未記録の場合、出力先を変更しない。
- error_count(): 保存件数0～64を返す。
- error_dump(): ロック中にスナップショットをコピーし、解除後に標準出力へ表示する。
  順序は保存順で最古から。時刻順への並べ替えはしない。
  呼び出しスレッドのスタックに64レコード分の領域を使用する。
  複数dump同時実行時の標準出力全体の直列化は保証しない。

引数のNULLチェックは追加しない。有効なポインタを呼び出し側が渡す。
ログ記録はプロセス（スレッド）コンテキスト限定とし、割り込みからは呼ばない。
write/read/countは個々に排他する。複数回readの間にwriteがあればindexは移動する。
error_recordの時刻取得と保存は別操作であり、スレッド間の保存順と時刻順は
必ずしも一致しない。

### dump形式

ヘッダに続き、index/time/wrapを10進数、fileを2桁の16進数、
lineを10進数で表示する。

```text
index time wrap file line
0 4294967295 7 12 42
1 0 8 12 43
```

## assert_warm

```c
assert_warm(_cond_, _file_, _ret_)
assert_void(_cond_, _file_)
```

- 条件が真なら継続する。
- 条件が偽ならerror_record()で記録し、指定値でreturnする。
- void版は記録後にreturnする。assert_warm_voidは互換名として使用できる。
- CLIの引数チェックは1つのassertで1つの条件式を検査する。
  複合条件は、参照するポインタを先に確認する順で分割する。
- 条件は1回だけ評価し、file/retの式は失敗時だけ評価する。
- 呼び出し元の__LINE__を記録する。
- do/while形式として、if/else内でも安全に使えるようにする。
- 既存チェックのreturnを維持するため、NDEBUGでも無効化しない。
- マクロ内に呼び出し側と衝突するローカル変数を設けない。
- CLIの既存引数チェックをassert_warm/assert_voidへ置き換える。

```c
assert_warm(ap != NULL, FILE_CLI_RUN, -1);
assert_warm(ap->name != NULL, FILE_CLI_RUN, -1);
assert_void(ap != NULL, FILE_CLI_RUN);
```

ファイル番号はcode_file.hのFILE_CLI_*を使用する。

## 移植API

```c
enum log_lock_id {
    LOG_ERROR_LOCK,
    LOG_LOCK_COUNT
};

int log_adapt_init(void);
void log_adapt_get_time(uint32_t *time, uint32_t *wrap);
uint32_t log_adapt_frequency(void);
unsigned int log_adapt_time_bits(void);
void log_adapt_lock(enum log_lock_id id);
void log_adapt_unlock(enum log_lock_id id);
```

- アプリケーションは通常、移植APIを直接呼ばない。
- 時刻源と排他は共通の仕組みとし、ロック実体はログ種別ごとに分ける。
- audit追加時はLOG_AUDIT_LOCKを追加し、専用ロックを用意する。
- Windowsは識別子ごとのSRWLOCK、RTOSはmutexを使用する方針。
- 単一実行主体のベアメタルではログ用ロックを空実装にできる。
- 時刻取得時の割り込みとの整合性はログ用ロックと別に保証する。

### Windows

- QueryPerformanceCounterの初期値からの差分を使用する。
- 1tickはQPCの1クロック。周波数はQueryPerformanceFrequencyから得る。
- 差分の下位32bitをtime、上位32bitをwrapとする。
- 1回のサンプルからtime/wrapを作り、周回境界での食い違いを防ぐ。
- ハードウェア周回割り込みを再現せず、取得値から周回数を求める。
- frequency APIの範囲に合わせ、周波数が0以下またはUINT32_MAX超なら
  初期化失敗とする。
- Windows本体はmain()のeshinit()より前にinit_error()を呼ぶ。

### Cortex-M（今後実装）

- SysTickを24bit最大周期（LOAD=0x00ffffff）で運用する方針。
- ダウンカウンタは1から0で割り込みを発生し、周期は2^24クロック。
- 割り込みハンドラでwrapだけを更新し、ログ記録は行わない。
- 増加方向のtimeへの変換、起動直後、割り込み保留中の補正を設計する。
- 複数周回に及ぶ割り込み禁止では周回を失うため、許容時間を定める。
- 現行SysTick_Handler()はHAL_IncTick()を呼んでいるため、
  HAL用時刻源との共存方法は移植時に決定する。

## テスト

MSYS2 MINGW64 shellで以下を実行する。

```sh
cd src/core/shell/log/test
make test
make clean
```

- リングの空・64件境界・複数周回後の保存順
- 未記録read時の戻り値と出力先維持
- assertの成功・失敗・void・単一評価・行番号・NDEBUG
- 模擬時刻によるtime最大値前後とwrapの保持
- Windows移植実装の起点差分・32bit分割・複数周回・周波数範囲
- dumpの出力形式
- Windows実時刻の増加と4スレッド計40000件の保存・読み取り

Windows本体のビルドと既存par/exeテストも確認し、実行後はmake cleanする。

## 参考資料

- [Arm SysTick](https://arm-software.github.io/CMSIS_5/5.8.0/Core/html/group__SysTick__gr.html)
- [Arm割り込み条件](https://documentation-service.arm.com/static/5ea04d399931941038de55af)
- [Windows時刻源](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)
