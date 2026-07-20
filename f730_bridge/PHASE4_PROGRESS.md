# 阶段 4 进度记录

更新时间：2026-07-20（Asia/Shanghai）

状态：已从暂停点恢复，并完成有文档依据的阶段 4 BSP 骨架、Keil 全量编译和 MCP 审计。完整 USB Vendor Bulk、FPGA ACK/CRC/RESET 和板上联调仍受缺失定义约束。

## 恢复后的完成结果

- 新增 `bsp_fmc`、`bsp_i2c`、`bsp_usb` 和 `bsp_fpga_ctrl`，并加入 Keil `Drivers/BSP` 分组。
- `bsp_fmc` 实现 16 位对齐访问、已知寄存器偏移、只读数据窗口和 64 字节块头解析；未实现未知 ACK/控制位/CRC 语义。
- PC6 保持 CubeMX 安全输入并按高电平有效进行轮询。尝试通过 MCP 改为 EXTI 时，CubeMX 预检两次停在第三方组件模型扫描并超时，因此没有绕过 MCP 或直接修改 `.ioc`。
- PC7 保持输入；复位 API 明确返回 `BSP_STATUS_UNSUPPORTED`。
- I2C 仅提供通用 7 位地址 HAL 封装；USB 仅报告 PCD 能力，Vendor Bulk 明确不可用。
- TIM6 作为 1 MHz 16 位计数器启动；主循环刷新 IWDG。
- 显式 `STM32F730V8T6` MCP 全量构建：0 Error、0 Warning；Code=18388、RO=528、RW=12、ZI=5628。
- CubeMX 配置审计 24/24、目标身份审计、项目审计和 HEX 地址范围校验均通过；未进行硬件下载。

## 暂停时已完成

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

## 暂停时仓库基线

- F730 分支：`codex/f730-bsp-v1`
- 最新 F730 提交：`426bd86 Configure F730 peripherals through CubeMX MCP`
- MCP 分支：`codex/f730-target-support`
- 最新 MCP 提交：`3c7b6a6 Add staged F730 CubeMX configuration workflow`
