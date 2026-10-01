# deskwong service

提供工时 `/worktime/summary`、AI 用量 `/ai/usage` 和服务配置后台 `/admin`。
RaceBox 由设备直接上传到 MQTT Broker；本服务不再订阅 RaceBox 或连接 PostgreSQL。

## 配置后台与持久化

访问 `http://服务地址:8001/admin`，用 `server.username` / `server.password` 登录。设备访问令牌不能管理配置。
首次启动将 `conf/config.yml` 导入 `/app/data/settings.sqlite`；以后以 SQLite 为准。
页面可修改 PingCode 地址/账号/密码、每日标准工时、工时 Token、MySQL 兼容数据源和管理账号密码；监听地址与设备访问 Token 在 conf/config.yml 中固定。
点击“保存配置”后只写入 SQLite，点击“重启服务”才应用；无需重建镜像或刷固件。
管理密码、PingCode 密码、Token 和 DSN 不明文回显；`******` 保留原值。工时 Token 留空则使用服务 Token。

敏感字段在写入 SQLite 前使用 AES-256-GCM 独立随机 nonce 加密。生产环境建议通过
`DESKWONG_SETTINGS_KEY` 提供 32 字节密钥的 Base64 或 64 位十六进制；未设置时服务会在
SQLite 同目录生成权限为 0600 的 `settings.sqlite.key`。备份或迁移时必须同时保留数据库与密钥，
密钥丢失后加密配置无法恢复。旧版本 SQLite 首次启动会自动把明文敏感字段迁移为密文。

SQLite 开启 WAL 和 synchronous=FULL，保存成功前完成事务提交。
Compose 使用命名卷 `deskwong-settings`，容器重建和断电重启后仍保留；不要删除此卷。
`DESKWONG_SETTINGS_DB` 可指定其他持久化路径。备份运行中的 SQLite 应使用 SQLite backup API，
或停服务后备份整个数据目录（包括尚存在的 WAL 文件）。

设备网页的 WiFi/MQTT/RaceBox 参数使用 ESP32 Flash NVS 保存，同样断电保留。
设备保存后重启设备生效；服务的 MySQL 参数则重启本服务生效。

## PingCode 工时

在 `/admin` 填写公司 PingCode HTTPS 地址、登录账号和密码。服务使用与网页相同的登录接口获取
短期访问令牌，再按月分页读取 `/api/ladon/workload-logs/mine`，按 `register_date` 汇总
`man_hour` 后输出既有 `/worktime/summary` 契约。访问令牌只保存在内存，不写入 SQLite 或日志。

## 工时 SQL 兼容源

MySQL 仅用于查询公司 PingCode 工时，与已移除的 RaceBox PostgreSQL 落库无关。
建议使用只读 MySQL 账号。填写示例：

```
pingcode_ro:密码@tcp(10.0.0.10:3306)/pingcode?parseTime=true&loc=Asia%2FShanghai&timeout=10s
```

默认 SQL 文件为 `/app/conf/worktime.sql`，已打包进镜像。网页填写 SQL 内容时优先使用网页内容。
模板支持 `{{.Year}}`、`{{.Month}}`、`{{.EmployeeNo}}`、`{{.Start}}`、`{{.End}}`。
结果须返回日期列 `date/work_date/day/workday/dt` 与工时列 `hours/work_hours/duration/worktime`，
否则取前两列。工号配置为你自己的工号，例如 `ZR16060018`。

数据源优先级为 `DESKWONG_WORKTIME_UPSTREAM`、PingCode 网页 API、MySQL。配置 PingCode 后会优先于 MySQL。

## AI 用量

`DESKWONG_AI_UPSTREAM` 可代理自建适配器。否则读取 `DESKWONG_CODEX_ACCESS_TOKEN`，
或 `DESKWONG_CODEX_AUTH_FILE` 指定的 Codex auth.json（默认 `~/.codex/auth.json`）。
直接设置 Access Token 时还应设置 `DESKWONG_CODEX_ACCOUNT_ID`。凭据不打包进镜像；
接口结果缓存 60 秒。Docker 应只读挂载整个 `.codex` 目录，不能只挂载 `auth.json`：
Codex 刷新登录时会原子替换文件，只挂载单文件会让容器继续读取旧 inode。

## 运行

```sh
docker compose -f service/docker-compose.yml up -d
```

初次部署先修改 `conf/config.yml` 的 `server.token` 和 `server.password`，不要使用示例值。
以后在 `/admin` 修改；YAML 只用于新建 SQLite 时的初始导入。
MQTT Broker、主题、用户名和密码在设备后台配置；与本服务 SQLite 无关。

| 方法 | 路径 | 鉴权 |
| --- | --- | --- |
| GET | /health | 无 |
| GET | /worktime/summary | 服务 Token 或独立工时 Token |
| GET | /ai/usage | 服务 Token |
| GET | /admin | 页面入口；读取配置需管理账号密码 |
| GET / PUT | /api/settings | 管理账号密码（HTTP Basic） |
| POST | /api/restart | 管理账号密码（HTTP Basic） |
