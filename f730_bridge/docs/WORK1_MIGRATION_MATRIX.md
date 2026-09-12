# `work1` 到 F730 主工程的迁移矩阵

更新日期：2026-08-18

> 2026-08-18 决策更新：下表保留 2026-08-16 的产品化审计结论，但用户已决定在当前
> 学生科研联调阶段采用 CDC FS。主工程没有复制 `work1` 的单 flag 回环，而是迁入 CDC
> 类并实现 1 KiB 环形缓冲、OUT 背压、发送完成所有权和 PA5 包活动脉冲。ST
> `0483:5744` 仍明确为受控实验室 test-only 身份，不改变下表关于对外发布的限制。

## 结论

`f730_bridge` 继续作为权威主工程；`work1` 只作为 USB Device 底层和
GCC/CMake 构建来源，禁止整树覆盖。排除 `work1/build` 后，两边共有 106 个
同路径文件，其中 101 个 SHA-256 完全一致。真正不同的共同文件只有
`.gitignore`、`.mxproject`、`Core/Inc/stm32f7xx_hal_conf.h`、
`Core/Src/main.c` 和 `Core/Src/stm32f7xx_it.c`。

## 移植

| 内容 | 处理方式 | 依据 |
| --- | --- | --- |
| USB Device Core | 移植 `Middlewares/ST/STM32_USB_Device_Library/Core/{Inc,Src}` | `work1/cmake/stm32cubemx/CMakeLists.txt` 已纳入 USBD Core |
| USBD-PCD 适配层 | 合并 `USB_DEVICE/Target/usbd_conf.[ch]`，保留控制请求、PCD 回调转发和 RX/TX FIFO 配置 | 主工程现在只有裸 PCD 初始化 |
| USB 初始化与描述符框架 | 复用 `usb_device.[ch]`、`usbd_desc.[ch]` 的结构，改为项目自有 Vendor Bulk 类 | `work1` 当前注册的是 CDC，身份也是 ST VCP 示例，不能原样迁入 |
| GCC/CMake 并行构建 | 迁移顶层 CMake、presets、GNU 启动文件、链接脚本、`syscalls.c`、`sysmem.c` | 已用独立输出目录和 Zephyr SDK ARM GCC 12.2 clean build 验证 |
| 主工程 CMake 清单 | 按主工程重写，加入 `BSP/Inc`、全部 `BSP/Src`、IWDG 源码和 HAL IWDG | `work1` 的清单缺少 BSP/IWDG |

合并时只能保留一套 PCD 所有权。主工程 `Core/Src/usb_otg.c` 与
`work1/USB_DEVICE/Target/usbd_conf.c` 都定义了 `hpcd_USB_OTG_FS` 和
`HAL_PCD_MspInit()`；最终应由 USBD LL 层统一驱动一份硬件配置。

## 保留主工程

| 内容 | 原因 |
| --- | --- |
| MPU/cache | 继续保留 `0x60000000` 起 64 KiB FMC 非缓存、不可缓冲、禁止取指区域和 I-Cache；`work1` 没有额外改进 |
| FMC 初始化与 BSP | 保留现有对齐访问、寄存器写白名单、稳定块快照、数据窗口和 64 B 块头解析 |
| 主循环/BSP 调度 | USB 作为 `BSP_Process()` 的非阻塞子状态机接入，不能用 CDC echo 覆盖 |
| IWDG/LSI | 保留 LSI、IWDG 初始化、HAL 模块和主循环刷新；任何长事务必须分片并持续喂狗 |
| USB IRQ | 两边实现相同，不替换整个 `stm32f7xx_it.c` |
| Keil 工程 | 继续作为正式构建之一；GNU startup 只用于并行 GCC 构建，不覆盖 ArmASM startup |

FMC/NWAIT 是两边共有的未决项，而不是 `work1` 的修复：PD6 已配置成
`FMC_NWAIT`，但 `WaitSignal` 关闭而 `AsynchronousWait` 打开。冻结电气连接和
时序前不猜值；实现必须支持在一个配置点选择“启用异步 NWAIT”或“固定保守等待
周期”，并分别留下板级验收项。

## 丢弃（不并入产品；不删除 `work1`）

| 内容 | 原因 |
| --- | --- |
| CDC 类、`usbd_cdc_if.[ch]` 和注册语句 | CDC 只保留作隔离 bring-up，最终接口是 Vendor Specific Bulk |
| ST 示例 VID/PID/字符串 | 未经分配，不能作为产品身份 |
| 单 flag CDC 回显 | flag 忙时仍重挂 OUT，会静默丢包；复制前也没有显式目标缓冲区上界检查 |
| `work1` 主循环 | 只有 CDC echo，缺少 BSP、FMC/FPGA 调度和 IWDG |
| `work1` 对 IWDG/LSI/HAL IWDG 的删除 | 与主工程运行保障冲突 |
| HardFault 点灯临时代码 | 不覆盖主工程统一故障策略 |
| `Desktop.ioc` 整体覆盖 | 它绑定 CDC/CMake 且移除了 IWDG，只参考 USB Device 部分 |
| `work1/build`、IDE 缓存和另一用户绝对路径 | 不可复现，不得提交 |
| 101 个重复 HAL/CMSIS/Core 文件 | 内容相同，不做无意义批量覆盖 |
| `starm-clang.cmake` | 当前只建立已验证的 GCC 并行构建，不扩大工具链范围 |

## 第一批合并边界

第一批只包含 USB Device Core、单一 PCD/USBD LL 适配层和可复现 GCC 构建。
主工程 BSP、MPU/cache、FMC、主循环、IWDG 和 Keil 工程保持权威；CDC 类、
CDC echo 和示例描述符均不进入最终产品实现。
