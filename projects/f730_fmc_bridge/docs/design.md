# 设计思路

## 1. 问题背景

F730 作为 FPGA 到主机之间的 USB 桥接端，读取 FPGA 16 位复用 FMC/FSMC 窗口中的块数据，校验块头、记录和 CRC 后通过 USB FS CDC 输出。

## 2. 总体架构

```text
ZYNQ/FPGA片上块 -> 16-bit FMC/FSMC -> STM32F730 -> UBR1/USB FS CDC -> PC
```

WCH 工程复用同一块协议和 FSMC 语义，目标器件为 CH32V307VCT6。

## 3. 模块划分

| 模块 | 职责 | 输入 | 输出 | 时钟/复位 |
|---|---|---|---|---|
| `f730_bridge/BSP/Src/bsp_fmc.c` | FMC 初始化和读写 | FPGA 总线 | 块快照 | F730 HCLK |
| `f730_bridge/BSP/Src/bsp_bridge.c` | 块状态、CRC、ACK | FMC 数据 | UBR1 发送 | 主循环/USB |
| `f730_bridge/Bridge/Src/bridge_protocol.c` | 协议编码 | 块/记录 | UBR1 帧 | 主循环 |
| `WCH/wch_f730_bridge_20261018/app/` | CH32V307 移植 | FSMC 数据 | UBR1/USB | WCH 时钟 |

## 4. 关键设计决策

- 先验证 USB 自身，再验证 FMC 载荷。
- 保留 `flags=0x8` 的成熟块头兼容语义。
- WCH 工程不改变冻结的块格式、记录 CRC 和 ACK 语义。

## 5. 时序、CDC 和复位

FMC 的地址/数据稳定窗口、NWAIT 极性和 USB 配置状态分别处理。FPGA 重新配置后，F730 可能保留旧启动状态，需要按文档执行复位。

## 6. 资源和功耗考虑

F730 工程的 RAM/Flash 预算和 WCH 工程的 `bss` 预算必须分别查看构建 MAP；WCH 工程曾记录约 62.8 KiB 静态资源，具体以当前 release 构建为准。

## 7. 风险与未决问题

- [ ] WCH 长时间稳定性和多通道持续无损
- [ ] F730/FSMC 外部时序在不同板卡和温度下复核
- [ ] FPGA 配置丢失后的自动恢复策略
