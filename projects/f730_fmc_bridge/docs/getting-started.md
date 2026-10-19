# 工程上手指南

## 1. 环境

- Windows PowerShell
- F730 的 Keil/CMake 工具链
- WCH GCC12/Ninja 或 WCH 官方下载工具
- Vivado 仅用于 FPGA 配置和 FMC 配套验证

## 2. 硬件准备

先确认 F730/WCH、FPGA、USB、共地和 FMC 排线。WCH 的 `WIRING_WCH_FSMC.md` 是当前 CH32V307 接线输入。

## 3. 获取和配置

```powershell
git clone https://github.com/YoungsenseSong/STM32F730-FMC-USB-Bridge.git
cd STM32F730-FMC-USB-Bridge
```

生产 F730 源码在 `f730_bridge/`，WCH 源码在 `WCH/wch_f730_bridge_20261018/`。

## 4. 编译和下载

F730 使用既有 Keil/CMake 工程和 `f730_bridge/` 入口。WCH 使用：

```powershell
cd WCH/wch_f730_bridge_20261018
cmake --preset fmc-bridge-vscode
cmake --build --preset fmc-bridge-vscode
```

下载前保存当前镜像和 SHA256；不要使用整片擦除覆盖未知目标。

## 5. 运行和观察

USB 枚举后使用仓库工具读取 UBR1。真实数据长测使用 WCH 工程内的 `tools/real_stream_test.py`；双 RX 终端使用 `tools/dual_rx_console.py`。

## 6. 常见问题

| 现象 | 可能原因 | 排查方法 |
|---|---|---|
| USB 枚举但无帧 | FPGA 未配置或桥接状态 fault | 先读 DONE/ILA，再复位桥接端 |
| WCH 只读块头 | flags/块 CRC 兼容问题 | 对照 `Bridge_BlockHeaderDecode` 和成熟 F730 |
| COM 句柄错误 | SWD reset 后 Windows CDC 状态异常 | 重新插拔 USB 后再开采集 |
