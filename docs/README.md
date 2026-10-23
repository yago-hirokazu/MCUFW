# MCUFW 文書案内

更新日: 2026-10-24

現行ソースに基づく仕様と、過去の依頼・作業記録を案内する。
user移行の段階別の実装・試験結果は
[引き継ぎ書](unix_v7_user_context_handoff.md)を参照する。

## 現行仕様

| 文書 | 内容 |
|---|---|
| [Pipe API](pipe_api.md) | 上限、fd・file・inodeの所有権、公開API |
| [File・pipe core](pipe_core.md) | 初期化、参照数、I/O、user area |
| [Parser](pipe_parser.md) | pipeline構文とbytecode |
| [Executor](pipe_executor.md) | process接続、実行、status、cleanup |
| [Scheduler](pipe_sched.md) | sleep/wakeup、kernel lock、current user |
| [Windows coroutine](cortn_windows.md) | context保存、stack、backend選択 |
| [Windows thread](pipe_thread.md) | 実行token、TLS、join |
| [Windows stdio](pipe_stdio.md) | consoleとpipeの切替、対応関数 |
| [RAM block I/O](ram_block_device.md) | device、buffer API、error、使用例 |
| [Buffer再割当て](bio_reassociate_explanation.md) | assign_buffer_to_blockの責務 |
| [回帰試験](pipe_regression.md) | 試験入口、実行条件、clean |

現在は`core/sys`にfile・inode・pipe・scheduler・buffer cacheを配置する。
既定cache数は5、pipeは最大3本、pipelineは最大4段である。
`struct proc.p_addr`で専用userを参照し、userは`u_error`、`u_r`、`u_procp`、`u_ofile[]`、
`u_base/u_count/u_offset`を持つ。
fd table、file割当・pipe作成の結果、inode/pipe I/Oとsys2はuserへ移行済み。
後続Stepではerror・寿命・資源再利用と全回帰を検証する。

## 作業記録・計画

以下の本文・試験結果は記載当時の記録。冒頭の現況追記と現行仕様を先に確認する。
古いAPI例、source path、未着手表記を現在の仕様として使用しない。

- [Pipe初期レビュー](pipe_review_results.md)
- [Review・refactoring計画](pipe_review_refactoring_plan.md)
- [Review引き継ぎ](pipe_review_refactoring_handoff.md)
- [2026-10-04の解説チャット](pipe_explanation_chat_20261004.md)
- [Buffer cache導入](unix_v7_buffer_cache_pipe_handoff.md)
- [汎用sys移行](unix_v7_generic_sys_migration_handoff.md)
- [Coroutine移動・初期化分離](cortn_finit_refactoring_handoff.md)
- [RAM device導入](unix_v7_ram_block_device_handoff.md)
- [User導入計画と実装済み範囲](unix_v7_user_context_handoff.md)
- 過去の依頼: [10-11](request_2026-10-11.md)、[10-12](request_2026-10-12.md)、
  [10-14](request_2026-10-14.md)、[10-20](request_2026-10-20.md)
- [引用変数の不具合記録](issue_2026-09-26_001.md)、[開発ログ](log.md)

## コーディング規約

適用範囲と優先規則は[AGENTS.md](../AGENTS.md)を参照する。

- [Linux kernel要約](coding_style_summary_linux_kernel.md)
- [STM32Cube firmware要約](coding_style_summary_stm32cubefw.md)
- [Arm CMSIS要約](coding_style_summary_arm_cmsis.md)
