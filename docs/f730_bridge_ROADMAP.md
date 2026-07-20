# F730 Bridge 后续完善需求

本文记录第一版之后继续完整桥接所需的外部定义、实现任务和验收边界。未获得下列输入前，不在固件中猜测协议值、硬件极性或实验结果。

## 第一版已完成

- STM32F730V8T6 `Core`/`BSP` 工程结构和 Keil 分组。
- USB OTG FS Device Only、FMC Bank1 16 位复用总线、I2C1、IWDG、TIM6 和安全 GPIO 配置。
- FMC 16 位底层访问、文档给出的寄存器偏移、数据窗口读取及 64 字节 `FPB1` 块头解析。
- 通用 7 位 I2C 主机传输封装。
- FPGA IRQ 高电平轮询、事件统计和 RESET unsupported 能力报告。
- USB PCD 状态和 FS 64 字节最大包能力报告；未伪造 USB 类或枚举。
- 显式 F730 MCP 构建、CubeMX 24/24 审计、HEX 地址范围校验及 MCP 回归。

## P0：USB Vendor Bulk

需要提供或确定：

- USB VID/PID 及其授权来源。
- Manufacturer、Product、Serial Number 字符串策略。
- Configuration、Interface 和 Endpoint 描述符。
- Bulk IN/OUT 端点号、缓冲策略及超时规则。
- Vendor Bulk 类实现方案，或允许采用的 USB Device Middleware 版本。
- 控制端点上的厂商请求、协议版本查询和错误恢复行为。

验收目标：

- USB FS Device Only 稳定枚举，MaxPacket 为 64 字节。
- Bulk OUT 到 Bulk IN 回环通过。
- 断开重连、主机取消传输和总线复位可恢复。

## P0：FPGA 寄存器和 FMC 握手

需要 FPGA 侧提供：

- `GLOBAL_CTRL`、`GLOBAL_STATUS`、`IRQ_STATUS`、`IRQ_MASK`、`SYNC_CTRL`、`SYNC_STATUS`、`BLOCK_STATUS` 的完整位定义。
- `BLOCK_ACK` 写入值、块序号或缓冲区编号编码，以及重复 ACK 的处理方式。
- `FPGA_ID`、`PROTO_VERSION` 和兼容性判断常量。
- READY、MCU_READING、ACK、FREE 状态转换的原子性与超时规则。
- CMD/RSP FIFO 的字宽、深度、满空条件和命令帧格式。

验收目标：

- 固定寄存器读写、W1C IRQ 清除和 A/B 缓冲所有权转换通过。
- 非法状态、重复块、超时和 FPGA 复位后能够恢复。

## P0：CRC32 定义

需要明确：

- 多项式。
- 初始值。
- 输入/输出反射方式。
- 最终异或值。
- CRC 覆盖范围以及块头字段是否参与计算。
- 至少一组 FPGA 生成的输入与期望 CRC 测试向量。

验收目标：STM32 与 FPGA 对同一测试向量和实际数据块得到一致结果，错误块不会发送至 USB。

## P0：FPGA RESET 和 IRQ

需要明确：

- PC7 是否确实连接 FPGA RESET。
- RESET 有效电平、最小保持时间和释放后的就绪时间。
- PC6 IRQ 的板级上拉/下拉、电气类型和去抖要求。
- 是否允许把 PC6 从轮询输入改为 EXTI，以及中断优先级约束。

验收目标：可控复位不会产生总线争用；IRQ 在持续高电平和并发事件情况下不丢失。

## P1：I2C 固定 ID 协议

需要明确：

- 7 位从机地址。
- 器件或 FPGA I2C 从机类型。
- ID 寄存器地址、宽度、字节序和期望值。
- 寄存器地址宽度、重复起始要求、超时和重试策略。

验收目标：上电读取固定 ID，能够区分设备缺失、总线占用、NACK 和 ID 不匹配。

## P1：完整桥接和恢复

在以上协议冻结后实现：

1. USB Vendor Bulk 回环。
2. FMC 固定值和寄存器测试。
3. FMC 块头、payload 和 CRC 校验。
4. A/B 双缓冲及 ACK。
5. FMC-to-USB 持续传输与反压。
6. USB、FMC、FPGA 与 I2C 异常恢复。
7. 诊断计数器、协议版本和错误状态查询。

验收需要记录板卡版本、FPGA bitstream 版本、固件提交、测试命令、吞吐量、丢块数、CRC 错误数和恢复结果；没有真实硬件日志时不得标记完成。

## 当前阻塞输入清单

- USB VID/PID、描述符、端点规划和 Vendor Bulk 类实现。
- FPGA 控制/状态位定义、ACK 编码、ID 常量和完整握手协议。
- CRC32 全部参数及测试向量。
- FPGA RESET 极性和板级连接定义。
- I2C 地址、固定 ID 寄存器和访问协议。
- 可用于 DAPLink、USB、FMC 和 FPGA 联调的硬件环境。
