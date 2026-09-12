# STM32F730 工程交接（R005/r1）

## 工程身份与用途
- 本机开发目录：`F:/OliverS/MultiSensorResearch/F730+FPGA/f730_bridge`；实际 Git 根是其父目录 `F:/OliverS/MultiSensorResearch/F730+FPGA`，不是独立仓。
- HEAD `bd7d6de5399b226ef3b667d54b14304c9c82655a`，分支 `codex/work1-validation`，上游 `origin/codex/work1-validation`，本轮前后 clean。
- `zynq7020/` 是独立嵌套仓，绝不能作为 F730 提交的一部分。
- 用途：STM32F730V8T6 通过 FMC 接 FPGA 块数据并通过 USB Device 输出到主机；同时保留 CDC 回环诊断和 USART1 调试。

## 当前阶段与证据分层
- **源码可开发：是。** CubeMX `.ioc`、Keil 工程、GNU Arm CMake/Ninja、HAL/CMSIS/USB 源码、BSP/Bridge 和 host 工具均在仓内。
- **本轮固件构建：未通过/未完成。** 隔离 CMake configure 退出 1，原因是本机未找到配置所需的兼容 `arm-none-eabi-*`/`arm-zephyr-eabi-*` 编译器；因此 build 标记 `NOT_RUN_CONFIGURE_FAILED`。不得从 nRF/ZYNQ 构建推断 F730 已通过。
- **本轮无板测试：部分通过。** 共享协议 9/9 单元测试退出 0；CDC host self-test 退出 0。
- **历史构建陈述：仅文档证据。** 生产 README 记录 2026-08-18 Keil 5.42/Arm Compiler 6.23 与 GNU Arm Release clean build 成功；本轮未独立复现。
- **已有板测证据：有限。** 用户报告旧 Gate A 通过，只能证明旧镜像的 SWD/PA5/IWDG 基础 smoke；当前 CDC 镜像、FMC、FPGA IRQ/RESET、FMC→USB 完整链路均未板测。nRF→ZYNQ 的闭环不证明 F730。

## 环境依赖
- MCU/封装：STM32F730V8T6，LQFP100；STM32Cube FW_F7 V1.17.4（来自 `.ioc`）。
- 构建路径一：Keil MDK 5.42 + Arm Compiler 6.23（历史记录，本机本轮未定位到 UV4/ArmClang）。
- 构建路径二：CMake 3.22+、Ninja、GNU Arm Embedded GCC；通过 `TOOLCHAIN_ROOT`/`TOOLCHAIN_PREFIX` 指定工具链。本机 CMake 4.3.0-rc2、Ninja 1.11.1 可用，但 ARM GCC 不可用。
- 主机测试：Python 3.12.3；实机 CDC 测试需要 pyserial、Windows COM 设备和明确授权。
- 共享依赖：同一 F730 Git 根下的 `protocol/`、`bridge_contract_v0.md`；协议测试还只读引用嵌套 ZYNQ 的 `protocol_defs.vh`，跨仓发布时必须固定对应 ZYNQ commit。

## 从仓库开始开发
1. 克隆 F730 仓；确认 `f730_bridge/`、`protocol/`、顶层合同/交接均存在，且不要把独立 `zynq7020/.git` 混入。
2. 选择 Keil 或 GNU Arm 工具链，并记录精确版本。Keil 打开 `f730_bridge/MDK-ARM/Hal_template.uvprojx`；CubeMX 仅在明确需要重生成时打开 `Hal_template.ioc`，不得覆盖手工层。
3. GNU Arm：`cmake --preset Release -S f730_bridge`，若工具不在 PATH，加入 `-DTOOLCHAIN_ROOT=<toolchain-root> -DTOOLCHAIN_PREFIX=arm-none-eabi-`；随后 `cmake --build f730_bridge/build/Release --clean-first`。
4. 先跑 `python -B -m unittest -v protocol.test_contract` 与 `python -B f730_bridge/tools/cdc_echo_test.py --self-test`。
5. 所有构建/测试输出写入忽略目录或仓外，不覆盖历史证据。

## 构建与测试入口
- Keil：`f730_bridge/MDK-ARM/Hal_template.uvprojx`，Target `Hal_template`。
- GNU Arm：`f730_bridge/CMakePresets.json`、`f730_bridge/cmake/gcc-arm-none-eabi.cmake`。
- 协议：`protocol/test_contract.py`。
- CDC：`f730_bridge/tools/cdc_echo_test.py --self-test`；实机步骤见 `f730_bridge/docs/HARDWARE_BRINGUP.md`。
- 本轮实际结果见 [验证摘要](HANDOFF_VALIDATION.md)；固件 build 不是 PASS。

## 烧录前条件
- 先由 Astra/用户批准 commit、工具链、具体 HEX 哈希和板卡；重新 clean build 并保存 MAP/size/日志。
- Keil 必须使用 CMSIS-DAP 与正确 STM32F7 Flash algorithm，不得处于 Simulator。
- 先按 Gate A/A.1/B 分级验证，FMC/FPGA 未接入前验证当前 USB 身份边界；`0483:5744` 是实验室 ST 示例身份，无产品分发授权。
- Gate C 前必须审核实际 XDC、Bank 电压、PL clock、FMC 引脚、NWAIT 极性/时序；当前 `ch0_test_top` 不含 F730 数据面。

## 回退
- 保留并核对上一已知镜像、MAP 和 SHA-256；固件与相应 FPGA/合同版本成套回退。
- 源码用 Git commit/tag 回退；不删除唯一 HEX、失败日志或板测记录，不用 `reset --hard` 覆盖 dirty 工作。
- USB 身份或 FMC gate 失败时回到前一 Gate 的镜像/连接状态，而不是绕过安全门。

## 已知问题与首个建议任务
- 本轮缺可用 ARM GCC/Keil，因此无法独立确认 F730 固件构建；这是发布前需解除的环境阻塞。
- CDC 默认仍是诊断回环；`BSP_USB_CDC_ECHO_ENABLED=1` 时不能声称 FMC→host 数据链。
- ST 示例 VID/PID/字符串不可作为产品身份发布；FPGA ID、完整四路/FMC XDC、RESET 和持续吞吐仍需配置/验证。
- 首个建议任务：在固定工具链的隔离 clean build 中复现 Keil或GNU构建并记录哈希，然后只按 `HARDWARE_BRINGUP.md` 进入下一未通过 Gate。


## 本次发布说明

本文件基于已冻结的 R005/r1 Opus 交付，由 Astra 核对清单和当前 Git 基线后整理。上述 HEAD 是被验证的源码基线，不是加入交接文档后的提交号。构建与仿真由 Opus 执行；Astra 本次不宣称独立重跑全部构建。仅发布现有开发分支和说明，不新增稳定版或板测通过声明。
