# Windows stdioとsys pipe接続

更新日: 2026-10-13

Windows buildは`ESH_STDIO_PIPE`でprocess stdio routingを有効にする。parser-only、
quote-only buildはconsole-only adapterを使用する。

- `printf`、`putchar`、`fprintf(stdout, ...)`はcurrent processのfd 1が
  `FWRITE|FPIPE`なら`sys_write()`へ送る
- `getchar`はfd 0が`FREAD|FPIPE`なら`sys_read()`から1 byte読み、EOFまたは
  read errorではEOFを返す
- 対応pipe endpointがなければ従来のWindows console I/Oへ渡す
- `fprintf(stderr, ...)`とshell debug/error macroはconsoleへ出す
- `scanf`はconsole専用とし、pipe stdinではdataを消費せずEOFを返す
- public read/write/close APIは`src/core/sys/h/file.h`で宣言する
- `fwrite`、`fputs`、`puts`、`vprintf`、host `FILE` bufferingは対象外とする

Windows formatted pipe outputは`vsnprintf`で長さを測り、callごとのhost bufferを
確保してから`sys_write()`で全byteを書き込む。bufferは書込み完了後またはEPIPE
時に解放する。このhost専用allocationはportable sys層へdynamic allocationを
導入しない。MCU formatterは別の移植作業とする。

Windows coroutine stackは64 KiBとする。16 KiBではCRT formatting結合試験中の
context returnに失敗した。固定4 process poolはWindowsで256 KiBのstack領域を
予約する。MCU stack sizingは別途決定する。

試験はecho producer、getchar EOF、`PIPSIZ`を超えるformatted output、putchar、
stdout/stderr分離、scanf拒否、resource cleanup、executor pipelineをO0/O2で
確認する。coroutine/threadで同じstdout dataと終了statusを要求する。

## 制約

- current processがないconsole処理はhost stdioへ渡す
- pipe endpoint以外の通常file descriptorは未実装
- read errorとEOFはmapped `getchar()`では区別できない
- pipe stdinへ`scanf`を使用できない
- portable sys層はformattingやhost allocationを担当しない
