# 四路同步无线采集与 USB 桥接工作区

> 团队开发交接版：先读 [TEAM_HANDOFF.md](TEAM_HANDOFF.md) 和 [验证摘要](HANDOFF_VALIDATION.md)。本仓是 F730 与共享协议仓，不包含独立 ZYNQ/nRF 仓。本轮 F730 固件未完成构建，仅有协议/CDC 宿主测试证据；下文历史记录不代表当前镜像已通过板测。

本目录是学生科研项目的系统级入口，包含 STM32F730、ZYNQ7020、三端共享协议、
板级资料与论文材料。nRF54L15 工程目前仍位于外部 NCS 工作区；迁移后建议作为独立
Git 仓库放到本目录的 `nrf54l15-connectkit/`，不要合并三个仓库的历史。

## 系统结构

```text
传感器 / nRF TX
  -> 2.4 GHz
  -> nRF RX (SPIS + DRDY + SYNC capture)
  -> ZYNQ7020 PL (SPI master + record/block/FMC)
  -> STM32F730 (FMC + USB device)
  -> PC
```

## 第一次进入工作区的阅读顺序

1. `handoff.md` 最后一节：最近一次实板证据和准确停点；
2. `bridge_contract_v0.md`：三端唯一共享的字节、CRC、所有权和错误语义；
3. `docs/WORKSPACE_MIGRATION.md`：Git 边界、迁移方法和不可复制的缓存；
4. 当前任务所属工程的入口文档：
   - F730：`f730_bridge/README.md`；
   - FPGA：`zynq7020/zynq7020_bridge/docs/FPGA_README.md`；
   - nRF：外部仓库自己的 `handoff.md`；
5. 论文工作：已迁至同级独立写作目录 `../paper/`，使用根目录
   `../paper.code-workspace` 打开，不纳入工程 Git。

新对话不得只依据旧章节推断当前状态；先执行三个仓库各自的
`git status --short --branch` 和 `git rev-parse HEAD`。所有仓库当前都有未提交修改，
禁止 reset、clean、覆盖式 checkout 或用远端 clone 替代现有工作树。

## 当前工程边界（2026-09-03）

| 工程 | 当前目录 | Git 边界 | 当前硬件结论 |
| --- | --- | --- | --- |
| STM32F730 | `f730_bridge/` | 顶层仓库 | Gate A 已通过；CDC/FMC 完整链路仍需继续上板 |
| ZYNQ7020 | `zynq7020/` | 独立嵌套仓库 | CH0 SPI 主链路已产生有效 record/COMMIT；吞吐与残余恢复错误未验收 |
| nRF54L15 | 外部 `D:/nRF54L15/NCS-Project/nrf54l15-connectkit/` | 独立仓库 | CRC-16 修正版已与 CH0 实板闭环；由 nRF 对话维护 |
| 论文 | `paper/` | 顶层仓库 | 只引用可追溯的日志、CSV、bit/hex 哈希和明确验证边界 |

最近 CH0 串口证据达到 `records=1957`、`pop_total=1956`、`crc_errors=0`，证明
CRC 修正后的 PEEK/COMMIT 主链路工作。仍有 `invalid_cmd=44`、`short_xfer=29`、
`submit_errors=49`，且 record queue `overflow=4270`；因此只能写成“CH0 功能闭环已
建立”，不能写成“持续无损传输通过”。同次 `Imp/iladata.csv` 只有字段头，没有采样行，
FPGA 计数仍需重新导出。

## 构建输出与证据

- 源码、XDC、Tcl、测试和协议文档进入对应 Git；
- `.runs/`、`.cache/`、`build*/`、Keil Objects 等生成物不得作为唯一证据；
- 实际烧录的 HEX/BIT/LTX 应记录 SHA-256，并把小型报告或清单放入独立写作目录
  `../paper/evidence/`；大型原始数据只保存路径、哈希和测试条件；
- 软件构建、RTL 仿真、bitstream 生成和实板通过必须分别表述。

## 工作区与迁移

当前可直接打开 `multisensor-research.code-workspace`。迁移到新目录时，只需完整复制
本目录，并把 nRF 自定义仓库放入 `nrf54l15-connectkit/`，再把工作区文件中的 nRF
路径改成相对路径。完整步骤见 `docs/WORKSPACE_MIGRATION.md`。
