# WCH 设计思路

## 1. 问题背景

用 CH32V307VCT6 替代 F730，保持 FPGA 16 位 FSMC/FMC、UBR1 USB 分片、块/记录 CRC 与 ACK 协议不变。

## 2. 总体架构

```text
FPGA 双通道块 -> CH32V307 FSMC -> bridge_state/CRC -> USB FS CDC -> PC
```

## 3. 模块划分

| 模块 | 位置 | 职责 |
|---|---|---|
| FSMC 端口 | `firmware/wch_f730_bridge_20261018/app/extbus_port.c` | 16 位总线读写和等待 |
| 桥接状态机 | `firmware/wch_f730_bridge_20261018/app/bridge_state.c` | 块快照、fault、重试和 ACK |
| 协议 | `firmware/wch_f730_bridge_20261018/protocol/` | FPB1、记录 CRC、UBR1 |
| USB | `firmware/wch_f730_bridge_20261018/app/usb_port.c`、`vendor_usb/` | CDC 发送 |

## 4. 约束

- 保持 F730/FPGA 冻结协议逐字节兼容。
- `flags=0x8` 合法；块/记录 CRC 失败必须停在可诊断状态。
- 不把模拟源测试当成真实 nRF 通过。
