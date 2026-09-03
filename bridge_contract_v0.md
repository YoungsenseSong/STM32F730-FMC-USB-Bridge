# F730 - ZYNQ7020 - nRF54L15 bridge contract v0

更新日期：2026-09-03
状态：软件数据面冻结；CH0 nRF-ZYNQ 接口已冻结并建立实板功能闭环；四路/FMC/USB 产品项仍受门控

## 1. 适用基线

| 端 | 基线 |
| --- | --- |
| F730 | `c3aa0268712428d05965583901293f3979585414`，分支 `codex/work1-validation` 从该提交创建 |
| ZYNQ7020 | 基线提交 `65c8c6004655501ced7ae5e7f027196694631b38` 加当前 dirty CH0 实现；实物速度级确认后器件为 `xc7z020clg400-2`，CH0 顶层 `ch0_test_top` |
| nRF54L15 | 外部仓库分支 `codex/rf-link-2p4g-devlog`，基线提交 `4e360863a3569c9494197ec701604404519d811d` 加当前 dirty SPIS/CRC 修正；由 nRF 工作区维护 |

所有多字节整数均为 little-endian。所有保留字段发送时必须写 0，接收时必须忽略
未知保留位但拒绝不兼容的版本。

## 2. 冻结状态

### 已冻结

- nRF 到 ZYNQ 的 248 B record 字节布局；
- record magic、版本、长度、CRC 参数和覆盖范围；
- nRF 软件协议引擎的命令编号及 PEEK/COMMIT/DROP 所有权规则；
- ZYNQ 到 F730 的 64 B block header、完整 record 边界和 payload CRC；
- FMC CSR 的现有 byte offset、16-bit little-endian 访问和 BLOCK_ACK 成功规则；
- USB 应用帧的通用头部、类型、分片无关解析和 payload CRC。
- CH0 使用 J4/Bank13/LVCMOS33、SPI mode 0、MSB first、初始 1 MHz；
- CH0 request A 为 8 B、response B 为 260 B，两次独立 CS；DRDY 高有效保持型，
  SYNC_IN 上升沿，RESET_N 首轮不接。

### 仍为 `CONFIG_REQUIRED` 或尚未完成验收

- CH1..CH3 的完整板级映射、扇出和四路同时运行时序；
- CH0 从 bring-up 的双 5 ms 保护间隔优化到可持续无损吞吐的最终 SPIS 时序；
- RESET_N 的最终驱动方式、极性、上电默认值和复位范围；
- ZYNQ 的最终产品 `FPGA_ID` 和 build 编码；
- USB 合法 VID/PID、制造商、产品名和序列号来源；
- NWAIT 最大拉低时间和最终 FMC 时序裕量。

CH0 测试 bitstream 已完成综合、实现、时序、DRC 和实板功能验证，但它不是产品
bitstream。上述产品项未冻结前，不得把 CH0 镜像扩展结论写成四路/FMC/量产通过。

## 3. nRF -> ZYNQ 固定 record

总长度固定为 248 B：40 B header + 204 B V1 RF frame + 4 B payload CRC32。

| Byte | 长度 | 字段 | v0 规则 |
| ---: | ---: | --- | --- |
| 0 | 4 | `magic` | `0x3146524E`，线上字节为 `4E 52 46 31` (`NRF1`) |
| 4 | 1 | `version` | `1` |
| 5 | 1 | `node_id` | `0..255`；四通道部署时由系统配置绑定，不由 FPGA 猜测 |
| 6 | 2 | `record_type` | `1` 表示 V1 RF frame |
| 8 | 4 | `transport_seq` | 32-bit 自然回绕；COMMIT/DROP 必须精确匹配 |
| 12 | 4 | `sync_epoch` | ARM 时给定的逻辑同步 epoch |
| 16 | 8 | `rx_tick` | nRF 接收时间基准 tick |
| 24 | 8 | `logical_sample_index` | epoch 内逻辑样本起点 |
| 32 | 4 | `status_flags` | 见下表 |
| 36 | 2 | `payload_len` | 固定 `204` |
| 38 | 2 | `header_crc16` | 覆盖 byte `0..37` |
| 40 | 204 | `rf_frame` | 原始 V1 空中帧 |
| 244 | 4 | `payload_crc32` | 只覆盖 byte `40..243` |

`status_flags`：bit0 valid、bit1 gap_before、bit2 index_estimated、bit3
batch_start、bit4 batch_end、bit5 synced、bit6 sync_degraded；bit31:7 保留。

204 B `rf_frame`：

| Record byte | 长度 | 字段 |
| ---: | ---: | --- |
| 40 | 2 | magic `0xA55A` |
| 42 | 2 | 16-bit RF sequence |
| 44 | 2 | sample count，`1..96` |
| 46 | 2 | V1 RF flags |
| 48 | 4 | timestamp in ms |
| 52 | 192 | 96 个 little-endian `int16_t` 样本 |

接收端必须在入 FIFO 前同时验证 magic、version、record type、payload length、
header CRC16 和 payload CRC32。PEEK/READ 不释放记录；只有匹配当前
`transport_seq` 的 COMMIT 或 DROP 才释放。错误 sequence 不得释放；重复 COMMIT
单独计数。

## 4. CRC

### Header CRC16

- 名称：CRC-16/CCITT-FALSE；
- polynomial `0x1021`；init `0xFFFF`；refin/refout false；xorout `0x0000`；
- check(`123456789`) = `0x29B1`；
- record header 覆盖 byte `0..37`，结果以 little-endian 放在 byte `38..39`。

### Payload/block/USB CRC32

- 名称：CRC-32/ISO-HDLC（与 Zephyr `crc32_ieee()`、Python
  `binascii.crc32()` 一致）；
- normal polynomial `0x04C11DB7`，实现使用 reflected polynomial
  `0xEDB88320`；init `0xFFFFFFFF`；refin/refout true；xorout `0xFFFFFFFF`；
- check(`123456789`) = `0xCBF43926`；
- 结果以 little-endian 存放。

不得用当前 provisional 的 MSB-first CRC32 实现代替该算法。

## 5. nRF 软件命令语义

命令编号与远端 `fpga_spi_transport.h` 一致：

| Code | 命令 | 所有权影响 |
| ---: | --- | --- |
| 1 | `GET_INFO` | 无 |
| 2 | `GET_STATUS` | 无 |
| 3 | `PEEK_RECORD` | 返回当前记录，不释放 |
| 4 | `READ_RECORD` | 返回当前记录，不释放 |
| 5 | `COMMIT_RECORD` | argument 匹配时成功释放 |
| 6 | `DROP_RECORD` | argument 匹配时丢弃并释放 |
| 7 | `CLEAR_STATS` | 无 |
| 8 | `ARM_SYNC` | argument 为 epoch；清除旧 pending/alignment |
| 9 | `START_STREAM` | 无记录所有权变化 |
| 10 | `STOP_STREAM` | 无记录所有权变化 |
| 11 | `RESET_LINK` | 中止 pending 并复位链路状态 |

逻辑 request 固定为 8 B：byte0 code、byte1..3 为 0、byte4..7 为 argument。
逻辑 response 固定为 260 B：byte0..3 signed status、byte4..7
transport sequence、byte8 record_valid、byte9..11 为 0、byte12..259 为
248 B record。

这里冻结的是序列化字节，不允许直接依赖 C 结构体 padding。CH0 真实 SPIS backend
与专用 FPGA controller 已采用两个 CS 事务：A=8 B request，等待 5 ms，B=260 B
response/MOSI 全 0；B 到下一 A 也等待 5 ms。SPI mode 0、MSB first、初始 1 MHz，
CS setup/hold 各 1 us。5 ms 属于 bring-up 保护值，不是最终吞吐配置。

DRDY 为高有效保持型，在 pending record 存在期间保持高。SYNC_IN 取上升沿，首次脉冲
高电平 1 us、空闲低。RESET_N 首轮不接线。

## 6. ZYNQ -> F730 block

### 6.1 四路 record 对齐规则

ZYNQ 在写 block 前对四路各保留一个已通过 record CRC 的槽位。只有四路
`status_flags.SYNCED=1`、`sync_epoch` 与当前 FPGA epoch 相同、且
`logical_sample_index` 完全相同，才按 channel 0、1、2、3 输出一组。未同步、旧 epoch
或旧逻辑索引记录必须丢弃并计数，不允许复制、插零或改写原始 204 B RF frame。

该规则只冻结 record 级逻辑整数样本对齐。`rx_tick` 当前属于各 RX nRF 本地时钟，不可
跨路直接比较；V1 空中协议没有公共 TX/MEMS 采样时基或高分辨率采样时间戳。因此分数
采样相位补偿、漂移校正和每路固定延迟值仍未冻结，必须等待 V2 空中协议与标定证据。

每个 block 为 64 B header 加 `N * 248 B` 完整记录。v0 推荐目标
`N=66`（payload 16,368 B），最大 `N=132`（payload 32,736 B）；禁止在 record
中间封块。

64 B header：

| Byte | 长度 | 字段 |
| ---: | ---: | --- |
| 0 | 4 | magic `0x31425046` (`FPB1`) |
| 4 | 2 | protocol version `1` |
| 6 | 2 | header bytes `64` |
| 8 | 4 | block sequence |
| 12 | 4 | sync epoch |
| 16 | 4 | payload bytes，必须为 248 的整数倍 |
| 20 | 4 | channel mask，仅低 4 位有效 |
| 24 | 8 | first FPGA tick |
| 32 | 8 | last FPGA tick |
| 40 | 16 | channel 0..3 record counts，各 32-bit |
| 56 | 4 | flags |
| 60 | 4 | payload CRC32，覆盖 header 后的全部 payload |

flags：bit0 输入记录错误、bit1 最大长度边界异常、bit2 STOP flush 边界异常、
bit3 timeout flush；其余保留。正常 block 的 bit3:0 必须全为 0。

F730 必须先读取 `BLOCK_STATUS/LEN/STATUS` 稳定快照，再读取 64 B header，验证
header 和长度，读取 payload，验证 CRC，最后才 ACK。任何失败都不得 ACK；v0
没有单独的 FMC NACK 编码，固件最多重读 3 次，仍失败则记录错误并走受控 FPGA
reset/重新识别流程。

`BLOCK_ACK` 写入当前 `block_sequence[15:0]`。只有当前被 MCU claim 的 bank 且
值匹配时才释放；错误、过期或重复值不得释放其他 bank。32-bit sequence 自然
回绕。

## 7. FMC/NWAIT

FMC byte base 为 `0x60000000`，16-bit word address 对应 byte offset / 2；数据
窗口和 CSR 均按完整 16-bit 访问。

v0 采用 FPGA 已实现的低有效 `NWAIT` 语义，F730 侧启用异步 wait sampling；这是
唯一能覆盖 BRAM 非零读取延迟且不靠猜固定周期的策略。最终 ADDSET/DATAST、
NWAIT 最大拉低时间、采样边沿和恢复策略必须用逻辑分析仪验证。在 XDC/走线未
冻结前，这一条只代表控制器语义，不代表板级电气验证通过。

## 8. USB Vendor Bulk 应用帧

USB interface class/subclass/protocol 均为 vendor-specific (`0xFF/0x00/0x00`)；
FS endpoint 建议为 OUT `0x01`、IN `0x81`，max packet 64 B。合法 VID/PID 和
字符串未提供，因此描述符身份仍为 `CONFIG_REQUIRED`。

USB bulk 字节流使用固定 24 B common header；USB 64 B 分片和多帧粘连不得改变
解析结果。

| Byte | 长度 | 字段 |
| ---: | ---: | --- |
| 0 | 4 | magic `0x31524255`，线上 `UBR1` |
| 4 | 2 | protocol version `1` |
| 6 | 2 | type：1 DATA、2 CMD、3 RSP、4 EVENT |
| 8 | 4 | packet sequence |
| 12 | 4 | message id：DATA=block seq；CMD/RSP=command id；EVENT=event id |
| 16 | 4 | payload length |
| 20 | 4 | payload CRC32 |

DATA payload 是未经改写的完整 FPGA block。CMD/RSP/EVENT payload 按各 message
id 定义；未知 id 返回明确错误，不得静默忽略。相同非零 command id 的重试必须
幂等，缓存最近响应；DATA 的 packet sequence 与 block sequence 均保留，便于定位
USB 侧丢失。

## 9. 版本拒绝规则

- magic 错误：丢弃到下一个 magic，增加 framing error；
- major/version 不等于 1：拒绝整条消息/记录，不做降级猜测；
- 长度超上限或不是规定整数倍：拒绝且不改变所有权；
- CRC 错误：拒绝且不 ACK/COMMIT；
- 保留发送位非 0：v0 接收端记录 protocol warning，但仅在不影响长度/所有权时
  可以继续；
- 所有错误都必须进入可读计数器，禁止静默丢弃。

## 10. Golden vectors

仓库 `protocol/golden/` 由 `protocol/generate_golden_vectors.py` 生成并带 SHA-256。
至少包含：

1. 合法 248 B nRF record；
2. header CRC 损坏 record；
3. payload CRC 损坏 record；
4. 两条 record 组成的合法 560 B block；
5. 合法 DATA USB frame；
6. USB 分片/粘包输入序列。

nRF 源码模型、ZYNQ testbench、F730 C codec 和 host Python 测试必须消费同一组
二进制文件，禁止各端各自生成只验证自身的“自洽”向量。
