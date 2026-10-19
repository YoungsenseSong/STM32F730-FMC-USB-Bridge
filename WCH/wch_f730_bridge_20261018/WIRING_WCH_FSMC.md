# CH32V307EVT → ZYNQ 单 Tx 双 Rx FPGA FSMC 接线

本表依据 `CH32V30xSCH.pdf` 的 CH32V307VCT6 页面和当前双通道工程 XDC。WCH 与 FPGA 必须共地，信号电平按 3.3 V LVCMOS 处理。先断电接线，确认无短接后再上电。

| WCH EVT | 信号 | FPGA V3.9 J3/球位 | 方向 |
|---|---|---|---|
| P1-3 | PD0 / FSMC_D2 | `fmc_ad[2]` / M19 | 双向 |
| P1-4 | PD1 / FSMC_D3 | `fmc_ad[3]` / M20 | 双向 |
| P1-7 | PD4 / FSMC_NOE_N | Y19 | WCH→FPGA |
| P1-8 | PD5 / FSMC_NWE_N | Y18 | WCH→FPGA |
| P1-9 | PD6 / FSMC_NWAIT_N | R18 | FPGA→WCH |
| P1-10 | PD7 / FSMC_NE1_N | P15 | WCH→FPGA |
| P1-12 | PB7 / FSMC_NADV | P16 | WCH→FPGA |
| P2-21 | PD15 / FSMC_D1 | `fmc_ad[1]` / N20 | 双向 |
| P2-25/26 | PD14 / FSMC_D0 | `fmc_ad[0]` / U19 | 双向 |
| P2-27 | PD9 / FSMC_D14 | `fmc_ad[14]` / W19 | 双向 |
| P2-29 | PE15 / FSMC_D12 | `fmc_ad[12]` / R17 | 双向 |
| P2-30 | PD10 / FSMC_D15 | `fmc_ad[15]` / W18 | 双向 |
| P2-31 | PE13 / FSMC_D10 | `fmc_ad[10]` / V20 | 双向 |
| P2-32 | PD8 / FSMC_D13 | `fmc_ad[13]` / R16 | 双向 |
| P2-33 | PE11 / FSMC_D8 | `fmc_ad[8]` / N17 | 双向 |
| P2-34 | PE14 / FSMC_D11 | `fmc_ad[11]` / W20 | 双向 |
| P2-35 | PE9 / FSMC_D6 | `fmc_ad[6]` / N18 | 双向 |
| P2-36 | PE12 / FSMC_D9 | `fmc_ad[9]` / P18 | 双向 |
| P2-37/39 | PE7 / FSMC_D4 | `fmc_ad[4]` / U20 | 双向 |
| P2-38/40 | PE10 / FSMC_D7 | `fmc_ad[7]` / P19 | 双向 |
| P2 bottom row | PE8 / FSMC_D5 | `fmc_ad[5]` / T20 | 双向 |
| P1 任一 GND | GND | J3/J4 GND | 电源参考 |

## 先核对的关键点

- WCH 数据线是 `D0..D15`，按原 FSMC 复用总线的网名连接；不要按排针物理顺序把它们当作连续 GPIO。P2 上个别单双排针标号在 PDF 中与网名交错，实际接线以板上网名和原理图为准。
- `PB7` 是 CH32V307 FSMC_NADV 的默认映射；本固件使用 USBFS，避免同时占用 USBHS 的 PB7。不要把 `PB7` 接到 `NE1`。
- `PD7` 是 `NE1`，`PD4` 是 `NOE`，`PD5` 是 `NWE`，`PD6` 是低有效 `NWAIT`。
- 当前 `extbus_port.c` 使用 16 位复用 FSMC、Bank1 NOR/SRAM、低有效异步等待和保守时序。`PD11/PD12/PD13` 是 A16/A17/A18；当前 CSR 和数据窗口均低于 64 KiB，先不接这三根独立高地址线，若 ILA 显示地址不稳定再补接。NWAIT 的实际波形和最大拉低时间仍需板上确认。
- 这张表只覆盖 WCH↔FPGA 总线；TX1/RX1/RX2 与 FPGA 的 SPI 接线保持 [双通道 WIRING.md](../../projects/03_dual_channel/WIRING.md) 不变。
- 当前 FPGA `prototype_zynq7020_f730_dual.xdc` 的数据/控制球位必须以实际下载的 bitstream 对照；接线前先确认 P15/P16/Y19/Y18/R18 没有被交换。

## 调试顺序

1. 断电接线并共地；WCH P7 USBFS 保持连接，WCH-Link 接 P2 调试口。
2. 先只下载 WCH 桥接镜像，确认 COM 枚举和启动日志。
3. FPGA 先下载已验证的单 Tx 双 Rx bitstream，确认 `FPGA_ID`、`PROTO_VERSION` 和 `BLOCK_STATUS` 可读。
4. 再让 WCH 读取 64 B block header，确认长度和 magic；通过后才允许读取完整块、USB 发送和 `BLOCK_ACK`。
5. 首次失败保留 WCH 串口日志、FPGA ILA、USB 原始 UBR1 和镜像 SHA256，然后回退到原 F730 或 WCH 模拟源镜像。
