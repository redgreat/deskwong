# deskwong Codex 工作规则

项目详细规则从 `doc/AI_RULES.md` 开始；需求、架构、协议规范和实施状态分别读取
`doc/REQUIREMENTS.md`、`doc/ARCHITECTURE.md`、`doc/SPECIFICATIONS.md`、`doc/PLAN.md`。

## PostgreSQL MCP

- 本项目数据库 MCP 的 Codex 配置名为 `deskwong_postgres`，连接 eadm PostgreSQL。
- MCP 提供 `list_tables`、`describe_table`、`query` 和 `execute_sql`。新会话应优先使用这些工具检查表结构和执行迁移，不再把数据库密码写入命令、仓库或聊天回复。
- 当前会话若看不到这些工具，需要新建或重启 Codex 会话以重新加载用户级 MCP 配置；不要因此重复安装。
- 查询前先用 `describe_table` 核对真实列和索引。写入或 DDL 前必须明确目标、检查影响范围，并尽量在单一事务中执行；清理历史数据前先建立可恢复备份。
- RaceBox 幂等约束为 `imp_racebox(file_name)`、`imp_racebox(session_key)`、`imp_racebox_batch(batch_key)`、`lc_racebox(session_key, record_index)`。
- 数据库迁移源文件位于 `D:\wangcw\Documents\github\agentwong\scripts\migrations\versions`。执行后必须重新查询列类型、索引或约束，不能只以脚本无报错作为成功依据。

## RaceBox 刷机

Windows 构建、COM 端口确认、`tools\flash.bat` 用法和刷机验收标准见 `doc/SPECIFICATIONS.md` §7。

## 测试

- 项目自有测试、桩和 UI 真实渲染预览统一放在根目录 `test/`；第三方 managed component 自带测试除外。
- Windows 统一执行 `powershell -ExecutionPolicy Bypass -File test/run.ps1`。
- 新增组件必须补正常路径、边界/错误路径测试；真实外部服务测试默认跳过，只能临时注入凭据并保持只读。
