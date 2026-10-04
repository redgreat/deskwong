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

## 本机 ESP-IDF 与刷机

- 本机已装 ESP-IDF v5.5.1：`C:\Espressif\frameworks\esp-idf-v5.5.1`；工具链与 Python 环境都在
  `C:\Users\wangcw\.espressif`（Python 环境 `python_env\idf5.5_py3.13_env`，xtensa 工具链 `esp-14.2.0_20241119`）。
  `idf.py` 不在 PATH 上，不要为此改全局 PATH，直接用下面的脚本（脚本内部已设好
  `IDF_PATH` / `IDF_TOOLS_PATH` / UTF-8 控制台变量并预置交叉编译器 PATH）。
- 一键构建 + 刷机（Windows）：`powershell -ExecutionPolicy Bypass -File script\flash.ps1 -Port COM7`。
  脚本依次做：npm 构建 `web/` → `idf.py build` → spiffsgen 打包网页 → 烧录固件 → 烧录 SPIFFS；
  加 `-AppOnly` 跳过 web/SPIFFS 只刷固件。不带 `-Port` 时自动取最后一个串口。
- 串口确认：`Get-PnpDevice -Class Ports -PresentOnly`。本板是 **ESP32-S3 显示屏开发板**，
  没有 CH343 这类外置 USB-UART 芯片，走的是 ESP32-S3 **原生 USB 串口（USB Serial/JTAG）**，
  端口为 **COM7**；esptool 连不上时用它确认当前实际端口号。
- 串口监视：`& C:\Users\wangcw\.espressif\python_env\idf5.5_py3.13_env\Scripts\python.exe C:\Espressif\frameworks\esp-idf-v5.5.1\tools\idf.py -p COM7 monitor`
- 旧入口 `tools\flash.bat` 需要手动进 ESP-IDF 终端环境，日常已被 `script\flash.ps1` 取代；
  刷机验收标准（分区 Hash verified、ota_0 启动、无复位循环）仍见 `doc/SPECIFICATIONS.md` §7。
- 验证刷机是否真的生效：对比 `build/deskwong.elf` 的 SHA256 与设备 0x20000 处 app_desc
  里的 `app_elf_sha256`（esptool read_flash 0x20000 512 可读）。启动日志里 `built=` 日期
  是 esp_app_desc 在增量构建下冻结的旧日期，不能作为新旧依据。
- 服务端部署：用 `script/redeploy.sh`（保留 `deskwong-settings` 卷数据）。目标机未部署过时，
  部署到上次指定的目录（脚本会优先用 `DEPLOY_DIR`，其次找脚本同级 docker-compose.yml，
  最后回落仓库 `service/`），不要另起新目录重复部署。

## 测试

- 项目自有测试、桩和 UI 真实渲染预览统一放在根目录 `test/`；第三方 managed component 自带测试除外。
- Windows 统一执行 `powershell -ExecutionPolicy Bypass -File test/run.ps1`。
- 新增组件必须补正常路径、边界/错误路径测试；真实外部服务测试默认跳过，只能临时注入凭据并保持只读。
