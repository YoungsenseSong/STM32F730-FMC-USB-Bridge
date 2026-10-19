# F730 / ZYNQ Bridge 分级上电清单

更新时间：2026-09-03

本清单从当前离线通过状态开始。每次只引入一个硬件变量，并保存板卡版本、固件/RTL
提交、镜像 SHA-256、供电、电流、示波器/逻辑分析仪和主机日志。离线构建成功不能替代
任一上电 Gate。

> 跨工程状态：nRF RX 到 ZYNQ CH0 已建立 SPI CRC/PEEK/COMMIT 功能闭环，但当前
> `ch0_test_top` 不含 F730 FMC 数据面，且 1 MHz/双 5 ms 保护造成 nRF record queue
> overflow。该证据不能跳过本文件的 Gate B/C/D。

## Gate A：F730 安全 smoke（用户报告全部通过）

> 2026-08-17 用户确认 Gate A 全部通过。该记录仅采用用户给出的通过结论，不补造尚未
> 回传的电流、视频或下载日志。新板 MCU 与板载外设按现有资料开发，排针位置按实物核对；
> 板载指示 LED 继续以用户确认的 PA5 为准。

使用 Keil 输出：

```text
MDK-ARM/Hal_template/Hal_template.hex
SHA-256 6023d80386f73a47b08b95985351bab6c2f6d5493e2f514a98030e93f2a2f974
```

当前 Keil 工程已按本机的 DAPLink/CMSIS-DAP 保存为硬件调试模式。下载前在
`Options for Target` 中复核：

1. `Debug -> Use` 为 `CMSIS-DAP Debugger`，而不是 `Simulator`；
2. `Utilities` 勾选 `Use Target Driver for Flash Programming`；
3. `Flash Download` 中主 Flash 算法为 `STM32F7x_64_AXI.FLM`，地址
   `0x08000000`、大小 `0x00010000`；`STM32F72x_73x_OPT.FLM` 仅用于 Option
   Bytes，不能代替主 Flash 算法。

若日志仍出现 `Using ARM Cortex-M4 Simulator` 或 `No Algorithm found for:
08000000H`，停止 Gate A；这表示尚未真正烧录 MCU。关闭并重新打开工程后再次复核上述
三项。

该镜像保持 `BRIDGE_USB_IDENTITY_CONFIGURED=0`，因此预期行为是：

1. SWD 可连接、擦写和复位；
2. PA5 板载指示 LED 每 500 ms 翻转，完整周期约 1 s；
3. 连续运行至少 2 min，不因 IWDG 周期性复位，SWD 仍可重新连接；
4. USB 不应枚举，也不应使用 ST CDC 的 `0483:5744`；
5. 未配置 USB host 前，桥接状态机不读取 FMC 数据窗口、不 ACK FPGA block。

PA5 由现场实物确认；Gate A 只检查稳定翻转，不以初始亮/灭状态判定成败。

请记录：开发板版本、供电电压/静态电流、下载工具、LED 视频或时间记录、2 min 后
SWD 状态，以及 Windows 设备管理器是否出现未知 USB 设备。若 LED 不稳定，先不要连接
FPGA/nRF，回传复位原因、SWD halt PC/LR 和供电波形。

## Gate A.1：USART1 调试输出

本 Gate 使用加入串口后的新 Keil 镜像，不能用上面的旧 Gate A 哈希代替：

```text
MDK-ARM/Hal_template/Hal_template.hex
SHA-256 66a791f7fe016e2a5175a70081bebc1319e9318ff0e7c5223973c9d46d175d04
```

USART1 配置：PA9 TX、PA10 RX、115200 bit/s、8 data bits、no parity、1 stop bit、
no flow control。使用 3.3 V TTL USB-UART 模块，开发板已经供电时不要再接模块 VCC：

| F730 | USB-UART |
| --- | --- |
| PA9 / USART1_TX | RXD |
| PA10 / USART1_RX | TXD（仅看打印时可暂不接） |
| GND | GND |

新板排针位置与现有资料不同，接线前按实物丝印/连续性确认 PA9、PA10 和 GND；禁止接
RS-232 电平或 5 V TTL。复位后终端应依次看到：

```text
F730 bridge boot: USART1 115200 8N1
F730 bridge ready; send ping
F730 bridge heartbeat
```

之后每秒一条 heartbeat。在终端发送 `ping` 加回车，必须收到 `pong`。连续观察至少 2 min，
要求无乱码、无重复启动 banner、LED 仍按 500 ms 翻转且 SWD 可重连。
`printf("value=%lu\r\n", value);` 已可直接用于后续调试；
当前实现为轮询阻塞发送，不要在中断里打印，在高吞吐/严格时序测试前应关闭周期打印或
改为 DMA/ring buffer。

## Gate B：USB CDC FS 回环（当前下一步）

烧录当前 Keil 镜像：

```text
MDK-ARM/Hal_template/Hal_template.hex
SHA-256 bb66723bed617d68c8868952c6b2c266c6198aa4c940a753d0a02fff6270c4c5
```

该开发镜像沿用 `work1` 的 ST 示例身份：VID `0x0483`、PID `0x5744`、Manufacturer
`STMicroelectronics`、Product `STM32 Virtual ComPort`。这只用于学生项目受控实验室验证，
不表示本项目获得 ST VID/PID 授权，不得作为产品身份发布。

1. FPGA 和 nRF 暂不接入，只连接 F730 的 USB OTG FS 设备口；不要误接 DAPLink 调试口。
2. Windows 设备管理器应出现新的 COM 口；在“硬件 ID”中保存
   `USB\VID_0483&PID_5744` 截图，并记录实际 COM 号。
3. PA5 空闲时保持灭灯状态；每收到一个 USB OUT packet 执行约 60 ms 活动状态和 60 ms
   间隔。PA5 有效电平仍待正确原理图确认，因此若观察到“平时亮、接收时灭”，先记录
   极性，不据此判定 USB 失败。
4. 先运行主机逻辑自测，再把 `COMx` 替换成设备管理器中的端口：

```powershell
python -B f730_bridge\tools\cdc_echo_test.py --self-test
python -B f730_bridge\tools\cdc_echo_test.py --port COMx `
  --packet-size 64 --rounds 100 --burst 4 --read-chunk 17
python -B f730_bridge\tools\cdc_echo_test.py --port COMx `
  --packet-size 1024 --rounds 100 --burst 1 --read-chunk 64
```

5. 再验证 20 次 USB 拔插、20 次 MCU reset，以及至少 30 min 连续回环。保存脚本输出、
   Windows 枚举信息、PA5 视频和 USART1 日志。CDC 的 baud 参数只是主机 line coding，
   不改变 USB 实际传输速率。

通过条件：任意二进制逐字节原样返回；无 timeout/data mismatch；无永久 BUSY；拔插和复位
可恢复；PA5 对每个实际 OUT packet 排队产生一次活动脉冲。当前源码使用 1 KiB 环形缓冲，
只有 IN 发送完成后才释放对应数据，缓冲不足时停止重挂 OUT 而不是静默丢包。

本 Gate 只验证 PC↔F730 CDC FS，不验证 FMC、FPGA、nRF 或最终产品 USB 身份。

## Gate C：FMC 固定模式

只有在审核后的 XDC、Bank 电压、PL clock、FMC 引脚和测试 bitstream 可用后开始。先不接
nRF：固定 ID/版本、`55AA/AA55`、走马灯、1 KiB/16 KiB/32 KiB 数据和至少 `10^6`
个 16-bit 字。逻辑分析仪同时观察 NADV、NE1、NOE、NWE、AD[15:0]、NWAIT。

通过条件：数据/地址/CRC 错误为 0；NWAIT 极性和采样一致；MCU/FPGA reset 后可重新读
ID。当前没有 XDC/bitstream，因此本轮未执行 Gate C。

## Gate D：单路数据和完整链路

真实 nRF SPIS backend、CPOL/CPHA、SCLK、CS 周转及 DRDY/SYNC 极性冻结后，先接一路。
当前 `BSP_USB_CDC_ECHO_ENABLED=1`，因此还不能执行 FMC-to-host 数据 Gate。完成 Gate B 后，
先将其改为 0，并把现有 `UBR1` 主机解析器增加 CDC serial transport，再用主机工具读取数据。
旧的 `vendor_bulk_host.py` 实机模式针对 WinUSB/libusb，不适用于当前 CDC 镜像。

目标命令形式应为：

```powershell
python -B f730_bridge\tools\cdc_bridge_host.py --port COMx `
  --blocks 100 --overall-timeout-s 120 --output bridge_capture.ubr
```

工具会逐帧检查 USB packet sequence、block sequence、block CRC、record CRC 和 record
边界。一路稳定后才扩到四路，并分别做 30 min、2 h、USB 拔插、单 nRF 断电、FPGA
reset、STM32 reset、坏 CRC、错误/重复 ACK 和主机暂停。物理同步必须另有示波器和长期
漂移证据，不能只凭同一 `sync_epoch` 判定。
