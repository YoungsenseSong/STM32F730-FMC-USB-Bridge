# STM32F730 FMC/USB 与 WCH FSMC/USB 工程

本工程按仓库模板整理为两个可独立复建的工程入口：

- F730 成熟桥接：[`firmware/f730_bridge/`](firmware/f730_bridge/)
- WCH 国产替代桥接：[`../wch_fsmc_bridge/`](../wch_fsmc_bridge/)

- 类型：混合系统
- 状态：F730 单通道完整链路已验证；WCH 双 Rx 短窗口与长测已验证，复发故障仍需现场留证
- 主链路：16 位复用 FMC/FSMC → MCU → USB FS CDC

## 工程入口

- 设计思路：[docs/design.md](docs/design.md)
- 工程指标：[docs/spec.md](docs/spec.md)
- 上手指南：[docs/getting-started.md](docs/getting-started.md)
- 验证记录：[docs/verification.md](docs/verification.md)
- 改进记录：[docs/changes/](docs/changes/)
- F730 固件：[firmware/f730_bridge/](firmware/f730_bridge/)
- F730 Keil/CMake 入口：[firmware/f730_bridge/CMakeLists.txt](firmware/f730_bridge/CMakeLists.txt)
- WCH 工程：[../wch_fsmc_bridge/](../wch_fsmc_bridge/)

## 目录约定

`firmware/` 放可构建源码，`hardware/` 放接线与板级输入，`scripts/` 放仓库级入口，`results/` 只放整理后的验证结果；构建缓存、原始现场证据和第三方全量示例不进入发布目录。
