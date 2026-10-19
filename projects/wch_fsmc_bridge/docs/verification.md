# WCH 验证记录

| 日期 | 层级 | 结果 | 证据 |
|---|---|---|---|
| 2026-10-18 | WCH USB/FPB1/记录 CRC | 13/13 帧通过 | 本地实验 evidence/flash_final_protocol_20261018 |
| 2026-10-18 | 真实双 Rx USB 长测 | 331 帧，CH0=1955、CH1=1758，transport gap=0 | 本地实验 evidence/same_tx_pair_20261018 |
| 2026-10-18 | 静默缓解复测 | 120 s、两路记录与 CRC/序号通过 | 本地实验 evidence/silence_recheck_20261018 |

## 已知问题

- WCH 工程的配置丢失、COM 句柄异常和 FMC 偶发错字仍需现场保留状态后定位。
- 上述结果是构建/板测证据，不代表四通道或物理同步已经通过。
