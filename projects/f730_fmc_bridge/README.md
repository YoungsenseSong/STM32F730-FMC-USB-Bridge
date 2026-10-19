# STM32F730 FMC to USB bridge

成熟 F730 工程的整理入口。实际固件源码位于仓库既有 `f730_bridge/`，WCH 替代工程位于 `WCH/wch_f730_bridge_20261018/`。

- 类型：混合系统
- 状态：已验证（单通道 FMC 基线）；WCH 长测仍在继续
- 负责人：Oliver
- 主链路：16 位复用 FSMC/FMC -> USB FS CDC

## 工程入口

- 设计思路：[docs/design.md](docs/design.md)
- 工程指标：[docs/spec.md](docs/spec.md)
- 上手指南：[docs/getting-started.md](docs/getting-started.md)
- 验证记录：[docs/verification.md](docs/verification.md)
- 改进记录：[docs/changes/](docs/changes/)
- F730 源码：[../../f730_bridge/](../../f730_bridge/)
- WCH 替代：[../../WCH/wch_f730_bridge_20261018/](../../WCH/wch_f730_bridge_20261018/)

## 版本里程碑

| 版本 | 日期 | 内容 | 状态 |
|---|---|---|---|
| v0.1 | 2026-10-19 | 成熟 F730 与 WCH 工程整理 | 已验证/持续验证 |
