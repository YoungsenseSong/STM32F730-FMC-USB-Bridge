# 三工程与论文工作区迁移说明

更新时间：2026-09-03

> 迁移后实际布局更新：用户已选择把工程与论文写作分开管理。当前工程顶层为
> `F:/OliverS/MultiSensorResearch/F730+FPGA/`，nRF 仓库位于同级
> `../nrf54l15-connectkit/`；论文位于同级 `../paper/`，不使用 Git。工程与论文分别从
> `../engineering.code-workspace` 和 `../paper.code-workspace` 打开。本节以下内容保留为
> 迁移过程依据，其中“paper 属于 F730 顶层 Git”的旧规划已被此选择取代。

## 结论

最简单且稳定的迁移不是拆分当前仓库，而是完整保留现有目录结构和两个已有 Git
边界，再把 nRF 自定义仓库放到新根目录中。不要复制或移动整个 Nordic NCS SDK，
也不要重新创建 Vivado、CubeMX 或 Keil 工程。

推荐新目录：

```text
MultiSensorResearch/
├─ .git/                         顶层 F730/系统文档仓库，必须保留
├─ README.md
├─ handoff.md
├─ bridge_contract_v0.md
├─ multisensor-research.code-workspace
├─ protocol/                     三端 golden vectors 与共享测试
├─ f730_bridge/                  STM32F730 工程
├─ zynq7020/                     ZYNQ 独立 Git 仓库，保留其 .git/
├─ nrf54l15-connectkit/          nRF 独立 Git 仓库，保留其 .git/
└─ paper/                        论文提纲、图表、实验索引和参考文献
```

顶层 `.gitignore` 已排除 `nrf54l15-connectkit/`，因此 nRF 仍由自己的 Git 管理。
ZYNQ 当前也是嵌套仓库；不要删除它的 `.git/`，不要把它强行纳入顶层历史。

## 必须迁移

### 当前工作区

直接完整复制 `F:/OliverS/AI_Embedded/F730+FPGA/`，包括隐藏的顶层 `.git/`、
`zynq7020/.git/`、未跟踪 RTL/XDC/USB 源码、`硬件资料/`、`protocol/` 和
`handoff.md`。当前三个工作树均有未提交修改，只 clone 远端会丢失这些内容。

### nRF 自定义工程

从 `D:/nRF54L15/NCS-Project/nrf54l15-connectkit/` 迁移以下内容：

- `.git/`；
- `applications/`、`tests/`、脚本、顶层 CMake/Kconfig；
- `README.md`、`handoff.md`、设计/路线图和必要实验记录；
- 当前所有未跟踪但属于源码的 overlay、DTS binding、SPIS backend 和测试目录。

最稳妥方式是先完整复制该仓库，再在新位置删除并重新生成 `build*`。不要仅依赖
`git clone`，因为当前 CRC/SPIS 工作仍在 dirty working tree 中。

## 不需要迁移或必须重建

- Nordic SDK 的 `zephyr/`、`nrf/`、`modules/`、toolchains 和 `.venv/`：继续使用已安装
  的 NCS 3.1.0；它们是工具链依赖，不是项目源码；
- nRF `build*`、`twister-out*`：含旧绝对路径，只保留最终 HEX 的哈希和构建日志；
- Vivado `.Xil/`、`.cache/`、`.hw/`、`.sim/`：均可重建；
- F730 `build/`、Keil Objects/Listings：均可重建；
- 临时串口日志、空 ILA CSV 和工具缓存：只把有论文价值的原始证据归档。

为避免遗漏 dirty 源文件，第一次迁移可以完整复制所有内容；验证新副本可构建以后，
再删除上述缓存。禁止使用带 `/MIR` 的同步命令覆盖原目录。

## 推荐的无删除复制流程

关闭 Vivado、Keil、串口工具后，在用户选定的新父目录执行普通复制或如下非镜像命令：

```powershell
robocopy "F:\OliverS\AI_Embedded\F730+FPGA" "F:\OliverS\AI_Embedded\MultiSensorResearch" /E /COPY:DAT /DCOPY:DAT /R:2 /W:2 /XJ
robocopy "D:\nRF54L15\NCS-Project\nrf54l15-connectkit" "F:\OliverS\AI_Embedded\MultiSensorResearch\nrf54l15-connectkit" /E /COPY:DAT /DCOPY:DAT /R:2 /W:2 /XJ
```

这里故意不使用 `/MOVE` 或 `/MIR`。确认新工作区完整可用前，原目录保持不动。

## 迁移后的四项修正

1. 把 `multisensor-research.code-workspace` 中 nRF 的绝对路径改为：

   ```json
   "path": "nrf54l15-connectkit"
   ```

2. 删除新位置的旧 Vivado 运行缓存，再从已有 XPR 或相对路径 Tcl 重新生成。XPR 的
   source 路径使用 `$PPRDIR`，Tcl 通过脚本自身位置求根目录；不要复制旧 DCP 当作新构建。
   删除 `.runs/`/`build/` 前，先按 `handoff.md` 中的 SHA-256 把实际烧录过的 BIT/LTX
   归档到实验存档位置，避免丢失论文所对应的镜像。
3. 删除 nRF 的旧 `build*`，从已安装 NCS 工作区激活环境，用明确 `-s` 指向新源码目录
   做一次 clean build。
4. 分别检查顶层、`zynq7020/` 和 `nrf54l15-connectkit/` 的 Git 分支、HEAD 和 dirty
   清单，与迁移前逐项一致。

## 新对话恢复模板

```text
请读取工作区 README.md、handoff.md 最后一节、bridge_contract_v0.md，
再读取当前子工程的 README/handoff。先报告三个 Git 根的 branch、HEAD、dirty，
保留所有已有修改，不 reset、不 clean、不新建重复 Vivado/CubeMX 工程。
本轮只处理：<填写任务>。所有验证必须区分离线构建、仿真、bitstream 和实板证据。
```

## 迁移验收

- 三个 `.git/` 均存在且分支、HEAD、dirty 文件数量与原位置一致；
- F730 Keil/CMake 工程能从新路径打开；
- Vivado XPR 打开后 source/constraint 不缺失，part 仍为 `xc7z020clg400-2`；
- nRF clean build 使用新源码路径且不复用旧 CMake cache；
- `protocol/golden/` 的测试继续通过；
- README、handoff 和 paper 内部相对链接可点击。
