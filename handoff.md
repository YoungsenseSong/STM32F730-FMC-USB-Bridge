# F730 + ZYNQ7020 + nRF54L15 工程验证与续开发交接

更新时间：2026-08-15（Asia/Shanghai）

## 1. 交接目标

本文件用于下一次对话直接恢复工作，范围包括：

- STM32F730 FMC/USB 桥接主工程；
- 组员提供的 F730 改进版 `work1`；
- ZYNQ7020 四路 SPI-FMC 纯 PL 工程；
- nRF54L15 私有 2.4 GHz 链路及其 ZYNQ 传输预集成协议。

当前任务不是把 `work1` 整目录覆盖进主工程，而是先建立可重复的验证基线，再选择性移植其 USB Device/CMake 成果。编译通过、RTL 语法通过和硬件联调通过必须分别记录，不能互相替代。

## 2. 固定路径、仓库和设计依据

| 对象 | 本地路径 | 远端仓库 | 当前基线 |
| --- | --- | --- | --- |
| F730 主工程 | `F:\OliverS\AI_Embedded\F730+FPGA\f730_bridge` | <https://github.com/YoungsenseSong/STM32F730-FMC-USB-Bridge> | 分支 `codex/f730-bsp-v1`，提交 `c3aa0268712428d05965583901293f3979585414` |
| F730 组员版 | `F:\OliverS\AI_Embedded\F730+FPGA\work1` | 尚未单独纳入 Git；当前是 F730 仓库的未跟踪目录 | 组员交付快照，不能当成已审计提交 |
| ZYNQ7020 | `F:\OliverS\AI_Embedded\F730+FPGA\zynq7020\zynq7020_bridge` | <https://github.com/YoungsenseSong/ZYNQ7020-4CH-SPI-FMC-Bridge> | 本地分支 `codex/zynq7020-v1` 跟踪 `origin/main`，提交 `65c8c6004655501ced7ae5e7f027196694631b38` |
| nRF54L15 | 当前未克隆到本工作区 | <https://github.com/YoungsenseSong/nRF54L15-2.4G> | 远端默认分支 `codex/rf-link-2p4g-devlog`，审计时最新提交 `4e360863a3569c9494197ec701604404519d811d` |
| STM32/FPGA MCP | `F:\OliverS\AI_Embedded\stm32-fpga-mcp` | 本地共享工具 | 不指定 `chip` 时必须继续默认 H723；F730/ZYNQ 调用必须显式指定芯片 |

设计依据：

- F730：[`f730_bridge/f730_bridge.docx`](f730_bridge/f730_bridge.docx)，重点是第 5～10 页的软件状态机、USB Vendor Bulk、FMC/NWAIT、Cache 与验收流程，以及第 13～14 页的引脚冻结和首次上电顺序。
- ZYNQ：[`zynq7020/zynq7020.docx`](zynq7020/zynq7020.docx)，重点是第 2～8 页的纯 PL 数据通路、四路 SPI、SYNC、BRAM 所有权、FMC/CSR 和验证矩阵。
- nRF：远端 `README.md`、`applications/rf_link_FUTURE_INTEGRATION.md`、`applications/rf_link_INTEGRATION_REPORT.md` 和 `applications/rf_link_ROADMAP.md`。

资料优先级为：已确认原理图/电气连接 > 三端共同冻结的协议文档 > 两份 DOCX 的设计意图 > 当前代码。不得从 provisional RTL 或示例 USB 描述符反推并擅自冻结硬件协议。

## 3. 当前 Git 和验证状态

### 3.1 F730 仓库

```text
branch: codex/f730-bsp-v1
HEAD:   c3aa0268712428d05965583901293f3979585414
track:  origin/codex/f730-bsp-v1
dirty:  ?? work1/
```

`work1/` 是用户/组员数据，禁止删除、移动或在未确认范围时批量格式化。本文 `handoff.md` 创建后也会是新的未跟踪文件；本轮没有提交或推送。

F730 主工程已有：

- CubeMX `Hal_template.ioc` 和 Keil `MDK-ARM/Hal_template.uvprojx`；
- 216 MHz、FMC Bank1 16 位复用地址/数据总线、I2C1、TIM6、IWDG、USB OTG FS PCD；
- `BSP` 层的 FMC 对齐读写、寄存器映射、块头读取/基本检查、I²C、FPGA IRQ 轮询和 USB 能力接口；
- 仓库文档记录的历史全量构建为 0 error / 0 warning，Code 18,388 B、RO 528 B、RW 12 B、ZI 5,628 B。

但当前代码没有 USB Device 类/描述符，`BSP_USB_Init()` 明确返回 `BSP_STATUS_UNSUPPORTED`。因此历史编译成功只证明骨架可构建，不代表 USB 枚举或完整桥接可用。

### 3.2 `work1` 审计结论

可复用部分：

- CubeMX/CMake/Ninja 工程结构；
- STM32 USB Device Library 和 CDC 类已经加入；
- 主循环实现 CDC 收包后回显；
- MPU 把 `0x60000000` FMC 区域配置为非缓存，开启 I-Cache；
- 存在 `test_CDC.py`，UTF-8 语义与 Python AST 检查通过。

不能直接合并的部分：

- USB 是 CDC，不是设计要求的 Vendor Specific Bulk；
- 使用 ST 示例身份：VID `0x0483`、PID `0x5744`、制造商 `STMicroelectronics`、产品 `STM32 Virtual ComPort`，不能作为本项目最终量产身份；
- CDC 接收回调在 `cdc_rx_flag==1` 时仍立即重挂 OUT 接收，后续包会被静默丢弃；应改为队列/双缓冲或在背压解除后重挂；
- `cdc_rx_buf` 为 1024 B，当前 FS 单包通常不超过 64 B，但复制前仍应做显式长度上界检查；
- 主循环只有 CDC 回环，没有 FMC 读块、块所有权、CRC、ACK、FPGA IRQ/RESET、I²C 控制或 IWDG；
- 已有 `build/Debug/Desktop.elf` 和 MAP 的时间为 2026-08-02，只能证明组员机器曾产生输出；缓存路径指向另一位用户 `C:\Users\lyx06\...`，不能视为本机可重复构建证据；
- `Desktop.ioc`/`Core/Src/fmc.c` 虽配置 PD6 为 `FMC_NWAIT`，但 `hsram1.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE`。F730 主工程当前也相同，而 ZYNQ RTL 明确用 NWAIT 延长读事务。三端联调前必须选择并验证一种一致策略：启用异步 NWAIT，或保证所有 FPGA 读在固定保守等待周期内完成并暂时不用 NWAIT；不能保留“引脚有 NWAIT、控制器不采样、文档却依赖 NWAIT”的矛盾状态。

结论：以 `f730_bridge` 为权威目标，`work1` 只作为 USB/CMake 实验来源。不要整目录覆盖。

### 3.3 ZYNQ7020

当前工程是既有 `zynq7020_bridge.xpr`，器件 `xc7z020clg400-1`、顶层 `board_top`，不要搜索或创建第二份 Vivado 工程。

已实现四路 SPI 主机、记录解析入口、独立 FIFO、调度器、64 B 块头、A/B BRAM、FMC 复用总线从机、CSR 和 IRQ。Vivado `check_syntax` 曾成功；没有综合、实现、时序、引脚 DRC、bitstream 或上板证据。

仍未冻结：

- `constraints/board_top.xdc` 没有真实 PACKAGE_PIN/IOSTANDARD/时钟；
- `rtl/common/protocol_defs.vh` 中 FPGA ID、协议版本、SPI 命令/魔数/CRC、CSR 位和 ACK 仍是 `CONFIG_REQUIRED`；
- 当前 `SPI_RECORD_BYTES=240`，而 nRF V2 输出记录是 248 B；
- `BLOCK_ACK_POLICY=0`，ACK 写入不会释放 BRAM bank；
- RESET/SYNC 极性、时序以及 SPI mode/频率未冻结；
- FMC RTL 会产生 `fmc_nwait_n`，但 STM32 端当前禁用了 WaitSignal。

在原理图、Bank 电压、引脚和三端协议冻结前，只允许运行 `fpga.check_syntax(chip="zynq7020")`；不要综合、实现或生成 bitstream。

### 3.4 nRF54L15

远端 V1 成熟链路口径：IIM-42352 -> FLPR SPIM00 -> 4096 个 XYZ 交错 `int16_t` 样本 -> 共享 SRAM/CRC32/ICMsg -> CPUAPP ESB。空中帧固定 204 B，每批 43 帧，4 kHz/axis、12 ksps、有效载荷约 192 kbit/s。

V2 软件预集成增加：

- 32 位扩展序号、缺失区间、逻辑同步 epoch；
- 64 条静态队列；
- 248 B ZYNQ 记录：40 B 头 + 204 B 原始 RF 帧 + 4 B payload CRC32；
- PEEK/READ/COMMIT/DROP/ARM/START/STOP/RESET 命令语义。

真实 SPIS、DRDY 和硬件 SYNC 捕获仍被禁用，待转接板原理图冻结。注意 README 中 IIM-42352 的 SPI mode 3/8 MHz 是传感器侧 SPIM，不等同于 nRF RX 到 ZYNQ 的 SPIS mode；后者必须单独冻结。

## 4. 推荐验证顺序

### Gate 0：冻结输入和保存证据

1. 重新读取本文件，确认上述路径未变。
2. 分别执行 `git status --short --branch`、`git rev-parse HEAD`、`git remote -v`。
3. 不改动 `work1/build`；对新验证使用独立输出目录，例如 `F:\OliverS\AI_Embedded\artifacts\bridge-validation\20260815`。
4. 在任何协议代码修改前，新建一份三端共享的 `bridge_contract_v0.md`，把第 5 节所有字段一次冻结并附 golden vector。

通过条件：三个基线提交、工具版本、工作区脏文件和输出目录都写入测试记录。

### Gate 1：离线静态审计和可重复构建

#### F730 主工程

使用共享 MCP，所有调用显式指定 F730：

```text
board.inspect_target_identity(chip="STM32F730V8T6")
cubemx.inspect(ioc="configured", chip="STM32F730V8T6")
cubemx.audit(profile="configured", chip="STM32F730V8T6")
stm32.build(project="configured", target="default", clean=true, chip="STM32F730V8T6")
stm32.validate_image(image="latest", chip="STM32F730V8T6")
diagnostics.analyze_keil_map(map="latest", chip="STM32F730V8T6")
```

通过条件：CubeMX 没有未配置/错误外设，Keil clean build 为 0 error / 0 warning，镜像器件/Flash/RAM 边界正确，并保存 operation ID、完整日志、AXF/HEX/MAP 哈希和尺寸。

如果 CubeMX 再提示 FMC 未配置完整，优先核对 `WaitSignal`、`AsynchronousWait`、`DataAddressMux`、MemoryType、Bank/宽度和全部时序字段；不要点击“仍然生成”后把警告当成已解决。

#### `work1`

不要复用已携带其他用户绝对路径的 `work1/build/Debug`。使用本机 STM32CubeCLT/CubeIDE 的 CMake、Ninja、GNU Arm 工具链，在独立目录重新配置：

```powershell
$validationRoot = 'F:\OliverS\AI_Embedded\artifacts\bridge-validation\work1-debug'
$sourceRoot = 'F:\OliverS\AI_Embedded\F730+FPGA\work1'

cmake -S $sourceRoot -B $validationRoot -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$sourceRoot\cmake\gcc-arm-none-eabi.cmake" `
  -DCMAKE_BUILD_TYPE=Debug
cmake --build $validationRoot --clean-first --verbose
arm-none-eabi-size "$validationRoot\Desktop.elf"
python -B -c "import ast,pathlib; ast.parse(pathlib.Path(r'$sourceRoot\test_CDC.py').read_text(encoding='utf-8')); print('AST_OK')"
```

若 `cmake`/`arm-none-eabi-*` 不在 PATH，先定位本机 `$env:LOCALAPPDATA\stm32cube\bundles`，不要继续使用缓存中的 `C:\Users\lyx06\...`。

通过条件：新输出目录从零配置和编译成功、0 warning 或逐条解释 warning、记录 ELF/MAP/HEX 哈希和 Flash/RAM；现有组员产物只用于对比。

#### ZYNQ7020

```text
fpga.inspect_project(project="configured", chip="zynq7020")
fpga.check_syntax(project="configured", chip="zynq7020")
```

通过条件：确认仍是现有 XPR/器件/顶层，所有 RTL 被正确纳入，静态语法通过并保存 operation ID 和日志。当前阶段不运行 `fpga.build`。

#### nRF54L15

先按远端最新已确认提交克隆/checkout，记录 NCS 3.1.0 和 west manifest revision，然后执行：

```powershell
west build -p always --sysbuild -d build_rf_link_tx_mems `
  -b nrf54l15_connectkit/nrf54l15/cpuapp applications\rf_link_tx

west build -p always -d build_rf_link_rx_mems `
  -b nrf54l15_connectkit/nrf54l15/cpuapp applications\rf_link_rx

west build -p always -d build_rf_link_rx_stream `
  -b nrf54l15_connectkit/nrf54l15/cpuapp applications\rf_link_rx `
  -- "-DEXTRA_CONF_FILE=stream.conf"

west build -p always -d build_rf_link_rx_future `
  -b nrf54l15_connectkit/nrf54l15/cpuapp applications\rf_link_rx `
  -- "-DEXTRA_CONF_FILE=future.conf"

python -B -m unittest -v tests.test_rf_link_future
```

通过条件：四个配置和主机契约测试通过，保存各镜像 Flash/RAM、日志和哈希。离线通过仍不能标为 SPIS/SYNC 上板通过。

### Gate 2：模块级仿真和协议 golden vector

三端协议冻结后，先做不依赖硬件的共同测试：

1. 用一个固定 248 B nRF V2 record 作为输入，包含明确端序、node_id、transport_seq、epoch、时间戳、204 B RF 帧、CRC16 和 CRC32。
2. ZYNQ 仿真验证 SPI 命令、DRDY、重复 PEEK、正确/错误 COMMIT、CRC 破坏、FIFO 满和超时。
3. ZYNQ block builder 输出固定 64 B 块头和 payload，保存完整二进制 golden block。
4. F730 主机侧单元测试解析同一块，验证长度、序号、channel mask、CRC、ACK 编码和异常分支。
5. USB host codec 对 DATA/CMD/RSP/EVENT 做分片、粘包、坏 magic、坏长度、坏 CRC、重复 command_id 测试。

通过条件：同一组二进制向量在 nRF C、ZYNQ testbench、F730 C 和 host 工具中得到完全一致的字段和 CRC；不得只用各端各自生成的“自洽”向量。

### Gate 3：单板/双板分级上电

#### F730 USB

1. SWD 最小固件与 PA5 心跳；
2. `work1` CDC 只作为调试回环验证：确认枚举速度、64/256/1024 B 分片、连续突发、拔插和复位；修复丢包/背压后才记录通过；
3. 在主工程实现项目自有 Vendor Bulk 类和合法 VID/PID/字符串；
4. 使用 USBView/设备管理器保存枚举信息，用 libusb/WinUSB 工具做 64 B、1 KiB、1 MiB 回环和至少 30 min 连续流。

通过条件：无静默丢包、无永久 BUSY、拔插和 MCU 复位可恢复；应用帧在 USB 分片/粘包下仍正确。CDC 通过不能代替 Vendor Bulk 通过。

#### ZYNQ + F730 FMC

在引入 USB 数据流之前依次验证：

1. 固定 ID/版本和 `0x55AA/0xAA55`；
2. 走马灯地址/数据；
3. 1 KiB、16 KiB、32 KiB 递增数据和 CRC；
4. A/B block owner 与 ACK/NACK；
5. 连续至少 `10^6` 个 16 位测试字。

ILA/逻辑分析仪同时观察 NADV、NE1、NOE、NWE、AD[15:0] 和 NWAIT。分别在 D-Cache 禁用、FMC 区域非缓存以及最终 Cache 策略下验证，避免把 Cache 旧数据误判为总线错误。

通过条件：地址、数据、CRC 和所有权错误均为 0；STM32 与 FPGA 对 NWAIT 的策略一致；超时/FPGA 复位后能重新读 ID 并恢复到受控状态。

#### nRF V1 和单路 V2

先完成 V1 两板基线：每批 `samples_ok +4096`、`frames_ok +43`，共享内存/CRC/FIFO 错误为 0；抓取至少 220 帧并得到 `bad_batches=0`、`incomplete_batches=0`、`sequence_gaps=0`，短稳 30 min、建议长稳 2 h。

V1 通过后才接一块 RX 到 ZYNQ，启用真实 SPIS/DRDY/SYNC overlay，验证 PEEK 不释放、正确 COMMIT 释放、错误/重复 COMMIT 计数、DRDY 电平、高水位、ZYNQ 重启恢复和至少 2 h 单路流。

### Gate 4：完整链路与故障注入

完整路径：

```text
IIM-42352 -> nRF TX -> 2.4 GHz -> nRF RX
  -> SPIS/DRDY/SYNC -> ZYNQ FIFO/block/BRAM
  -> FMC/NWAIT/IRQ -> STM32F730
  -> USB FS Vendor Bulk -> host
```

每个数据块同时记录 RF seq、transport_seq、block_seq、USB packet_seq、node_id、epoch 和 CRC 结果，使丢失位置可定位。验收至少覆盖：

- 30 min 全链路无内部未计数丢块；
- USB 拔插/主机暂停；
- 单 nRF 断电、RF 丢帧和队列满；
- ZYNQ reset、STM32 reset；
- 错 ACK、重复 ACK、CRC 破坏和数据窗口读取中断；
- 四路高水位公平调度和单路故障隔离。

四路物理同步不能仅凭同一 `sync_epoch` 宣称完成；当前 V2 只能证明逻辑起点对齐。物理同步必须有 TX/MEMS 侧共同采样时基和示波器/长期漂移证据。

## 5. 实现前必须冻结的三端协议

| 项目 | 当前冲突/缺失 | 冻结输出 |
| --- | --- | --- |
| nRF-ZYNQ record | nRF V2 为 248 B，ZYNQ provisional 为 240 B | 固定字节布局、端序、版本、最大长度和至少 3 个 golden vectors |
| SPI transport | 命令、mode、频率、CS 时序未定；不要把 MEMS mode 3 混入 | 命令码、request/response 长度、mode、时钟上限、超时和异常恢复 |
| DRDY/SYNC/RESET | 极性、脉宽、扇出和复位范围未定 | 电平/脉冲语义、上电默认、CDC 方法和时序图 |
| CRC | CRC16/CRC32 参数未冻结 | polynomial、init、refin/refout、xorout、覆盖范围、CRC 字节序、check/residue |
| 身份与版本 | FPGA ID、PROTO_VERSION、BUILD 编码 provisional | 常量分配和不兼容版本拒绝规则 |
| CSR/IRQ | 控制/状态/W1C 位未冻结 | 完整位表、保留位读写行为、错误粘滞规则 |
| BLOCK_ACK | ZYNQ 默认 policy 0 不释放；F730 只有地址骨架 | ACK/NACK 编码、sequence 宽度、重复/错误 ACK 行为和超时 |
| FMC/NWAIT | FPGA 产生 NWAIT，STM32 两版都禁用 WaitSignal | 确认是否启用、极性、采样阶段、最大拉低时间和 STM32 超时策略 |
| I²C | 地址、ID 和固定寄存器协议未定 | 7-bit 地址、总线速率、寄存器宽度、端序、固定 ID、超时/重试 |
| USB | 主工程无类；work1 是 ST CDC 示例 | 合法 VID/PID、制造商/产品/序列号、Vendor Bulk 接口/端点、WinUSB 绑定和应用帧 |

这份协议应由 nRF/ZYNQ/F730 三仓共同引用。冻结后才能清除 ZYNQ `CONFIG_REQUIRED`、实现 F730 ACK/CRC/USB Vendor Bulk，并启用 nRF 真实 SPIS。

## 6. 下一次对话的首要动作

1. 先读取本文件并重新验证 Git 状态和远端提交，任何漂移都先报告。
2. 不修改主工程，先在隔离输出目录复现 `work1` 的 clean build；保存命令、工具版本、日志、尺寸和哈希。
3. 静态修复并参数化 `test_CDC.py`：端口/包长/轮数/超时作为 CLI 参数，增加多包突发、随机数据、短读循环、吞吐和错误退出码。
4. 只在独立分支（建议 `codex/work1-validation`）整理 `work1`；不要提交其旧 `build/` 产物。
5. 生成 `f730_bridge` 与 `work1` 的结构化差异清单，明确“移植”“保留主工程”“丢弃”三类。第一批只移植 USB Device 底层和可复现 GCC 构建，不移植 CDC 作为最终类。
6. 起草并评审 `bridge_contract_v0.md`，优先解决 248/240 B、SPI mode/命令、CRC、ACK 和 NWAIT。
7. 协议冻结后，为 ZYNQ 增加 testbench/golden vector；只运行 `fpga.check_syntax(chip="zynq7020")`。
8. 然后在 F730 主工程实现 Vendor Bulk、缓冲所有权和 FMC-to-USB 状态机，保持现有 BSP API 和 IWDG 运行保障。

## 7. 禁止事项

- 不要覆盖或删除 `work1`；
- 不要把 `work1/build` 的旧 ELF/MAP 当成本机 clean build；
- 不要把 CDC 示例 VID/PID 和 ST 字符串作为产品身份提交；
- 不要在没有三端协议和原理图证据时填写 `CONFIG_REQUIRED`；
- 不要创建重复 Vivado 工程；
- 不要在 ZYNQ 当前阶段运行综合、实现或 bitstream；
- 不要省略 MCP 的 F730/ZYNQ `chip` 参数；未指定参数继续是 H723；
- 不要把 build/testbench/syntax 成功描述成板级成功；
- 不要在 `F730+FPGA` 根仓库误提交嵌套 ZYNQ 仓库生成物或 `work1/build`。

## 8. 本轮实际完成与未完成

已完成：

- 核对 F730、ZYNQ 本地 Git 与远端仓库身份；
- 核对 nRF 远端当前 README、V2 预集成与路线图；
- 只读审计 `work1` 的 CubeMX/CMake、USB CDC、FMC、主循环和测试脚本；
- 检查两份设计文档的结构与关键设计要求；ZYNQ DOCX 的 9 页 QA 渲染已逐页检查；
- 形成上述验证 Gate、通过条件、协议阻塞项和下一步顺序。

未完成：

- 没有执行新的 F730/`work1`/nRF clean build；
- 没有重新运行 ZYNQ Vivado `check_syntax`；
- 没有下载、烧录或连接任何硬件；
- 没有修改固件/RTL；
- 没有提交或推送 Git；
- F730 DOCX 可被 Word 只读打开并识别为 14 页，但本机 Word 的 PDF 导出两次超时，标准渲染器又缺少 `pdf2image`，因此本轮使用 Word 页码化文本审计，没有产出新的逐页 PNG。源 DOCX 未被保存或改写。

下一轮报告必须从“未完成”项继续，并按 Gate 提供原始日志/哈希/串口或 ILA 证据。

## 9. 2026-08-16：从第 6 节继续后的实际结果

本节覆盖并取代第 8 节的“未完成”状态。当前已经完成所有不依赖开发板、电气约束或
授权 USB 身份的安全离线工作，停在 F730 Gate A 上电 smoke test。没有提交或推送 Git。

### 9.1 基线与边界

- F730 根仓：基线 `c3aa0268712428d05965583901293f3979585414`，工作分支 `codex/work1-validation`；
- ZYNQ 嵌套仓：基线 `65c8c6004655501ced7ae5e7f027196694631b38`，现有 XPR、part `xc7z020clg400-1`、顶层 `board_top`；
- nRF：只读参考远端默认分支 `codex/rf-link-2p4g-devlog` 的 `4e360863a3569c9494197ec701604404519d811d`，本轮没有修改 nRF 文件；
- 未综合、未实现、未生成 bitstream，未下载或连接任何板卡；
- 输出证据放在 `F:\OliverS\AI_Embedded\artifacts\bridge-validation\20260815` 和 `20260816`，没有复用 `work1/build` 的旧缓存。

### 9.2 第 6 节任务完成情况

1. `work1` 在隔离目录 clean configure/build 成功；增强后的 `test_CDC.py` 支持 CLI 参数、随机数据、短写/短读循环、多包突发、吞吐统计、明确退出码和无硬件 self-test。
2. 完成 `f730_bridge` / `work1` 结构化迁移矩阵：主工程保留 BSP、MPU/FMC、IWDG 和 Keil；只吸收 USB Device Core/LL 与 GCC/CMake；CDC 不作为产品类。
3. 根仓新增 `bridge_contract_v0.md`、确定性 golden generator 和共享二进制 vectors。冻结 248 B record、64 B block、CRC16/CRC32、低 16 位 ACK、24 B `UBR1` USB 帧和 NWAIT 控制器语义。
4. ZYNQ parser 改为固定 248 B 且严格检查 header/payload CRC；block target/max 为 66/132 条完整 record；A/B bank 只接受已 claim 且序号匹配的 ACK。
5. F730 实现 Vendor Bulk、OUT 背压、IN 发送所有权、USB 字节流解析、FMC 稳定快照、分段读、block/record/CRC 校验、USB 完成后 ACK、重复块和故障保持所有权。
6. F730 `.ioc` 与源码都启用低有效异步 NWAIT；最终时序只留给逻辑分析仪验证。
7. 新增 `f730_bridge/tools/vendor_bulk_host.py`，可离线消费共享 golden，也可在 WinUSB/libusb 下检查枚举、未知命令 RSP、USB/block/record CRC 和 sequence。

### 9.3 离线验证证据

| 项目 | 结果 | 证据 |
| --- | --- | --- |
| `work1` clean GCC Debug | 成功；Flash 37,888 B（57.81%），RAM 17,392 B（6.63%） | `20260815/work1-debug-zephyr-final/{configure.log,build.log,size.log}` |
| `test_CDC.py` | `SELF_TEST_OK`，AST OK | 2026-08-16 本机复核 |
| 共享协议 Python | 9/9 | `python -B -m unittest -v protocol.test_contract` |
| Vendor host codec | `VENDOR_HOST_SELF_TEST_OK frames=2 block_records=2` | `python -B f730_bridge/tools/vendor_bulk_host.py --self-test` |
| F730 C codec | ArmClang 6.23 `-Wall -Wextra -Werror` 两个 object 编译成功 | `20260816/f730-protocol-c-final/compile.log`；当前无本机 native C runner，因此这是编译证据，不冒充运行证据 |
| ZYNQ record parser TB | 合法 record 通过，坏 header CRC、坏 payload CRC、坏长度拒绝 | `20260816/tb_spi_record_parser_final_run.log` |
| ZYNQ block builder TB | 560 B shared golden 逐字节一致；两个 mid-record flush 边界通过 | `20260816/tb_block_builder_final_run.log` |
| ZYNQ A/B ACK TB | 未 claim、错误序号、正确序号、重复 ACK 行为通过 | `20260816/tb_bram_pingpong_final_run.log` |
| Vivado 2025.2 | 现有 XPR `check_syntax` 返回 0 | `20260816/zynq-check-syntax-final.log`；只有全局 Board Store/空 BoardPart 环境警告 |
| F730 Keil rebuild | Code 31,356、RO 544、RW 240、ZI 41,760；0 Error / 0 Warning | 2026-08-17 PA5 迁移后本机 full rebuild |
| F730 GCC Release clean | Flash 23,580 B（35.98%），RAM 46,128 B（17.60%）；无 compiler warning | 2026-08-17 PA5 迁移后本机 clean build |

关键镜像 SHA-256：

| 镜像 | SHA-256 |
| --- | --- |
| `work1` `Desktop.elf` | `9b49e2f34df853fd4c018207bcdece71d4508c4d181b0e8118d1f6f3e69938bd` |
| Keil `Hal_template.axf` | `d29ae7e669c1a28842a43b7dc7749d8fa669ad82a19d16e23f6ec2a0206ca51f` |
| Keil `Hal_template.hex` | `6023d80386f73a47b08b95985351bab6c2f6d5493e2f514a98030e93f2a2f974` |
| GCC Release `F730Bridge.elf` | `aa306c928716193ca28cb009f6e9c65f489ab01eae442e68ca72884e91d4ddfa` |
| GCC Release `F730Bridge.hex` | `ecc78b015cc8e39aa8fa0c2dcf1cabd9e09a5073999f6515efea660450780746` |

### 9.4 当前刻意保留的门控

- `BRIDGE_USB_IDENTITY_CONFIGURED=0`、VID/PID 为 0、字符串为 `CONFIG_REQUIRED`。当前 HEX 有完整 Vendor Bulk 代码，但有意不拉起 USB；没有复用 ST CDC `0483:5744`。
- `FPGA_ID`、BUILD、XDC/Bank 电压/PL clock、PC7 RESET、FMC 最终时序仍未知。
- nRF 远端存在软件协议模型，但真实 SPIS backend、CPOL/CPHA、SCLK/CS 周转和 DRDY/SYNC/RESET 极性仍未冻结；ZYNQ raw-record SPI 只用于仿真。
- 因上述物理项缺失，仍禁止综合、实现和 bitstream；离线通过不能称为全链路完成。

### 9.5 现在需要开发板的动作

先只执行 `f730_bridge/docs/HARDWARE_BRINGUP.md` Gate A：烧录 Keil HEX
`6023d803...2f974`，FPGA/nRF 先不作为变量引入，观察 PA5 每 500 ms 翻转并持续至少
2 min，确认 SWD 可重连且 USB 不枚举。回传板卡版本、供电/电流、LED 与 SWD 结果。

2026-08-17 首次下载尝试没有烧入 MCU：Keil 处于 Simulator 模式，CMSIS-DAP 条目又只
装载了 `STM32F72x_73x_OPT.FLM`，因此程序区 `0x08000000-0x08007D9F` 无匹配算法。
本机已确认连接 DAPLink/CMSIS-DAP，F7 DFP 3.1.1 及主 Flash 算法均存在；工程已改为
硬件调试模式并选择 `STM32F7x_64_AXI.FLM`（`0x08000000`、`0x00010000`）。下一步仍是
关闭并重开 Keil 后重试 Gate A 下载；成功日志不得再出现 Simulator 或 `No Algorithm`
提示，并应完成 Program/Verify。

2026-08-17 用户确认实际开发板的板载指示 LED 位于 PA5；随附核心板原理图却把绿色
D2 画在 PC13，并通过 1.5 kOhm 接到 3.3 V。该 PDF 与现场板型/版本不一致，后续以实际
开发板信息为准。主工程源码和 `.ioc` 已迁移到 PA5；`work1` 仍作为只读交付快照保留，
不随主工程修改。PA5 有效电平待正确原理图确认；Gate A 的翻转观察不依赖该极性。
后续用户确认新板只有开发板排针引出不同，MCU 和板载外设相同；开发继续按现有资料，
每次外接前只需按实物复核排针位置。用户于 2026-08-17 报告 Gate A 全部通过。

Gate B 之前还需提供授权 VID/PID、Manufacturer、Product；Gate C 之前需提供审核后的
XDC/电气和测试 bitstream 输入。Gate A 已由用户报告通过，当前先验收新增 USART1；不能
提前宣称 USB/FMC/全链路通过。

### 9.6 2026-08-17：四路 nRF record 逻辑对齐补充

只读核对 nRF 远端默认分支提交
`4e360863a3569c9494197ec701604404519d811d`。RX V2 已实现 16->32 位序号扩展、缺失
区间、`sync_epoch`、`rx_tick`、`logical_sample_index`、SYNC 状态机和 248 B FPGA record；
SYNC 后首个有效 `BATCH_START` 被映射为逻辑 index 0。远端同时明确：真实 SPIS、硬件
SYNC 捕获、TX/MEMS 公共采样时基和高分辨率 TX 采样时间戳尚未完成，所以当前只能证明
逻辑整数样本对齐，不能证明物理同时采样或分数相位已补偿。本轮没有修改 nRF 仓库。

ZYNQ 现有四路路径已在 CRC parser 与 block builder 之间加入
`four_channel_record_aligner`：四路各缓存一条完整 record，只对 `SYNCED=1`、epoch 等于
FPGA 当前 epoch、`logical_sample_index` 完全相同的四条记录按 ch0..ch3 成组输出；
未同步/DEGRADED、旧 epoch 和旧 index 记录按通道淘汰计数，204 B RF payload 保持原样。
新增 `0x0050..0x0076` 只读 ALIGN CSR，提供缓存/输出状态、组数、元数据错误、最近
epoch/index 和每路 drop count；任一 drop/error 会置全局 degraded。

离线证据：

- 四路 aligner TB：`groups=4 drops=2/1/0/0 errors=3`，通过；
- 原 record parser、block builder、A/B ACK 三项 Icarus 回归全部通过；
- 顶层 Icarus elaboration/compile 通过；共享协议 9/9 与 Vendor host self-test 通过；
- Vivado 2025.2 对现有 XPR/`xc7z020clg400-1`/`board_top` 执行 `check_syntax` 返回 0；
  未运行 synthesis、implementation 或 bitstream。

证据日志：

- `F:\OliverS\AI_Embedded\artifacts\bridge-validation\20260817\zynq-four-channel-align-iverilog-regression.log`
  SHA-256 `e25b245f0b8fb1554e1ce58523df693184f925fc1fd18bbf6725fd2850fc78b6`；
- `F:\OliverS\AI_Embedded\artifacts\bridge-validation\20260817\zynq-four-channel-align-check-syntax-final.log`
  SHA-256 `1018ee95b2ef65883938fef3e9b7f346a3fb0edb099270758f321c6f2a917709`。

Gate A 已由用户报告通过。四路 ZYNQ+nRF 实板对齐测试还需等待
nRF 转接板/真实 SPIS 与 SYNC 电气约定、审核后的 XDC 和测试 bitstream；之后用同一 epoch
注入四路已知相关信号，读 ALIGN CSR 与原始 payload 做互相关/长期漂移测量。只有获得
公共 TX/MEMS 采样时基、采样率和每路实测延迟后，才可设计整数采样延迟或分数延迟 FIR。

### 9.7 2026-08-17：Gate A 通过及 USART1 调试口

- 用户报告 Gate A 全部通过；该结论只覆盖 SWD、PA5 心跳、IWDG 持续运行和 USB 身份门
  关闭，不外推为 USB/FMC/FPGA/nRF 链路通过。
- 新板 MCU 与板载外设按现有资料开发；只有排针引出不同，外接前按实物核对。
- 新增 USART1：PA9 TX、PA10 RX、AF7、115200 8-N-1、无流控；`BSP_Debug_Write*` 与
  `printf` 已重定向。启动输出两条 banner，之后每秒输出 heartbeat；主循环非阻塞处理
  `ping` 并返回 `pong`。
- Keil AC6 rebuild：Code 34,880、RO 584、RW 240、ZI 41,936，0 Error / 0 Warning；日志
  `20260817/f730-usart-ping-keil-rebuild.log`，SHA-256
  `cab9f05052e7dcf5937ff49dd69397e140d6bc3c8dddc9a8bc4c93fa22472287`。
- GNU Arm GCC 12.2 Release clean：Flash 25,912 B（39.54%），RAM 46,320 B（17.67%）。
- 协议测试 9/9 与 Vendor host self-test 通过。
- 新 Keil HEX SHA-256：
  `66a791f7fe016e2a5175a70081bebc1319e9318ff0e7c5223973c9d46d175d04`。

下一步执行 `f730_bridge/docs/HARDWARE_BRINGUP.md` Gate A.1：以 3.3 V TTL USB-UART 交叉
连接 PA9/PA10 并共地，验证 banner、`ping`/`pong` 和至少 2 min 的每秒 heartbeat。USB 身份仍保持全零且
门关闭；取得合法 VID/PID/字符串前不得启动 Gate B。

### 9.8 2026-08-18：主工程切换为 CDC FS 回环

用户决定当前学生科研联调阶段沿用 `work1` 的 USB CDC Full Speed 配置，先完成
F730↔PC 通讯；板载活动 LED 使用已确认的 PA5。该决定取代 9.4/9.5/9.7 中“当前镜像
保持 USB 身份门关闭、等待授权 VID/PID 后才枚举”的下一步，但不把 ST 示例身份解释为
正式授权。

实际修改：

- 主工程从 Vendor Bulk 类切换为 CDC ACM FS，data OUT `0x01`、data IN `0x81`、command
  IN `0x82`、MPS 64；Keil/CMake/`.ioc` 已同步，最终 MAP 只链接 `USBD_CDC`。
- 开发身份沿用 `work1`：VID `0x0483`、PID `0x5744`、Manufacturer
  `STMicroelectronics`、Product `STM32 Virtual ComPort`；显式标记
  `BRIDGE_USB_IDENTITY_TEST_ONLY=1`，仅限受控实验室验证。
- 默认 `BSP_USB_CDC_ECHO_ENABLED=1`。RX 使用 1 KiB 环形缓冲；只有 IN 发送完成才释放
  数据，剩余空间不足 64 B 时暂停重挂 OUT，避免 `work1` 单 flag 的静默丢包。
- PA5 原 500 ms 心跳已改为 USB OUT 包活动队列：每包约 60 ms 活动、60 ms 间隔；
  USART1 每秒 heartbeat 保留。LED 有效极性仍以待补充的正确板卡资料为准。
- `UBR1` codec、FMC bridge 和命令解析仍保留；回环模式阻止 bridge 读取/ACK FPGA。
  Gate B 通过后把 echo 宏改为 0，即可继续开发 CDC 二进制桥接 transport。
- 新增 `f730_bridge/tools/cdc_echo_test.py`，无硬件 self-test 已通过。

离线构建证据：

- Keil AC6：Code 34,972、RO 568、RW 264、ZI 41,608，0 Error / 0 Warning；证据
  `F:\OliverS\AI_Embedded\artifacts\bridge-validation\20260818\f730-cdc-fs-keil-rebuild.htm`。
- Keil AXF SHA-256：`a4d930add70e0951d2d4469d334b24475aab88756687229868f018f22e96c01a`；
  HEX SHA-256：`bb66723bed617d68c8868952c6b2c266c6198aa4c940a753d0a02fff6270c4c5`。
- GNU Arm Release clean：Flash 26,728 B（40.78%），RAM 45,624 B（17.40%）；ELF
  SHA-256 `7b71cdea4bbd539d24809759dfcea084050d3a0d21849162ed58f8034ef8c29b`，HEX
  SHA-256 `5e083ff0bc0e957503974a41b5a916ae1c18e5b84114692955ec8175ebf65466`。

下一步只执行 `f730_bridge/docs/HARDWARE_BRINGUP.md` 的新 Gate B：烧录上述 Keil HEX，
FPGA/nRF 保持断开，确认 Windows 枚举为 `USB\VID_0483&PID_5744` 虚拟 COM，运行 64 B 与
1024 B 回环、PA5 包活动、20 次拔插/复位和至少 30 min 持续测试。未取得这些开发板证据
前，不得称 USB 板级通过，也不进入 FMC-to-USB 数据 Gate。

### 9.9 2026-08-18：RX0 -> ATK-DF7020 CH0 SPI 前置审计

本节只做板级资料、现有工程和 CH0 接口输入审计。未修改 RTL/XDC/XPR，未运行
synthesis、implementation 或 bitstream，也没有把静态代码结论写成硬件通过。

#### 9.9.1 仓库与板卡身份

- 根仓：`codex/work1-validation`，HEAD
  `c3aa0268712428d05965583901293f3979585414`，保留现有 F730/USB/协议未提交修改；
- ZYNQ 子仓：`codex/zynq7020-v1`，HEAD
  `65c8c6004655501ced7ae5e7f027196694631b38`，保留现有 RTL、文档、仿真和硬件资料修改；
- 继续使用唯一工程
  `zynq7020/zynq7020_bridge/zynq7020_bridge.xpr`，顶层 `board_top`，没有创建重复工程；
- XPR 当前 `Part=xc7z020clg400-1`、`BoardPart` 为空、`constrs_1` 为空；
- 用户照片 `zynq7020/硬件资料/48d122b395196cc1445db601e0bfd71f_720.png` 可见底板
  `ATK-DF7010/7020P`、核心板 `CF7010B/7020B`、贴纸 `ZYNQ 7020板` 和芯片顶标
  `XC7Z020`。扩展口旁为 `3.3V` 丝印，符合底板改版说明中 V3.9 将旧 `3V` 丝印改为
  `3.3V` 的特征；核心板外观与 V2.5 资料一致，但照片没有独立可读的 V2.5 版本丝印；
- 官方规格书 V1.2 第 6、9、10、14 页把 7020 核心板标为 `XC7Z020CLG400-2`，而当前
  XPR 是 `-1`。照片顶标未直接显示速度级，不能单靠照片消除冲突。任何 synthesis/bitstream
  前必须由采购标签、出厂信息或 Vivado/JTAG 器件信息确认速度级并修正 XPR；本轮未改。

实际读取资料：

| 资料 | 版本/关键页 | SHA-256 |
| --- | --- | --- |
| `领航者ZYNQ底板原理图_V3.9.pdf` | V3.9；第 5 页为 J3/J4 与核心板连接 | `ec8b68eeb07e0743e6c60528444f6aeb457dc60e897ddf9eb86d5a56c1ab6a69` |
| `领航者ZYNQ底板结构&装配图正面_V3.9.pdf` | V3.9；第 1 页确认 J3/J4 正面位置、J4 丝印球位与方形 Pin 1 焊盘 | `59a3240fc364ed270aaf1602793525c7fe748affb453f161faf3f9a0fbd2bc77` |
| `领航者ZYNQ底板结构&装配图背面_V3.9.pdf` | V3.9；第 1 页用于核对背面镜像视角，接线编号仍以正面图为准 | `5b3f9ee2d639a00867861c859e1953082ac7395df6daf2bbd025bc75a53d6616` |
| `ZYNQ7010_7020核心板原理图_2V5.pdf` | V2.5；第 3 页 Bank34/35 与 50 MHz，第 6 页 Bank13，第 7 页 VCCO | `1c8cf8105e91d762d29ad604e82b629b3921fa474d5a0311ed6c04237db107ea` |
| `【正点原子】领航者ZYNQ开发板规格书V1.2.pdf` | V1.2；第 12~13 页电源，第 17 页 PL 时钟，第 36 页扩展口 | `b974ed61b10fabafe408a42d119e94fc53c6133a9a7fc64b0191bfe5e14744e6` |
| `领航者ZYNQ IO引脚分配总表.xlsx` | PL/PS IO 总表；不含 J4 逐针表 | `12ea3d18b8cd7259d5299ecf7a95856b40c0aa8afb0d8100a8dfbec8229d3ea6` |
| `NAVIGATOR_ZYNQ_IO.xdc` | 官方板载外设示例；U18/50 MHz/LVCMOS33；不含扩展 J4 | `80438a533021b8bff2f904e454384e179db1e20aa0384093659d703949b20452` |
| FPGA 实物照片 | `ATK-DF7010/7020P` + `CF7010B/7020B` + 7020 贴纸 | `c1d9729f9d8dc68b791d0d7cdfbf6c20e57824ffb87f01653d5abc8bcce4a3a6` |

#### 9.9.2 CH0 推荐物理映射

V3.9 正面装配图的标准方向（“正点原子领航者”文字正向、按键在下）中，`J3` 是左边缘竖直
2x20，`J4` 是底边横向 2x20。用户照片相对该标准方向顺时针旋转 90 度，因此照片原方向的
左边缘竖直 2x20 才是同一个 `J4`，照片上边缘横向 2x20 是 `J3`。不得直接把装配图的左/下
描述套到未经旋转的照片。

J4 正面方形焊盘是 Pin 1：在标准装配图方向位于 J4 左下，在用户照片原方向位于 J4 上端的
板边/外侧列。由此，从用户照片上端靠近左上安装孔处向下计数，外侧/板边列为奇数
1,3,...,39，内侧/板中心列为偶数 2,4,...,40。板上没有印 `J4-4`、`J4-12` 这种编号，
而是逐针印 ZYNQ package ball（例如 T5、T9），实际接线应同时按“J4 针号 + 球位丝印”识别。
J4 整排还混有 Bank34 与电源脚，不能笼统称为全 Bank13；本表所选 J4-4 至 J4-16 均经原理图
确认属于 Bank13。以下七线全部选内侧偶数列；J4-37/39 均为 GND。J4-38 是 +3.3 V、
J4-40 是 +5 V，CH0 首次接线不得连接这两个电源脚。

| 信号 | J4 物理针 | 底板/核心板网络 | ZYNQ package pin | Bank/VCCO | 建议 IOSTANDARD | ZYNQ 方向 | 板载复用/器件 | 板载上下拉/串阻/电平转换 | 当前工程占用 | 证据 |
| --- | ---: | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SCK | 12（内侧第 6 针，丝印 `T9`） | `B13_L12_P` | T9 | Bank13 / 3.3 V | LVCMOS33 | output | 无，J4 直出 | 无 | RTL `spi_sclk[0]` 有端口；XDC 未约束 | 底板 V3.9 p5；正面装配图 p1；核心板 V2.5 p6/p7 |
| MOSI | 4（内侧第 2 针，丝印 `T5`） | `B13_L19_P` | T5 | Bank13 / 3.3 V | LVCMOS33 | output | 无，J4 直出 | 无 | RTL `spi_mosi[0]` 有端口；XDC 未约束 | 同上 |
| MISO | 6（内侧第 3 针，丝印 `U7`） | `B13_L11_P` | U7 | Bank13 / 3.3 V | LVCMOS33 | input | 无，J4 直入 | 无 | RTL `spi_miso[0]` 有端口；XDC 未约束 | 同上 |
| CS_N | 8（内侧第 4 针，丝印 `V8`） | `B13_L15_P` | V8 | Bank13 / 3.3 V | LVCMOS33 | output | 无，J4 直出 | 无 | RTL `spi_cs_n[0]` 有端口；XDC 未约束 | 同上 |
| DRDY | 10（内侧第 5 针，丝印 `U8`） | `B13_L17_N` | U8 | Bank13 / 3.3 V | LVCMOS33 | input | 无，J4 直入 | 无 | RTL `spi_drdy[0]` 有双触发器同步；XDC 未约束 | 同上 |
| SYNC_IN（FPGA 端 `sync_out[0]`） | 14（内侧第 7 针，丝印 `V6`） | `B13_L22_P` | V6 | Bank13 / 3.3 V | LVCMOS33 | output | 无，J4 直出 | 无 | RTL 有发生器；XDC 未约束 | 同上 |
| RESET_N（预留） | 16（内侧第 8 针，丝印 `Y6`） | `B13_L13_N` | Y6 | Bank13 / 3.3 V | LVCMOS33 | output/reserved | 无，J4 直出 | 无 | RTL 当前为推挽 `nrf_reset_n[0]`；XDC 未约束 | 同上 |
| 公共 GND | 37、39（外侧末两针） | GND | - | - | - | - | 公共地 | 直连地平面 | - | 底板 V3.9 p5 |

Bank13 电气证据不是从其他板卡类推：核心板 V2.5 原理图第 7 页明确 `VCCO_B13` 通过
0 Ohm 选件接 `+3.3V`，VCCIO 选件 NC；规格书 V1.2 第 13 页也明确 Bank13/34/35 均为
3.3 V。因此 ZYNQ 侧只能按 3.3 V LVCMOS 设计。RX0 的 `VDD_GPIO` 仍必须实测：若为
3.3 V 可评审直连；若为 1.8 V，七线不能按上述方式直接连接，必须加入定向电平转换，且
1.8 V MISO/DRDY 不能被假定满足 LVCMOS33 VIH。

底板与核心板在上述七线之间没有串联电阻、上下拉或电平转换。FPGA 未配置期间 IO 为
高阻，故适配板至少应在实测 nRF 电压域评审 `CS_N` 上拉、SCK/SYNC 安全下拉和
RESET_N 上拉；具体阻值与 RESET 驱动方式仍未冻结。飞线的源端串阻只能在逻辑分析仪/
示波器看到振铃后定值，不能把经验阻值写成已确认硬件参数。

#### 9.9.3 时钟、SPI 与 CH0 可复用性审计

- 官方核心板原理图 V2.5 第 3 页：PL 晶振 X1 为 50 MHz，经 R7=33 Ohm 到
  `PL_GCLK`/U18；官方 XDC 也约束 U18、LVCMOS33、20 ns；
- 当前 `board_top.PL_CLOCK_HZ=100000000`，但内部实际直接使用 `pl_clk`，参数没有生成
  新时钟。它与实物 50 MHz 不一致；`SPI_SCLK_DIVIDER=6` 在 50 MHz 下会产生约
  8.333 MHz，超过 nRF 给出的 8 MHz 上限。初始联调建议 1 MHz（50 MHz/50），但需在
  后续实现时改为明确参数，不能沿用 divider 6；
- `spi_master_ch` 是 CPOL=0/CPHA=0、MSB-first 的 raw master：下降沿更新 MOSI、上升沿
  采 MISO，能在同一时钟同时收发，但 CPOL/CPHA 不可参数化；
- 当前一次 CS 覆盖 1-byte `0x00` 命令和 248-byte raw record；不支持 8-byte request、
  260-byte response、PEEK/COMMIT 或两个 CS 事务。CS setup 约为半个 SCLK 周期，hold 和
  inactive 只由固定状态机/下游 backpressure 决定，均不可独立参数化；
- 基于“nRF 必须先收到并解析 8-byte request 才能准备对应 260-byte response”的当前
  command-engine 事实，CH0 首版建议采用两个 CS 事务：事务 A 发送 8 B request，nRF
  准备完成后事务 B 时钟出 260 B response；该建议仍需 nRF SPIS EasyDMA backend 确认。
  若采用两事务，暂不要求 same-CS turnaround/dummy；response 事务 MOSI 可填 0；
- DRDY 已有 2FF 电平同步，当前 RTL只按高电平启动，没有边沿检测。若冻结为高有效，应
  定义为“可 PEEK 数据/response ready 时保持高，正确 COMMIT 释放最后一条后才低”；FPGA
  必须增加事务状态，不能在 DRDY 持续高时重复自动 PEEK；
- `sync_pulse_gen` 能按 channel mask 输出高脉冲，当前固定 10 个 PL 周期，即 50 MHz 下
  200 ns，20 ns 粒度；高有效/上升沿与 200 ns 都只是当前实现。建议 nRF 首版选上升沿
  GPIOTE capture，联调脉宽先用 1 us 方便测量，但 nRF 可接受最小脉宽仍为 UNKNOWN；
- `nrf_reset_n` 当前是 FPGA 内部 `reset_n` 的四路推挽复制：FPGA reset 时低、释放后高。
  nRF RESET 引脚的容限、内部上下拉、允许推挽/开漏和最小时序均 UNKNOWN，因此 CH0
  第一次联调只预留 J4-16，不连接 RESET_N；
- parser 会先缓存完整 248 B，检查 NRF1 magic、version/type/length、CRC16 与 CRC32 后才
  输出；每通道 FIFO 深 16,384 byte，高水位 14,336 byte；随后有 248 B 四路 record
  aligner、64 B block header、A/B BRAM、FMC/CSR/IRQ。数据面、CRC 和下游缓存可复用；
- FPGA 没有 nRF 的 64-record queue，也没有 PEEK/正确 COMMIT/错误或重复 COMMIT 状态机；
  这些必须在未来 CH0 transport controller 中实现，不能由现有 BRAM `BLOCK_ACK` 代替；
- 当前四路 generate 全部实例化，`spi_start` 无 active-channel mask，aligner 又要求四路
  `all_valid/all_synced`。因此只接 CH0 虽可能完成 raw SPI/parser，却不会形成下游 block。
  后续应保留四路结构，增加 `ACTIVE_CHANNEL_MASK=4'b0001` 的 CH0 模式并使 aligner/block
  接受单通道；本轮没有实现；
- FMC/STM32 路径与 CH0 的 SPI 电气验证解耦：第一次 CH0 只验证 request/response、CRC、
  PEEK/COMMIT、DRDY 和 SYNC capture，不接入 FMC/USB，也不把 F730 Gate A/CDC 成功当作
  nRF-SPI 成功。

#### 9.9.4 交给 nRF 侧冻结的输入与 UNKNOWN

| 项目 | FPGA 审计输出 |
| --- | --- |
| CH0 排针 | 用户照片左边竖排 J4 的内侧偶数列：J4-4/丝印 T5 MOSI、J4-6/U7 MISO、J4-8/V8 CS_N、J4-10/U8 DRDY、J4-12/T9 SCK、J4-14/V6 SYNC_IN、J4-16/Y6 RESET_N 预留；外侧末端 J4-37/39 GND |
| ZYNQ pins | T5/U7/V8/U8/T9/V6/Y6，全部 Bank13 |
| ZYNQ VCCO/IOSTANDARD | 原理图确认 3.3 V；LVCMOS33 |
| nRF VDD_GPIO | UNKNOWN，接线前必须实测；1.8 V 时必须电平转换 |
| SPI mode | 当前 raw RTL 固定 mode 0；建议双方优先冻结 mode 0，但不是现有板级约束 |
| 初始 SCLK | 建议 1 MHz；FPGA 50 MHz 时 divider 50；当前 divider 6 不可用 |
| CS setup/hold/inactive | nRF 要求 UNKNOWN；FPGA 能以 20 ns 粒度实现，当前不可独立配置，需后续参数化 |
| 事务形状 | 建议 8 B request 和 260 B response 分两个 CS；由 nRF SPIS backend 最终确认 |
| turnaround/dummy | 两事务方案建议 0；若 nRF backend 不能在下一次 CS 前准备 TX，则重新冻结 |
| DRDY | 建议高有效电平，直到正确 COMMIT 后按队列状态撤销；当前 RTL只支持高电平触发，缺事务门控 |
| SYNC_IN | 建议上升沿 GPIOTE capture；初测 1 us 高脉冲；nRF 最小脉宽 UNKNOWN |
| RESET_N | 第一次 CH0 不连接；推挽/开漏、极性、电压和时序 UNKNOWN |
| 上电安全状态 | 当前已配置 RTL：CS_N=1、SCK/MOSI/SYNC=0；RESET_N 随 FPGA reset；未配置 FPGA 为高阻，外部安全偏置待 nRF 电压冻结 |
| 外部器件 | 板载没有七线的上下拉/串阻/电平转换；是否加源端串阻由真实线长和波形决定 |

本轮没有执行 Vivado `check_syntax`：现有流程会更新 XPR/source registration，且已发现
器件速度级冲突；继续在错误 part、空 XDC 下再跑一次语法检查对本轮板级结论没有新增
价值。软件/RTL 审计不代表 SPI 硬件成功。

当前准确停点：把本节完整转交 nRF Codex，由其确认/实测 `VDD_GPIO`、为所选七线给出
Connect Kit Rev.A 的 P0/P1 GPIOTE-capable GPIO 映射，并确认两事务、mode 0、1 MHz、
DRDY 高有效、SYNC 上升沿和 RESET 暂不连接是否可接受。返回冻结结果后才允许修改
ZYNQ part、RTL/XDC；不要先实现四路或生成 bitstream。

恢复工作的第一条命令：

```powershell
git -C F:\OliverS\AI_Embedded\F730+FPGA\zynq7020 status --short --branch
```

### 9.10 2026-08-18：CH0 SPI 测试镜像完成，停在实板烧录

本节承接 9.9，并采用 nRF handoff 第 12 节已冻结且已实现的接口。只实现
CH0；没有连接或修改 FMC/F730，没有展开 CH1～CH3，也没有声称板上 SPI
已经通过。根仓库仍为 `codex/work1-validation` / `c3aa0268712428d05965583901293f3979585414`，
ZYNQ 子仓库仍为 `codex/zynq7020-v1` /
`65c8c6004655501ced7ae5e7f027196694631b38`，两个工作树均保留原有未提交修改。

#### 9.10.1 器件、时钟和正式约束

- 实物照片确认 7020 板卡 SKU；匹配的官方领航者规格书 V1.2 在第 6、9、10、14
  页均把 7020 型号列为 `XC7Z020CLG400-2`。照片芯片顶标本身只能读到
  `XC7Z020/CLG400`，速度级结论来自同一官方板卡 SKU 文档，而非由照片猜测。
- 唯一 XPR 已由 `xc7z020clg400-1` 修正为 `xc7z020clg400-2`，BoardPart 保持空，
  顶层切换为专用 `ch0_test_top`。
- `constraints/ch0_test.xdc` 约束板载 U18 为 50 MHz；J4 CH0 为
  T5/MOSI、U7/MISO、V8/CS_N、U8/DRDY、T9/SCK、V6/SYNC_OUT，全部
  Bank13/LVCMOS33。RESET_N 的 Y6/J4-16 没有顶层端口和 XDC，首轮必须不接线。
- 输出复位安全态为 `CS_N=1`、`SCK=0`、`MOSI=0`、`SYNC_OUT=0`；输出采用
  8 mA、SLOW slew。板载路径没有电平转换或串联电阻。

#### 9.10.2 CH0 实现

- `spi_fixed_transaction_master.v`：SPI mode 0、MSB first、1 MHz、全事务保持
  CS，setup/hold 均 1 us。
- `ch0_spi_controller.v`：每个命令分别执行 8-byte request 与 260-byte
  response，A/B 间隔 1 ms，response MOSI 填 0；依次执行 GET_INFO、
  GET_STATUS、ARM_SYNC(epoch=1)、1 us SYNC、START_STREAM，再按 DRDY 高电平
  PEEK。
- PEEK payload 先经现有 248-byte parser 做格式、header CRC16 和 payload
  CRC32 校验。首条合法记录依次发错误 sequence COMMIT、正确 COMMIT、重复
  COMMIT，用于确认拒绝/释放语义；只有正确 COMMIT 后才允许向下游输出记录。
  此故障注入只发生一次，且会有意增加 nRF 的 bad/duplicate commit 计数。
- `ACTIVE_CHANNEL_MASK=4'b0001` 已加入 `board_top` 和 aligner；专用 CH0 顶层
  不实例化 FMC/四路数据面，因此不会等待其他通道。
- ILA 深度 4096，保留 412 个 probe bits，包括 request/response 计数、命令、
  status、transport_seq、record_valid/data/last、CRC、commit、bad/duplicate
  reject、timeout、envelope/parser error、DRDY、CS、SYNC 和 FSM state。

#### 9.10.3 离线验证和生成物

| 项目 | 结果 |
| --- | --- |
| CH0 SPI/nRF 从机模型 | `TB_CH0_SPI_CONTROLLER_OK transactions=8 bytes=248`；逐字节匹配共享 golden，并检查 1 us setup/hold、1 ms A/B、300 ns inactive、1 us SYNC |
| CH0 aligner | `TB_CH0_RECORD_ALIGNER_OK bytes=248`，`ACTIVE_CHANNEL_MASK=0001` 不等待其他三路 |
| 原四路 aligner 回归 | `TB_FOUR_CHANNEL_RECORD_ALIGNER_OK groups=4 drops=2/1/0/0 errors=3` |
| Vivado 2025.2 check_syntax | 通过 |
| synthesis | `synth_design Complete!` |
| implementation/bitstream | `write_bitstream Complete!`，0 unrouted net |
| 时序 | WNS 7.648 ns，WHS 0.048 ns，TNS/THS 0；0 个 unconstrained internal endpoint |
| DRC | 0 Error；5 个 Warning 均来自自动插入 debug hub 或未实例化 PS7 的 PL-only 镜像，详见 `build/ch0/drc.rpt` |

Vivado 报告 `All user specified timing constraints are met`。该结论覆盖 50 MHz
PL 内部时序；nRF 的外部 MISO tCO、飞线波形和板级 setup/hold 仍必须用 ILA/逻辑
分析仪实测，不能由实现报告替代。

生成物：

- bit：`zynq7020/zynq7020_bridge/zynq7020_bridge.runs/impl_1/ch0_test_top.bit`，
  SHA-256 `d3f5276f4f8ff0cf4d221836f8982e366df78b4e9bcb3cb4c6590913264a3a31`；
- ltx：`zynq7020/zynq7020_bridge/build/ch0/ch0_test_top.ltx`，
  SHA-256 `df44ba60db19cda2c6d50265abd87c070d37598cd425aa6dde5d5feeb284e54e`；
- 报告：`zynq7020/zynq7020_bridge/build/ch0/{build_summary.txt,timing_summary.rpt,drc.rpt,io.rpt,utilization.rpt,clocks.rpt}`。

#### 9.10.4 下一步：Hardware Manager 与 CH0 实板验证

1. 先断电接线并再次用万用表确认双方 GPIO 均为约 3.3 V。只连接公共 GND、
   J4-4 MOSI、J4-6 MISO、J4-8 CS_N、J4-10 DRDY、J4-12 SCK、J4-14
   SYNC_OUT；J4-16 RESET_N、J4-38 3.3 V、J4-40 5 V 均不接。
2. 给 nRF RX0 和 ZYNQ 上电，确保 RX0 已烧录 handoff 第 12 节对应固件。
3. Vivado 打开唯一 XPR，进入 Hardware Manager，`Open Target -> Auto Connect`，
   选中检测到的 `xc7z020` 器件，`Program Device`：Bitstream 选择上述 `.bit`，Debug probes
   选择上述 `.ltx`，然后 Program。若 ILA 无法识别，先把下载器 JTAG frequency
   降至 10 MHz，再重新连接，不改 SPI 频率。
4. 打开 ILA，先触发 `last_command==1`/GET_INFO 或 `ch0_cs_n` 下降沿；确认启动
   命令顺序 1、2、8、9，随后 DRDY 高时为 3、5、5、5。首条记录预期
   `crc_ok_count=1`、`commit_ok_count=1`、`bad_commit_reject_count=1`、
   `duplicate_commit_reject_count=1`，同时 `crc_error_count=0`、
   `commit_error_count=0`、`timeout_count=0`、`envelope_error_count=0`。
5. 同时保存 RX0 从上电开始的日志和至少一组 ILA capture；若有逻辑分析仪，量测
   SCK 1 MHz、mode 0、CS setup/hold、A/B 间隔和 SYNC 1 us。未取得这些证据前，
   状态只能写为“CH0 测试 bitstream 已生成”，不能写为“nRF-ZYNQ SPI 已通过”。

恢复构建命令：

```powershell
cd F:\OliverS\AI_Embedded\F730+FPGA\zynq7020\zynq7020_bridge
E:\Xilinx\2025.2\Vivado\bin\vivado.bat -mode batch -source scripts\build_ch0.tcl -nolog -nojournal
```

### 9.11 2026-09-03：CH0 实板首次闭环后的 FPGA 事务间隔修正

首次下载 9.10 镜像后，JTAG/ILA 均正常工作，但 SPI 协议没有进入启动序列：

- RX0 日志从 `req_xfer=0/rsp_xfer=0` 增长到 `req_xfer=3506`、
  `rsp_xfer=3506`，同时 `invalid_cmd=3503`、`short_xfer=3502`，
  `CONTROL armed=0/started=0`；无线接收本身仍在运行。
- `Imp/iladata.csv` 中 FPGA 停留在 GET_INFO，抓取时 `request_count=0x145`、
  `response_count=0x145`、`envelope_error_count=0x145`，没有 CRC/COMMIT 成功。

根因是 9.10 只在 request A 与 response B 之间等待 1 ms，而 response B 结束到下一条
request A 仅等待 `CS_INACTIVE_CYCLES=15`，即 50 MHz 下 300 ns。300 ns 满足电气
inactive 下限，却不足以让当前 nRF Zephyr SPIS worker 从 B 事务返回并重新装载下一次
8-byte request 的 EasyDMA buffer，导致 nRF 交替把 8 B/260 B buffer 用在错误事务上，
形成持续 `short_xfer`/`invalid_cmd`。

#### 9.11.1 修正内容

- `ch0_spi_controller.v` 新增独立参数
  `B_TO_NEXT_A_GAP_CYCLES=250000`；在 50 MHz 下，B 结束后等待 5 ms 才允许下一条
  request A。A/B 间隔仍为 1 ms，SPI 仍为 mode 0、MSB first、1 MHz，CS setup/hold
  仍为 1 us。
- `REQUEST_GAP_CYCLES=max(B_TO_NEXT_A_GAP_CYCLES, CS_INACTIVE_CYCLES)`，保留
  300 ns 作为所有 CS 事务的电气最小值；新增参数合法性检查。
- testbench 首次故意返回坏 GET_INFO envelope，随后要求控制器恢复并重试，且逐次断言
  B 到下一 A 的间隔不少于 5 ms。通过标志为
  `TB_CH0_SPI_CONTROLLER_OK transactions=9 bytes=248 envelope_recovery=1 b_to_next_a_ns=5000000`。
- parser、block builder、ping-pong BRAM、四路 aligner 和 CH0 aligner 回归均通过。

#### 9.11.2 构建签核和新生成物

Vivado 2025.2 对 `xc7z020clg400-2`/`ch0_test_top` 完成综合、实现和 bitstream：

- synthesis：`synth_design Complete!`；implementation：`write_bitstream Complete!`；
- route：0 failed/unrouted net；WNS 8.729 ns，WHS 0.059 ns；
- DRC Error 0；最终脚本输出 `CH0_FINALIZE_OK`。

新 bit：`zynq7020/zynq7020_bridge/zynq7020_bridge.runs/impl_1/ch0_test_top.bit`，
SHA-256 `a77051c78d6f1f3a209a00e6228c730990267f3f7f4c02a620cac8d7af5f4f8e`。

配套 ltx：`zynq7020/zynq7020_bridge/build/ch0/ch0_test_top.ltx`，
SHA-256 `df44ba60db19cda2c6d50265abd87c070d37598cd425aa6dde5d5feeb284e54e`。

#### 9.11.3 下一步只做重新烧录和 CH0 复测

使用上述新 bit 和 ltx 重新 Program Device；不要改接线，也不要连接 F730/FMC。RX0
保持正确 CH0 SPIS 固件，最好先复位 RX0 再下载 FPGA。运行后先观察：

1. RX0 的 `short_xfer`、`invalid_cmd` 不再随每个事务连续增长；
2. `CONTROL` 依次出现 armed/started，`SYNC capture_count` 增长；
3. FPGA 不再停在 GET_INFO，ILA 的 `last_command` 依次经过 1、2、8、9，再进入 3/5；
4. `envelope_error_count` 停止连续增长，随后出现 CRC 和 COMMIT 结果。

本节只证明 RTL、仿真与 Vivado 签核完成；新镜像尚未实板复测，不能写成 CH0 SPI
硬件已经通过。5 ms 会显著限制吞吐，只用于打通第一条闭环；若该间隔验证有效，最终
持续吞吐仍需 nRF SPIS 异步/双缓冲或等效自恢复机制配合后再逐步缩短。

### 9.12 2026-09-03：实板确认半速错相，增加 A/B 保护与确定性相位恢复

9.11 镜像实板运行后不再停在启动前：RX0 已出现 `CONTROL armed=1 started=1`、
`sync_capture_count=1`、`records=1`，证明 GET_INFO/GET_STATUS/ARM/START 和至少一次
PEEK 数据面能够成功。但错误仍持续：截图末段约为 `req_xfer=14477`、
`rsp_xfer=14477`、`invalid_cmd=7117`、`short_xfer=7155`；队列已满并开始 overflow。
新 `Imp/iladata.csv` 对应时刻可重组出 FPGA `request_count=response_count=13235`、
`crc_ok_count=6403`、`envelope_error_count=6827`，即成功与失败近似各占一半。

这排除了“FPGA 未启动”和“完全未接通”。其模式符合阻塞式 nRF SPIS 两阶段状态机
发生相位反转：若某次 260 B response B 在从机尚未重装时开始，随后 FPGA 的 8 B A
会被从机仍在等待的 response 阶段当作 short transfer，而下一次 260 B B 又会被 request
阶段解释为非法命令；仅增加 B/next-A 等待无法从该闭环自行恢复。

#### 9.12.1 FPGA 修正

- A/B 软件准备间隔由 1 ms 提高到 5 ms；B/next-A 继续为 5 ms。SPI mode、1 MHz、
  setup/hold 和线协议均不变。
- 新增 `ST_RECOVERY_WAIT`、`ST_START_RECOVERY`、`ST_WAIT_RECOVERY`。发生 response
  envelope/parser/CRC、COMMIT 或 transaction timeout 错误后，等待 5 ms，发送一次
  260 B MOSI=0 的恢复事务，再经 retry/request gap 重发命令。
- 该 260 B 长事务可确定性地把当前 nRF worker 收敛到 request phase：处于 response
  phase 时完成并返回 request；处于 request phase 时长度错误后仍停留在 request。
  因此一次漏装载不会再形成永久 A/B 反相。恢复动作本身在后一种情况下会有意增加一次
  nRF `short_xfer`，判据应是错误停止连续增长，而不是历史累计值归零。

#### 9.12.2 验证和新生成物

- 相位故障注入仿真主动漏掉首个 response，随后恢复并完整完成 9 个命令响应、248 B
  record 和错误/正确/重复 COMMIT：
  `TB_CH0_SPI_CONTROLLER_OK transactions=9 bytes=248 phase_flush=1 b_to_next_a_ns=5000000`。
- parser、block builder、ping-pong BRAM、四路 aligner、CH0 aligner 全部回归通过。
- Vivado 2025.2：`xc7z020clg400-2`、`ch0_test_top`，synthesis 和 bitstream complete，
  0 failed/unrouted net，WNS 8.792 ns、WHS 0.038 ns、DRC Error 0，
  `CH0_FINALIZE_OK`。

新 bit：`zynq7020/zynq7020_bridge/zynq7020_bridge.runs/impl_1/ch0_test_top.bit`，
SHA-256 `e8c2c29abe85334d20f804658298fecfa6fb17d67e844c40db1e3f35fdf155ed`。

新 ltx：`zynq7020/zynq7020_bridge/build/ch0/ch0_test_top.ltx`，
SHA-256 `5f76959bbb0d54d3d9c6b37dfb089395d62192d518b75bd71f1dd1fe0b1e3000`。

下一步：RX0 重新上电并等待 `radio ready`，再用本节新 bit/ltx Program Device。
记录烧录后至少 30 秒的 RX0 日志及新 ILA CSV。通过判据为 `CONTROL`/SYNC 保持有效、
`records` 与 `QUEUE pop_total` 持续增长、`invalid_cmd`/`short_xfer` 仅在恢复事件附近偶发
而不再近似每两事务增长一次，并且 FPGA CRC/COMMIT 计数持续增长。新镜像尚未实板
复测，因此当前仍不能宣称 CH0 SPI 硬件通过。

### 9.13 2026-09-03：nRF CRC 修正后 CH0 建立功能闭环

nRF 侧将 record header CRC16 修正为冻结的 CRC-16/CCITT-FALSE 后重新烧录。用户回传
的新 COM11 日志从启动零计数持续运行，最终关键值为：

```text
SPI_TRANSPORT records=1957 crc_errors=0 invalid_cmd=44 duplicate_commit=0
submit_errors=49 stall_ms=2 req_xfer=3996 rsp_xfer=3995
spi_errors=0 parser_errors=0 short_xfer=29
QUEUE push_total=2008 pop_total=1956 overflow=4270 high_water=64 level=52
SYNC state=5 sync_capture_count=1 sync_timeout=0 sync_epoch=1 sync_locked=0
CONTROL armed=1 started=1 stopped=0 reset=0 invalid=0
```

这批证据确认：

- CRC 算法不匹配的主故障已经解除；
- CH0 request/response 持续运行，真实 record 被读取，正确 COMMIT 释放队首；
- SPI 驱动和 reserved parser 没有报告错误；
- SYNC 上升沿曾被捕获并进入 stream。

当前只能标记“CH0 功能闭环建立”，还不能标记“持续无损链路通过”：

- `invalid_cmd` 约占 request 的 1.10%，`short_xfer` 约占 0.73%，仍有偶发恢复；
- `submit_errors=49` 与 nRF 主循环和 COMMIT worker 并发调用 transport service 的
  stage 竞争相符，需由 nRF 工程消除；
- FPGA 当前 1 MHz、A/B 与 B/next-A 各 5 ms 的 bring-up 时序无法追上 RF 输入，
  nRF record queue 已 overflow 4270；
- `sync_locked=0`，且没有四路公共 TX/MEMS 采样时基，不能宣称物理同步；
- 同次 `zynq7020_bridge/Imp/iladata.csv` 只有字段头和 Radix 行，没有 ILA 样本，
  尚未取得 FPGA `crc_ok_count`/`commit_ok_count` 的可归档计数。

准确下一步：保持 FPGA RTL 不变，重新 Run Trigger/Immediate 并导出含采样行的 ILA；
同时由 nRF 对话修复 transport service 并发 submit。两项完成后复测 30 min，再以零新增
CRC/parser/commit error、无持续 invalid/short、queue 不 overflow 为判据逐步缩短 5 ms
保护间隔。F730/FMC 仍未接入本 Gate。

## 10. 2026-09-03：三工程与论文工作区整理

为方便后续切换对话，已在本工作区补充：

- 总入口 `README.md`；
- 迁移说明 `docs/WORKSPACE_MIGRATION.md`；
- 当前可直接打开的 `multisensor-research.code-workspace`；
- `paper/` 论文提纲、图表、表格、证据和引用目录；
- F730/FPGA README、路线图、接口、设计与开发日志的 2026-09-03 状态。

本轮严格只修改 `F:\OliverS\AI_Embedded\F730+FPGA`，没有修改外部 nRF 仓库。
推荐迁移保持现有顶层 Git 和 `zynq7020/.git` 不变：完整复制本目录到新位置，再把
外部 `nrf54l15-connectkit` 作为独立嵌套 Git 放到新根目录。不要拆分 F730 子目录、
不要删除任一 `.git`、不要复制整个 NCS SDK，也不要用远端 clone 覆盖当前 dirty
工作树。迁移后只需把 `.code-workspace` 中 nRF 路径改为相对目录，并对 Vivado、nRF、
F730 的旧 build cache 做 clean rebuild。

下一次对话的固定恢复顺序：读根 `README.md`、本文件最后一节、
`bridge_contract_v0.md`、当前子工程 README；随后报告顶层/ZYNQ/nRF 三个 Git 根的
branch、HEAD、dirty，再开始任务。论文结论必须回指 `paper/evidence/` 的实验 manifest，
不得把历史绝对路径、截图或无采样行 CSV 当作唯一证据。

本轮文档整理时的 Git 快照：顶层分支 `codex/work1-validation`、HEAD
`c3aa0268712428d05965583901293f3979585414`、dirty；ZYNQ 分支
`codex/zynq7020-v1`、HEAD `65c8c6004655501ced7ae5e7f027196694631b38`、dirty。
外部 nRF 仅只读确认其分支/基线和当前接口证据，没有写入。CH0 本次论文证据清单保存为
`paper/evidence/20260903_ch0_crc_commit.md`。
