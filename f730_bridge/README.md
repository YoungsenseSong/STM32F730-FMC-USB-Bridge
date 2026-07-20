# STM32F730 FMC USB Bridge

本仓库保存 STM32F730V8T6 与 FPGA 桥接固件的第一版工程。工程由原始厂商项目验证后，整理为与既有 H723 工程一致的 `Core`/`BSP` 结构，并通过共享 STM32/FPGA MCP 的显式 F730 目标完成 CubeMX 审计和 Keil 构建。

## 当前状态

- MCU：STM32F730V8T6，LQFP100
- Keil Target：`Hal_template`
- 系统时钟：216 MHz
- USB：OTG FS Device Only，PA11/PA12，底层 PCD 已配置
- FMC：Bank1/NE1，16 位地址/数据复用总线
- I2C：I2C1，PB8/PB9
- FPGA 控制：PC6 IRQ 安全输入轮询；PC7 RESET 保持输入
- 运行保障：TIM6 1 MHz 计数器、IWDG 主循环刷新、PC13 低有效 LED 心跳
- 最新全量构建：0 Error、0 Warning

第一版已经提供 FMC、I2C、USB 能力和 FPGA 控制 BSP 骨架。USB Vendor Bulk、完整 FPGA 握手及连续 FMC-to-USB 桥接尚未实现；缺失定义不会用占位常量或假协议代替。

## 工程入口

- CubeMX：`Hal_template.ioc`
- Keil：`MDK-ARM/Hal_template.uvprojx`
- BSP 接口：`BSP/Inc`
- BSP 实现：`BSP/Src`
- 阶段 4 记录：`PHASE4_PROGRESS.md`
- 后续需求：`docs/f730_bridge_ROADMAP.md`

## 已验证构建

使用共享 MCP 工程 `F:\OliverS\AI_Embedded\stm32-fpga-mcp`，显式指定芯片：

```python
server.stm32_build(clean=True, chip="STM32F730V8T6")
```

验证环境：Keil MDK 5.42、Arm Compiler 6.23。最近一次程序占用：Code 18,388 B、RO 528 B、RW 12 B、ZI 5,628 B。

构建生成的 AXF、HEX、MAP、Keil 临时目录均由 `.gitignore` 排除，请在本机重新构建获得。

## 发布边界

仓库不包含共享 MCP、H723 参考工程、MCP artifacts、Python 虚拟环境、第三方硬件资料或未经许可的设计文档。`硬件资料` 与工程目录中的 DOCX 仅作为本地设计依据，不随源码发布。

## 硬件验证状态

当前尚未发现可用调试探针，未执行 DAPLink 下载、USB 枚举、FMC 板级读写、FPGA IRQ/RESET 或持续吞吐测试。因此仓库中的结果仅代表静态审计、镜像地址校验和编译验证，不代表板上测试已完成。
