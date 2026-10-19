# WCH 工程指标与接口规格

| 指标 | 目标 | 当前状态 |
|---|---|---|
| USB FS CDC | UBR1 CRC/序号连续 | 真实 FPGA/nRF 短窗口与长测通过 |
| FSMC 16 位桥接 | 块头、payload、ACK | 双 Rx 窗口通过；偶发错字仍保留诊断 |
| RAM 门限 | static < 56 KiB，含栈 < 64 KiB | 以当前构建 MAP/size 为准 |

## 接口

| 接口 | 方向 | 位宽/协议 | 备注 |
|---|---|---|---|
| FSMC AD | 双向 | 16 bit, 3.3 V | 复用地址/数据 |
| NE/NOE/NWE/NADV | 主控→FPGA | 3.3 V | 见硬件接线 |
| NWAIT | FPGA→主控 | 1 bit | 极性需按板测确认 |
| USB FS CDC | 双向 | UBR1 | 主机端校验 |

## 验收边界

WCH 真实 USB 数据通过不等于双 Tx/双 Rx 物理同步通过；长时间稳定性和配置丢失根因仍需独立证据。
