# F730 Bridge 后续完善需求

更新时间：2026-09-03

## 已完成的离线数据面

- 248 B record、64 B block、CRC16/CRC32、block ACK 和 `UBR1` USB 帧已冻结在根仓 `bridge_contract_v0.md`。
- 当前构建采用 CDC ACM FS：data OUT `0x01`、data IN `0x81`、command IN `0x82`、64 B max packet；默认运行可靠回环诊断。
- FMC-to-USB 状态机已实现：稳定快照、分段读、完整校验、USB 完成后 ACK、重复块和错误保持所有权。
- FMC 异步低有效 NWAIT 已在源码和 `.ioc` 中一致启用。
- USART1 调试输出已配置为 PA9/PA10、115200 8-N-1，并提供 `printf` 重定向及
  `ping`/`pong` 双向检查。
- Keil 与 GNU Arm 双工具链 clean build、共享 golden tests、RTL testbench 和 Vivado `check_syntax` 已通过。

## 仍需输入，禁止猜测

### USB 产品身份与 CDC 过渡方案

当前学生科研联调镜像明确使用 `work1` 的 ST 示例 `0483:5744`、
`STMicroelectronics` 和 `STM32 Virtual ComPort`，仅用于受控实验室 CDC 功能验证。
它不是本项目获得的身份授权。对外分发前仍需取得授权来源明确的 VID/PID、Manufacturer
和 Product，再修改 `USB_DEVICE/App/bridge_usb_identity.h` 并重新执行双工具链构建。

量产优先申请本公司的 USB-IF VID，再由 VID 所有者内部唯一分配 PID；也可使用持有者
书面授权的 VID/PID 子分配。不得使用 ST 的 `0483:5744`、随机 VID/PID 或未经授权的
第三方 VID。Manufacturer 使用负责该设备的合法公司/品牌名，Product 使用稳定且可区分
型号的产品名；每台设备还应生成唯一序列号字符串。

当前 Windows 主机按 CDC ACM 绑定虚拟 COM 口，不再人工绑定 WinUSB。若后续重新选择
Vendor Bulk，才需要恢复相应类、合法身份和 WinUSB/libusb 绑定方案。

### FPGA 与板级电气

- 最终 `FPGA_ID` 和 BUILD 编码；
- 原理图审核后的 PL clock、全部 PACKAGE_PIN、IOSTANDARD 和 Bank 电压；
- PC7 是否连接 FPGA RESET、有效电平和最小保持/启动时间；
- NWAIT 最大拉低时间、FMC ADDSET/DATAST 和采样裕量；
- CH1..CH3 的 nRF-ZYNQ 引脚、电气与四路同时运行策略；CH0 已冻结为 mode 0、
  1 MHz、两个 CS 事务、DRDY 高有效和 SYNC 上升沿，但当前双 5 ms 保护间隔尚不满足
  持续吞吐；
- nRF RESET_N 的最终驱动语义；CH0 首轮明确不接。

CH0 专用测试 bitstream 已完成综合、实现并建立实板功能闭环；这不是四路或产品
bitstream。其余内容未提供前，不得从 CH0 结果外推四路/FMC/量产结论。

### I2C 固定 ID

仍需 7-bit 地址、寄存器地址宽度、固定 ID、端序、超时和重试规则。现有 BSP 仅提供
通用 HAL 封装，不写死未知设备。

## 板级 Gate

1. F730 旧安全 smoke：用户报告已通过。
2. 烧录 2026-08-18 CDC FS 镜像，验证 `0483:5744` 虚拟 COM、字节回环、PA5 包活动脉冲、拔插/复位和持续传输。
3. USART1 PA9/PA10 调试输出可并行验收，用于记录 CDC 运行状态；不要在高吞吐阶段持续打印。
4. 有审核 XDC/bitstream 后，先做 FMC 固定字、地址/数据/NWAIT，再做 A/B bank 和 ACK。
5. 接单路 nRF，验证真实 SPIS/DRDY/SYNC；最后扩至四路和持续吞吐/故障注入。

截至 2026-09-03，第 5 项的 CH0 基本功能已经达到 `records=1957`、`pop_total=1956`、
`crc_errors=0`；仍有少量 invalid/short/submit 错误和大量 nRF record queue overflow，
所以持续吞吐和长测尚未通过。F730 工作不依赖先完成四路，但 FMC 联调必须使用带
block/BRAM/FMC 的正式顶层，不能把 `ch0_test_top` 直接当作 FMC 镜像。

具体命令、预期和证据字段见 `HARDWARE_BRINGUP.md`。任何一级只证明该级，不向后外推。
