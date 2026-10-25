# sys pipe sleep・wakeup

更新日: 2026-10-26

## 役割

portableな`src/core/sys/sys/sched.c`はwait listと`struct proc`状態を管理し、
context切替とlock実装をscheduler backendへ委譲する。Windows backendは
`src/core/sys/dev/windows/sched_cortn.c`と`sched_thread.c`にある。

`struct proc`と`enum proc_state`は`h/proc.h`で定義し、`h/sched.h`がincludeする。
`p_stat`は実行状態、`p_wchan`は待機対象、`p_link`は待機リストの連結を表す。
公開状態はV7型の分類を使い、継続不能・取消の`PROC_ERROR`をMCUFW拡張とする。
`p_addr`は呼出元所有userへの直接参照、`p_xctx`はbackend所有の実行context、
`p_ret`はexecutorが保存するmainのint戻り値である。

`src/core/sys/dev/windows/sched_backend.mk`の`SCHED_BACKEND`でbackendを選ぶ。
defaultは`cortn`で、`thread`指定時はnative Windows threadを使用する。

## Kernel lock

pipe状態、inode lock、file/inode参照、wait listを単一の非再帰kernel lockで
保護する。coroutine backendは所有状態を追跡し、yield時にlockを解放してcontext
切替後に再取得する。thread backendはSRW lockとcondition variableを使う。

## Sleep

`sched_sleep()`はlock保持中に呼び出す。

```text
pipeまたはinode lock条件を検査
  -> processをwait channel末尾へ登録
  -> SSLEEPへ変更（理由はWAIT_LOCK/READ/WRITE引数とbackendへ渡す）
  -> backendがlock解放と待機を不可分に実行
  -> wake後にlock保持状態で復帰
  -> 呼出し元loopで条件を再検査
```

wait登録前に実行権を返さないため、条件検査直後のwakeupを失わない。

## Wakeup

`sched_wakeup()`はchannel上の全processをwait listから外し、backendをwakeして
`SRUN`へ変更する。wake後にI/O可能とは限らない。各processは条件を
再検査し、満たされていなければ再びsleepする。
backend wakeに失敗したprocessは`PROC_ERROR`とし、同じchannelの残りの
processの起床を続ける。各processの`p_wchan/p_link`は成功・失敗とも解除する。

使用するchannelは次のとおり。

- write後にread channelをwake
- 全dataをreadしてbufferを空にした後にwrite channelをwake
- endpoint close後にread/write channelをwakeし、EOF/EPIPEを再判定
- inode lock解放時に`IWANT`があればlock channelをwake

## Blocking I/O

`sys_read()`はcurrent processとfdを解決し、`readp()`を呼ぶ。dataがあれば現在
取得可能なbyte数を返し、空でwriterが存在すればsleepする。最終writer close後
かつdataが空なら0を返す。

`sys_write()`は`writep()`を呼び、要求全体を転送するまで部分writeとsleepを
繰り返す。待機中に最終readerがcloseした場合は`-SYS_EPIPE`を返す。

backendをbindしていないprocessでは、待機が必要な場合に`-SYS_EAGAIN`または
転送済みbyte数を返す。

## Scheduler状態

`sched_resume()`はbackendを再開し、報告をlock下の共有状態と照合する。

| 状態 | 値 | 意味 |
|---|---|---|
| `SIDL` | 4 | 初期化済み。準備前または管理専用 |
| `SRUN` | 3 | 実行可能または実行中 |
| `SSLEEP` | 1 | イベント待機中 |
| `SZOMB` | 5 | entryが通常returnし、結果回収待ち |
| `PROC_ERROR` | 7 | 実行機構の継続不能または完了前の取消 |

実行中と実行可能は`p_stat`で分けず、currentとbackendの実行権で管理する。
backendが古い`SSLEEP`を報告しても、wakeup済みの`SRUN`は保持する。
不正な状態報告や未解除の待機を伴う完了報告は、待機を除去して`PROC_ERROR`にする。
fatal状態とfatal報告は通常完了より優先し、通常returnで消去しない。

元のstackへ安全に戻れる待機試行の失敗は、待機登録を解除して`SRUN`へ戻す。
`sched_sleep()`の負値をentryが処理してreturnすれば`SZOMB`になり得る。
backendの継続不能は`PROC_ERROR`とし、`sched_resume()`は負値を返す。
I/OのEOF/EPIPE、mainの非ゼロ戻り値と、実行機構のfatalを混同しない。

`current_proc()`はcoroutineではbackend global、threadではTLSからcurrent
processを返す。runnable processがなく未完了processが残る場合、executorは
idleまたはdeadlockとして扱う。

processは`proc_init(proc, user)`で初期化する。`p_addr`は呼出元所有userを指し、
`current_user()`と`u`マクロはcurrent procからその参照先を求める。currentが
なければbackend-local base userを使い、thread backendではTLSに置く。
sleep/wakeupでuserを初期化しない。`u_base/u_count/u_offset`と`u_r`も
各userに保持し、別procやhostの転送状態と分離する。fd tableは`p_addr->u_ofile[]`にあり、
`u_procp`は所有procを指す。currentなしのbase userでは`u_procp`はNULLとなる。
専用userはbackendのrelease/join完了まで生存させる。
pipe待機へ入る前に`u_count`を全要求の残量へ戻す。復帰後はinode条件を再検査し、
別procのcloseでEPIPE/EOFになった場合も、実際の転送済み状態を保持する。
plockの競合やsched_sleepの失敗は`u_error`へ記録し、取得していないlockは解放しない。

`sched_release()`はprocessをwait listから外し、backend handleをkernel lock外で
解放する。thread join中にworkerがlockを必要とするため、lock内でjoinしない。
未完了backendは`PROC_ERROR`を公開してから`p_xctx`を切り離す。
通常完了した`SZOMB`と`p_ret`は保持し、backendなしのreleaseは状態を変えない。
fdのcloseは所有者が行い、release自体はfdを閉じない。管理用procの
backendなしreleaseも、fd参照やuserのerror/resultを変更しない。
releaseはuserを初期化しない。threadは停止通知により待機からEINVALで戻り、
同じp_addrのuserでI/Oの残量・結果を確定してから終了する。coroutineは停止中の
stackを破棄するため、待機後の処理を実行しない。両方式ともrelease完了後にのみ
userとprocを再初期化できる。破棄済みstackを指すu_baseは参照しない。
`p_xctx == NULL`や終端状態だけではjoin完了を意味しない。
単一コマンドはbackendを作らず`SIDL -> SRUN -> SZOMB`で直接実行し、
mainの戻り値を`p_ret`に保持する。管理専用ownerは`SIDL`のままとする。

## V7との差分

V7のwait channelと条件再検査を採用するが、process切替機構は移植可能backendへ
分離する。priority、signal、timeout、swapping、CPU preemptionは実装しない。
coroutineとthreadは同じ`struct proc`状態とpipe結果を公開する。

| メンバ | V7との対応とMCUFWでの範囲 |
|---|---|
| `p_stat` | SSLEEP=1、SRUN=3、SIDL=4、SZOMB=5を採用。PROC_ERROR=7は独自拡張 |
| `p_wchan` | 待機対象。MCUFWでは対象objectに埋め込むwait_chanへのpointer |
| `p_link` | V7は実行・睡眠キューに使用。MCUFWはchannelごとの睡眠キューだけに使用 |
| `p_addr` | V7はswap可能imageの配置。MCUFWは専用userへの直接pointer |
| `p_xctx` | MCUFW固有。eXecution ConTeXtを指し、backendが資源を所有 |
| `p_ret` | MCUFW固有。V7 xproc.xp_xstatのoverlayやwait用bit encodingは採用しない |

SIDLは管理専用procにも使い、SZOMBは論理的なentry完了を示す。
PID・親子関係・wait system callや、native threadのjoin完了は意味しない。
設計根拠は[Step 0契約](unix_v7_proc_alignment_step0.md)、実装・全回帰結果は
[proc整合引き継ぎ書](unix_v7_proc_alignment_handoff.md)を参照する。
