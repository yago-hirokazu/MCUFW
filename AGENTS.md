# AGENTS.md

## MCUFW

PC、マイクロコントローラに移植可能なコマンドシェル。


## ディレクトリ構造 /

docs/						文書、資料置き場
src/						ソースコード


## ディレクトリ構造 src/

core/						移植可能なレイヤ
	elib/					ライブラリ。再利用可能そうなモジュールをここに集約する。
		stm32/				STM32マイコンFW開発プロジェクトから抽出したモジュール。
	esh/					コマンドシェル
		include/			公開ヘッダファイル置き場
		stub/				テストプログラム用のスタブ
		target/				コマンドシェルのハードウェア依存レイヤ
				cygwin/		Cygwin用 glue code
				stm32/		STM32用 glue code
				windows/	Windows用 glue code
		test/				core/用テストプログラム置き場
				cquote/		test for core/esh/cquote.c
				exe/		test for core/esh/exe.c
				par/		test for core/esh/par.c
	support/				育成中のプログラム置き場
				getopt/		組込み機器用のgetopt
	usr/					STM32機種非依存プログラム置き場
		cmd/				STM32用汎用UARTコマンド
target/						ハードウェア依存レイヤ
	NUCLEO-U575ZI-Q/		NUCLEO-U575ZI-Q評価ボード
	windows					Windows (mingw64)


## core

### 背景

- UARTコマンドシェルとして育てていた。
- 最初のハードウェアプラットフォームはSTM8。以後、STM32やWindowsに移植。
- 参考にしたもの：
  - UNIX V6 の tty とその他の部分のソースコードやコメンタリー
  - busybox のソースコード


### 現在取り組んでいること

- スクリプトインタプリタの開発・結合。
  - bash ではなく、半世紀前の Bourne shell 相当の文法を解釈できる。Bourne shell の方ができないことが多く、その分実装負担が少ないと考えたから。ただし、Bourne shell の完全再現は目指さない。
  - lex(er) で文字列をトークンに変換する。
  - par(ser) でトークンをバイトコードに変換する。
  - exe(cute) でバイトコードを実行する。
  - lex, par, exe は、ある程度できている。
  - lex, par, exe は、OSがない環境（ベアメタル）でも動けるようにステートマシンとコンテキスト構造体で実行コンテキストを保持する設計になっている。
  - 解釈可能な Bourne shell の文法を拡張しているところ。
  - 引数あり break コマンドの追加のところで作業が中断している。（多忙のため）


### これからやりたいこと

#### 確定事項

下記の設計で UNIX パイプを使えるようにしたい。

1. パイプ処理をするときは、RTOS が使えない環境でも動かせるように、一時的に coroutiine を用いて異なるコマンド処理を時分割で実行する。RTOS が使える環境では coroutine ではなくスレッドを使って UNIX パイプを使えるようにしたいので、移植レイヤを用意して開発を進める。

2. UNIX V7 相当のファイルシステムを使えるようにして、UNIX V7 と同じようにしくみでパイプを使えるようにする。ただし、MMU (Memory Management Unit) がない環境でも動かしたいため、下記の制約を守った設計にする。
   - 最下層はバッファキャッシュまでとする。物理ストレージへのアクセスはしない。
   - ファイルシステムは UNIX V7 ファイルシステムを完全再現はせず、パイプを使えるだけの最小限の構成とする。


## target

`target` 開発の基本方針

1. まず Windows で動くソフトウェアを開発する。

2. 1 ができたら、移植可能な部分を  NUCLEO-U575ZI-Q に移植する。


## 開発環境

### Target=Windows

- mingw64
C:\msys64\mingw64\bin

### Target=STM32

- Keil MDK-ARM
C:\Keil_v543a\ARM\ARMCLANG\bin


## ビルドとテスト

### /src/target/windows

1. mingw64シェルで下記を実行

- make
- ./main.exe

2. プロンプトが `#` に変わったら下記のコマンドを実行して問題ないか確認

- sam1
- sam2 1 10 100
- sam3 -a -b string


### /src/core/esh/test/par

ming64シェルで下記を実行して`test pass`になる確認

- bash ./run_test1.sh
- bash ./run_test2.sh


### /src/core/esh/test/exe

ming64シェルで下記を実行して`all tests passed`になる確認

- bash ./run_test.sh

## コーディングルール1

### 対象ディレクトリ

src/core/
src/target/windows/

### 適用ルール

docs/coding_style_summary_linux_kernel.md


## コーディングルール2

### 対象ディレクトリ

src/target/NUCLEO-U575ZI-Q/

### 適用ルール

STM32CubeMX が生成した下記対象ディレクトリのプログラムコードには下記のルールを適用する。
下記の対象ディレクトリ以外のプログラムコードは「コーディングルール1」を適用する。

#### 対象ディレクトリ

src/target/NUCLEO-U575ZI-Q/Drivers
src/target/NUCLEO-U575ZI-Q/Inc
src/target/NUCLEO-U575ZI-Q/Src

#### 適用ルール

docs/coding_style_summary_stm32cubefw.md


## コーディングルールの観点

1. インデント
2. 行の長さ
3. 波括弧と空白
4. 名前の付け方
5. `typedef`
6. 関数
7. 終了処理の一元化
8. コメント
9. プリプロセッサ処理
10. データ構造
11. 戻り値

