# 改进记录：建立 WCH 独立工程入口

- 日期：2026-10-19
- 类型：工程重构

## 改动

- 将可发布 WCH 源码从仓库根目录 `WCH/` 整理到 `projects/wch_fsmc_bridge/firmware/`。
- 在项目根补齐模板要求的 README、project.yaml、设计、规格、上手、验证、硬件和结果入口。
- `WCH/` 仅保留未纳入发布的旧迁移、官方 EVT 和原始现场资料说明。

## 验证

- 使用仓库结构检查脚本检查项目入口。
- 重新执行 WCH out-of-tree 构建，确认工程路径变化不影响 CMake。
