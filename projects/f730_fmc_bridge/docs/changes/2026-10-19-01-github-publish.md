# 改进记录：发布成熟 F730 与 WCH 桥接工程

- 日期：2026-10-19
- 类型：工程重构
- 关联指标：`docs/spec.md` 中的 FMC、USB 和 WCH 桥接
- 关联提交：待提交

## 1. 改动前状态

F730 成熟工程已存在于 `projects/f730_fmc_bridge/firmware/f730_bridge/`，WCH 桥接工程位于工作区实验目录，仓库缺少统一模板入口。

## 2. 改动来源与思路

保留既有源码和构建入口，只补充模板文档，并把 WCH 真实桥接工程纳入仓库。

## 3. 具体改进方法

- 新增 `projects/f730_fmc_bridge/` 文档入口。
- 纳入 `projects/wch_fsmc_bridge/firmware/wch_f730_bridge_20261018/` 的源代码、协议、工具、release 和必要证据。
- 排除 WCH build、Vivado 缓存和 CH32V307 官方全量示例库。

## 4. 涉及文件

- `projects/wch_fsmc_bridge/firmware/wch_f730_bridge_20261018/`
- `projects/f730_fmc_bridge/`
- `AGENTS.md`
- `docs/00-repository-standard.md`
- `docs/ai-workflow.md`

## 5. 验证方法

- 检查 WCH 工程构建入口和 release 镜像存在。
- 使用既有 F730/WCH 实测结果更新验证文档。

## 6. 改动后效果

| 指标 | 改动前 | 改动后 | 结论 |
|---|---|---|---|
| WCH 工程 GitHub 可复建性 | 工作区实验目录 | 仓库内独立工程 | 已整理 |
| FMC/USB 证据边界 | 分散在交接记录 | 项目验证文档 | 已整理 |

## 7. 限制与下一步

- WCH 长时间稳定性和复发故障仍需新的证据窗口。

