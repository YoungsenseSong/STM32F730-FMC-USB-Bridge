# 设计思路

## 1. 问题背景

F730 和 WCH 都作为 FPGA 到主机之间的 USB 桥接端，读取 16 位复用 FMC/FSMC 窗口中的块数据，校验块头、记录和 CRC 后通过 USB FS CDC 输出。

## 2. 总体架构

```text
ZYNQ/FPGA 片上块 -> 16-bit FMC/FSMC -> F730 或 CH32V307 -> UBR1/USB FS CDC -> PC
```

## 3. 工程分层

| 工程 | 入口 | 职责 | 输出 |
|---|---|---|---|
| F730 | `firmware/f730_bridge/` | 成熟 FMC/USB 桥接 | UBR1 |
| WCH | `../wch_fsmc_bridge/firmware/wch_f730_bridge_20261018/` | 国产 FSMC/USB 替代 | UBR1 |
| 协议 | `protocol/` | 块、记录、CRC、ACK 约束 | golden vectors |

## 4. 关键设计决策

- 生产协议、flags bit3 兼容语义和 ACK 顺序保持不变。
- F730 与 WCH 的构建、下载、现场证据分开记录。
- 只把真实 FPGA/nRF 数据的 USB 校验结果写成实物证据；内部模拟源仅作协议自测。

## 5. 时序、CDC 和复位

FMC/FSMC 地址/数据稳定窗口、NWAIT 极性和 USB 配置状态分别处理。FPGA 重新配置后，桥接端按文档复位并重新读取块状态。

## 6. 资源与风险

F730 的 Flash/RAM 以正式 MAP 为准；WCH 的静态 RAM 以 GCC `size` 和固件门限为准。WCH 配置丢失、COM 句柄异常和 FMC 偶发错字不得混写成同一根因。
