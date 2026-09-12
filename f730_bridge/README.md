# STM32F730 FMC USB Bridge

本仓库保存 STM32F730V8T6 与 FPGA 桥接固件。工程由原始厂商项目验证后，整理为 `Core`/`BSP`/`Bridge` 结构；当前同时保留 Keil 与 GNU Arm CMake/Ninja 构建。

## 当前状态

- MCU：STM32F730V8T6，LQFP100
- Keil Target：`Hal_template`
- 系统时钟：216 MHz
- USB：OTG FS Device Only，PA11/PA12，CDC ACM 虚拟串口；data OUT `0x01`、data IN `0x81`、command IN `0x82`
- FMC：Bank1/NE1，16 位地址/数据复用总线，低有效异步 NWAIT 已启用
- I2C：I2C1，PB8/PB9
- 调试串口：USART1，PA9 TX / PA10 RX，115200 8-N-1，无流控，支持 `printf`
- FPGA 控制：PC6 IRQ 安全输入轮询；PC7 RESET 保持输入
- 运行保障：TIM6 1 MHz 计数器、IWDG 主循环刷新；PA5 在收到每个 USB OUT 包时产生一次活动脉冲
- 桥接：完整块 CRC/record 校验后发送 USB，USB 完成后才按块序号低 16 位 ACK
- 最新全量构建：Keil 0 Error / 0 Warning；GNU Arm Release clean build 成功

当前默认启用 CDC FS 回环诊断：主机写入的任意字节会原样返回。接收端使用 1 KiB 环形缓冲，发送完成后才释放数据，空间不足时暂停重挂 OUT，避免 `work1` 单标志方案的静默丢包。将 `BSP_USB_CDC_ECHO_ENABLED` 改为 0 后，现有 `UBR1`/FMC 桥接状态机可改由相同 CDC data endpoints 承载；该模式尚未上板验证。

为了完成受控实验室联调，开发镜像暂时沿用 `work1` 的 ST 示例身份 `0483:5744` 和字符串。这只是技术验证配置，不表示本项目获得 ST VID/PID 授权，不能作为产品身份发布。FPGA RESET、最终 FPGA ID、完整四路/FMC XDC 和产品级持续吞吐时序仍为 `CONFIG_REQUIRED`；CH0 的 J4/SPI/DRDY/SYNC 测试接口已经冻结并建立功能闭环。

## 工程入口

- CubeMX：`Hal_template.ioc`
- Keil：`MDK-ARM/Hal_template.uvprojx`
- BSP 接口：`BSP/Inc`
- BSP 实现：`BSP/Src`
- 阶段 4 记录：`PHASE4_PROGRESS.md`
- 后续需求：`docs/f730_bridge_ROADMAP.md`
- 上电步骤：`docs/HARDWARE_BRINGUP.md`
- CDC 回环测试：`tools/cdc_echo_test.py`
- 历史 Vendor Bulk 协议工具：`tools/vendor_bulk_host.py`（当前 CDC 镜像只可运行其离线 self-test）

## 已验证构建

2026-08-18 最新 CDC FS 离线构建结果：

- Keil MDK 5.42 / Arm Compiler 6.23：Code 34,972 B、RO 568 B、RW 264 B、ZI 41,608 B，0 Error / 0 Warning；
- GNU Arm GCC Release clean build：Flash 26,728 B / 64 KiB（40.78%），RAM 45,624 B / 256 KiB（17.40%）；
- CDC host 工具通过无硬件 self-test；Keil MAP 确认链接 `USBD_CDC`，没有链接旧 `USBD_VENDOR`；
- F730 C codec 用 ArmClang `-Wall -Wextra -Werror` 编译通过。

可重复执行：

```powershell
python -B -m unittest -v protocol.test_contract
python -B f730_bridge\tools\cdc_echo_test.py --self-test
cmake --preset Release -S f730_bridge
cmake --build f730_bridge\build\Release --clean-first
```

工具链不在 PATH 时，按 `cmake/gcc-arm-none-eabi.cmake` 的 `TOOLCHAIN_ROOT` 和 `TOOLCHAIN_PREFIX` 显式指定。构建输出均由 `.gitignore` 排除。

## 发布边界

仓库不包含共享 MCP、H723 参考工程、MCP artifacts、Python 虚拟环境、第三方硬件资料或未经许可的设计文档。`硬件资料` 与工程目录中的 DOCX 仅作为本地设计依据，不随源码发布。

## 硬件验证状态

用户于 2026-08-17 报告旧 Gate A 全部通过；这只证明旧镜像的 SWD、PA5 心跳和 IWDG 持续运行。2026-08-18 CDC FS 回环镜像已离线构建，尚需重新烧录并验证 Windows 枚举、双向回环、PA5 活动脉冲、拔插恢复和持续传输；不能把离线构建写成 USB 板级通过。FMC、FPGA IRQ/RESET 和完整链路仍未上板验证。

## 系统对接与迁移

2026-09-03，nRF RX 到 ZYNQ CH0 已建立 CRC/PEEK/COMMIT 实板功能闭环；该结果只到
ZYNQ 输入端，不代表本 F730 的 FMC 或 USB 数据面已经通过。F730 下一步仍从
`docs/HARDWARE_BRINGUP.md` 的 CDC/FMC 分级 Gate 继续，不应跳过固定字、地址数据、
NWAIT 和 block ACK 测试直接做完整流。

本工程属于顶层 `F730+FPGA` Git，而不是独立仓库。迁移时必须复制顶层 `.git/`、
`protocol/`、`bridge_contract_v0.md` 和 `handoff.md`，不能只复制本目录后声称保留了
历史与共享协议。统一入口和迁移步骤见 `../README.md` 与
`../docs/WORKSPACE_MIGRATION.md`。
