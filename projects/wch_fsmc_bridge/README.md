# WCH CH32V307 FSMC/USB bridge

CH32V307 国产替代 F730 FMC→USB 桥接工程，复用冻结的 FPGA 块格式、记录 CRC、UBR1 分片和 ACK 语义。

- 类型：混合系统
- 状态：已完成 WCH USB、真实 FPGA/nRF 短窗口和双 Rx 长测；配置丢失/总线偶发错误仍保留现场诊断入口
- 负责人：Oliver
- 器件：CH32V307VCT6

## 工程入口

- 设计思路：[docs/design.md](docs/design.md)
- 工程指标：[docs/spec.md](docs/spec.md)
- 上手指南：[docs/getting-started.md](docs/getting-started.md)
- 验证记录：[docs/verification.md](docs/verification.md)
- 改进记录：[docs/changes/](docs/changes/)
- 可构建固件：[firmware/wch_f730_bridge_20261018/](firmware/wch_f730_bridge_20261018/)
- FSMC 接线：[hardware/WIRING_WCH_FSMC.md](hardware/WIRING_WCH_FSMC.md)
- 采集工具：[firmware/wch_f730_bridge_20261018/tools/](firmware/wch_f730_bridge_20261018/tools/)

## 当前状态

WCH 固件已纳入源码、协议、构建入口、release 镜像和必要的第三方声明。真实双 Rx 短窗口与长测的 CRC/序号/传输缺口通过；原始 evidence、构建缓存和旧迁移目录仍保留在本地工作区，不作为仓库发布内容。

## 构建入口

```powershell
cmake --preset fmc-bridge -S firmware/wch_f730_bridge_20261018
cmake --build firmware/wch_f730_bridge_20261018/build/fmc-bridge
```

使用 WCH 工具链时设置 `WCH_TOOLCHAIN_ROOT`；下载前保留当前镜像 SHA256，并按 [docs/getting-started.md](docs/getting-started.md) 先做 USB/FPGA 状态核对。
