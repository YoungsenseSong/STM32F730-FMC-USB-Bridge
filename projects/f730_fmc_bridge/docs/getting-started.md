# 上手指南

## 1. 环境

- Windows PowerShell
- F730 的 Keil 或 Arm GNU 工具链
- WCH RISC-V Embedded GCC12、CMake、Ninja
- Vivado 仅用于 FPGA 配置和 FMC 配套验证

## 2. 工程入口

- F730：`projects/f730_fmc_bridge/firmware/f730_bridge/`
- WCH：`projects/wch_fsmc_bridge/firmware/wch_f730_bridge_20261018/`
- WCH 接线：`projects/wch_fsmc_bridge/hardware/WIRING_WCH_FSMC.md`

## 3. 编译

F730：

```powershell
cmake --preset Release -S projects/f730_fmc_bridge/firmware/f730_bridge
cmake --build projects/f730_fmc_bridge/firmware/f730_bridge/build/Release --clean-first
```

WCH：

```powershell
cmake --preset fmc-bridge -S projects/wch_fsmc_bridge/firmware/wch_f730_bridge_20261018
cmake --build projects/wch_fsmc_bridge/firmware/wch_f730_bridge_20261018/build/fmc-bridge
```

## 4. 下载与观察

下载前保存当前镜像和 SHA256，不使用整片擦除覆盖未知目标。WCH USB 枚举后使用其 `firmware/.../tools/real_stream_test.py` 读取 UBR1；真实数据长测前先核对 FPGA DONE/ILA 状态。

## 5. 常见问题

| 现象 | 可能原因 | 排查方法 |
|---|---|---|
| USB 枚举但无帧 | FPGA 未配置或桥接状态 fault | 先读 DONE/ILA，再复位桥接端 |
| WCH 只读块头 | flags/块 CRC 兼容问题 | 对照 WCH 工程的协议解码与成熟 F730 |
| COM 句柄错误 | SWD reset 后 Windows CDC 状态异常 | 重新插拔 USB 后再开采集 |
