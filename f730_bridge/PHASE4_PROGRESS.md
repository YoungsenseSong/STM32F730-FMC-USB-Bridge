# 阶段 4 进度记录

更新时间：2026-09-03（Asia/Shanghai）

状态：用户报告旧 Gate A 全部通过；F730 当前下一步仍为 CDC FS 回环镜像的板级枚举和
双向传输验收。nRF RX 到 ZYNQ CH0 已建立 CRC/PEEK/COMMIT 实板功能闭环，但 F730
尚未接入；完整四路/FMC 约束、FPGA RESET 和产品级持续吞吐仍受门控。系统总状态与
迁移入口见 `../README.md`、`../handoff.md` 和 `../docs/WORKSPACE_MIGRATION.md`。

## 2026-08-18 USB CDC FS 回环

- 按用户决定，主工程当前 USB 类从未启用的 Vendor Bulk 切换为 `work1` 同源 CDC ACM FS；
  data OUT `0x01`、data IN `0x81`、command IN `0x82`，max packet 64 B。
- 开发镜像沿用 `work1` 的 ST 示例 `0483:5744` 和字符串，仅用于受控实验室验证；代码中
  标记 `BRIDGE_USB_IDENTITY_TEST_ONLY=1`，不冒充本项目获得正式授权。
- 默认 `BSP_USB_CDC_ECHO_ENABLED=1`：1 KiB RX 环形缓冲，发送完成后才释放；空间不足时
  暂停重挂 OUT，避免 `work1` 单 flag 在连续包下静默丢失。
- PA5 不再执行 500 ms 心跳，而是按每个 USB OUT packet 排队执行 60 ms 活动和 60 ms
  间隔；USART1 每秒 heartbeat 保留。当前 LED 有效极性仍待正确板卡资料确认。
- 桥接 codec、FMC 状态机和 `UBR1` 解析仍保留；回环通过后把上述宏改为 0，即可继续将
  CDC data endpoints 用作二进制桥接传输，但该路径尚未上板验证。
- Keil AC6 rebuild：Code=34,972、RO=568、RW=264、ZI=41,608，0 Error / 0 Warning；
  SHA-256：AXF `a4d930add70e0951d2d4469d334b24475aab88756687229868f018f22e96c01a`，
  HEX `bb66723bed617d68c8868952c6b2c266c6198aa4c940a753d0a02fff6270c4c5`。
- GNU Arm Release clean build：Flash=26,728 B（40.78%），RAM=45,624 B（17.40%）；
  SHA-256：ELF `7b71cdea4bbd539d24809759dfcea084050d3a0d21849162ed58f8034ef8c29b`，
  HEX `5e083ff0bc0e957503974a41b5a916ae1c18e5b84114692955ec8175ebf65466`。
- `tools/cdc_echo_test.py --self-test` 通过；MAP 确认最终镜像链接 `USBD_CDC`，不链接
  `USBD_VENDOR`。这些仍是离线证据，USB 枚举、PA5 包活动和真实回环等待开发板验证。

## 2026-08-16 离线收尾

- 冻结 248 B nRF record、64 B FPGA block、反射 CRC32/IEEE、低 16 位 block ACK 和 24 B `UBR1` USB 帧；golden vectors 位于根仓 `protocol/golden/`。
- 实现 Vendor Specific Bulk（FS、OUT `0x01`、IN `0x81`、64 B）、OUT 背压、IN 单所有权、USB 分片/粘包命令解析以及断线重传状态。
- 实现 FMC 稳定快照、512 B 分段读取、完整 block/record/CRC 校验、USB 完成后 ACK、重复块处理和三次失败后保持不 ACK 的故障态。
- 启用 FMC 异步低有效 NWAIT；最终时序裕量仍须逻辑分析仪验证。
- USB 身份保留 `BRIDGE_USB_IDENTITY_CONFIGURED=0`，未复用 ST CDC 示例 VID/PID；当前镜像不会枚举。
- Keil 最终 rebuild：Code=31,372、RO=544、RW=240、ZI=41,760，0 Error / 0 Warning。
- GNU Arm GCC 12.2 Release clean build：Flash=23,584 B（35.99%）、RAM=46,128 B（17.60%）。
- 协议测试 9/9、Vendor host self-test、三项 RTL testbench 和 Vivado `check_syntax` 均通过；未综合、实现或生成 bitstream。
- nRF 工程只参考远端提交 `4e360863a3569c9494197ec701604404519d811d`，没有修改。

## 2026-08-17 Gate A 板型修正

- 用户确认现场板载指示 LED 为 PA5；随附原理图属于错误板卡版本。
- 主工程源码与 `.ioc` 已迁移到 PA5；PA5 与当前 USB/I2C/FMC/TIM6 配置无冲突。
- Keil rebuild：Code=31,356、RO=544、RW=240、ZI=41,760，0 Error / 0 Warning。
- GNU Arm GCC 12.2 Release clean build：Flash=23,580 B（35.98%）、RAM=46,128 B（17.60%）。
- 协议测试 9/9 与 Vendor host self-test 继续通过；LED 有效电平待正确原理图确认，Gate A
  仅检查每 500 ms 稳定翻转。

用户于 2026-08-17 报告 Gate A 全部通过。该结论是用户实测结果，不补造未回传的电流、
视频或下载日志；不要把 PA5 心跳通过写成 USB/FMC/全链路通过。

## 2026-08-17 USART1 调试口

- 新板 MCU 与板载外设沿用现有资料，只把排针位置差异保留为接线前核对项。
- 芯片资料确认 PA9/PA10 为 USART1_TX/RX（AF7）；现有工程无复用冲突。
- 配置 115200 bit/s、8 data bits、no parity、1 stop bit、no flow control，轮询发送；
  `BSP_Debug_Write*` 和 `printf` 均重定向至 USART1。
- 启动时输出 `F730 bridge boot: USART1 115200 8N1` 和
  `F730 bridge ready; send ping`，之后每秒输出 `F730 bridge heartbeat`；主循环非阻塞接收
  `ping` 并返回 `pong`，用于首次双向串口验收。
- Keil rebuild：Code=34,880、RO=584、RW=240、ZI=41,936，0 Error / 0 Warning。
- GNU Arm GCC 12.2 Release clean build：Flash=25,912 B（39.54%）、RAM=46,320 B（17.67%）。
- 协议测试 9/9 与 Vendor host self-test 继续通过；USART1 仍需开发板实测。

以下内容是 2026-07-20 的历史快照，用于保留决策依据；其中“未实现/未知”项以本文件上方的 2026-08-16 状态和根仓 `handoff.md` 第 9 节为准。

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
