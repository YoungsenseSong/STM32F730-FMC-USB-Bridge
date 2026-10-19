# WCH 上手指南

## 1. 工程路径

```text
projects/wch_fsmc_bridge/
├─ firmware/wch_f730_bridge_20261018/  # 可构建固件、协议、测试、发布镜像
├─ hardware/WIRING_WCH_FSMC.md         # FSMC 接线输入
├─ scripts/                            # 仓库级说明
└─ results/                            # 整理后的结果入口
```

## 2. 构建

```powershell
$env:WCH_TOOLCHAIN_ROOT='F:\OliverS\_toolcache\wch\gcc12\riscv-none-elf-gcc-12-win-1.92'
cmake --preset fmc-bridge -S projects/wch_fsmc_bridge/firmware/wch_f730_bridge_20261018
cmake --build projects/wch_fsmc_bridge/firmware/wch_f730_bridge_20261018/build/fmc-bridge
```

## 3. 下载与运行

按 `hardware/WIRING_WCH_FSMC.md` 接线，下载前保存镜像 SHA256。先验证 USB 枚举，再接入 FPGA，最后用 `firmware/.../tools/real_stream_test.py` 做短测或长测。

## 4. 故障边界

FPGA DONE=0、WCH fault、USB 句柄错误和 RF gap 分层记录；现场复发时先留状态再复位。
