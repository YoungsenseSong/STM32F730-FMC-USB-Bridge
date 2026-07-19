# 阶段 4 暂停点

更新时间：2026-07-20（Asia/Shanghai）

状态：按用户要求暂停。尚未修改阶段 4 固件源码、CubeMX 配置或 Keil 工程，尚未执行阶段 4 编译。

## 已完成

- 已读取并逐页检查 `zynq7020.docx` 全部 9 页。
- 文档标准渲染器因本机缺少隔离运行时依赖和 LibreOffice 无法使用；改用 Microsoft Word 只读导出 PDF，再生成 9 张页面图检查。
- 临时 QA 输出保存在 `F:\OliverS\AI_Embedded\.codex-docx-qa\zynq7020`，原始 DOCX 未修改。
- 已检查现有 `BSP`、`main.c`、中断文件和 Keil BSP 分组。

## 文档确认的 FPGA/FMC 约束

- FMC 为 STM32F730 16 位地址/数据复用从接口，基地址沿用 `0x60000000`。
- FPGA IRQ 为高电平有效，事件在未屏蔽且未清除时保持有效；IRQ 状态为 W1C。
- FMC 字节偏移：
  - `0x0000` FPGA_ID
  - `0x0002` PROTO_VERSION
  - `0x0004` BUILD_DATE/BUILD_HASH
  - `0x0010` GLOBAL_CTRL
  - `0x0012` GLOBAL_STATUS
  - `0x0020` IRQ_STATUS
  - `0x0022` IRQ_MASK
  - `0x0030` SYNC_CTRL
  - `0x0032` SYNC_STATUS
  - `0x0040` BLOCK_STATUS
  - `0x0042` BLOCK_LEN
  - `0x0044` BLOCK_ACK
  - `0x0100+` CHx_STATUS
  - `0x0400+` CMD/RSP_FIFO
  - `0x1000+` DATA_WINDOW
- 数据块头为 64 字节，magic 为字节序列 `FPB1`，包含版本、块序号、同步 epoch、payload 长度、通道掩码、首末 FPGA tick、四路记录数、flags 和 payload CRC32。
- 建议块 payload 为 16 KiB 或 32 KiB；READY 后数据与块头不可修改。
- A/B BRAM 所有权流程：FREE -> FILLING -> READY -> MCU_READING -> ACK -> FREE。

## 不得猜测的缺失定义

- GLOBAL_CTRL、GLOBAL_STATUS、IRQ、SYNC 和 BLOCK_STATUS 的具体位分配。
- BLOCK_ACK 的写入值/序号编码。
- FPGA_ID 与 PROTO_VERSION 的期望常量。
- CRC32 多项式、初值、反射和异或定义。
- FPGA RESET 的有效电平及板级复位连接；文档只确认 IRQ 高电平有效。
- I2C 从机地址和 I2C 固定 ID 寄存器协议。
- USB Vendor Bulk 的 VID/PID、描述符、端点编号和类实现。

## 恢复后的实施顺序

1. 通过 MCP 在隔离副本中把 PC6 从普通输入改为上升沿 EXTI，并保留 PC7 为安全输入；暂存生成、暂存编译、哈希应用。
2. 新增 `bsp_fmc`：仅实现有文档依据的寄存器偏移、16 位读写、只读数据窗口和 64 字节块头读取；不实现未知 ACK/控制位语义。
3. 新增 `bsp_fpga_ctrl`：IRQ 电平/事件计数；RESET API 明确返回 unsupported。
4. 新增通用 `bsp_i2c` HAL 封装，不写死未知器件地址或寄存器。
5. 新增 `bsp_usb` 能力状态；在 Vendor Bulk 类缺失时明确返回 unsupported，不伪造枚举。
6. 在主循环中刷新 IWDG，并启动/使用 TIM6 1 MHz 计数器。
7. 更新 Keil BSP 分组，通过显式 F730 MCP 目标执行全量编译与审计。

## 当前仓库基线

- F730 分支：`codex/f730-bsp-v1`
- 最新 F730 提交：`426bd86 Configure F730 peripherals through CubeMX MCP`
- MCP 分支：`codex/f730-target-support`
- 最新 MCP 提交：`3c7b6a6 Add staged F730 CubeMX configuration workflow`

