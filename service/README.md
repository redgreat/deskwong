# deskwong service

提供工时 `/worktime/summary`、AI 用量 `/ai/usage` 和服务配置后台 `/admin`。
RaceBox 由设备直接上传到 MQTT Broker；本服务不再订阅 RaceBox 或连接 PostgreSQL。

## 配置后台与持久化

访问 `http://服务地址:8001/admin`，用 `server.username` / `server.password` 登录。设备访问令牌不能管理配置。
首次启动将 `conf/config.yml` 导入 `/app/data/settings.sqlite`；以后以 SQLite 为准。
页面可修改管理账号密码；PingCode 域名与账号密码只在 `conf/config.yml` 的
`worktime.pingcode` 配置，不在页面编辑。监听地址与设备访问 Token 在 conf/config.yml 中固定。
点击“保存配置”后写入 SQLite 并自动重载生效，无需手动重启；修改 `conf/config.yml` 后点“重启服务”或重启容器生效。
管理密码与 Token 不明文回显；`******` 保留原值。
忘记管理密码时：停服务后删除 `settings.sqlite`（及 `settings.sqlite.key`）再启动，配置将从 `conf/config.yml` 重新导入。

敏感字段在写入 SQLite 前使用 AES-256-GCM 独立随机 nonce 加密。生产环境建议通过
`DESKWONG_SETTINGS_KEY` 提供 32 字节密钥的 Base64 或 64 位十六进制；未设置时服务会在
SQLite 同目录生成权限为 0600 的 `settings.sqlite.key`。备份或迁移时必须同时保留数据库与密钥，
密钥丢失后加密配置无法恢复。旧版本 SQLite 首次启动会自动把明文敏感字段迁移为密文。

SQLite 开启 WAL 和 synchronous=FULL，保存成功前完成事务提交。
Compose 使用命名卷 `deskwong-settings`，容器重建和断电重启后仍保留；不要删除此卷。
`DESKWONG_SETTINGS_DB` 可指定其他持久化路径。备份运行中的 SQLite 应使用 SQLite backup API，
或停服务后备份整个数据目录（包括尚存在的 WAL 文件）。

设备网页的 WiFi/MQTT/RaceBox 参数使用 ESP32 Flash NVS 保存，同样断电保留。
设备保存后重启设备生效；服务配置则重启本服务生效。

## PingCode 工时

在 `conf/config.yml` 的 `worktime.pingcode` 填写公司 PingCode 域名（base_url）、登录账号和密码
（不在 `/admin` 编辑）。服务使用与网页相同的登录接口获取
短期访问令牌，再按月分页读取 `/api/ladon/workload-logs/mine`，按 `register_date` 汇总
`man_hour` 后输出既有 `/worktime/summary` 契约。访问令牌只保存在内存，不写入 SQLite 或日志。
数据源优先级为 `DESKWONG_WORKTIME_UPSTREAM` 代理、PingCode 网页 API；每日标准工时固定 8 小时。

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

初次部署先修改 `conf/config.yml` 的 `server.token`、`server.password` 和 `worktime.pingcode`，不要使用示例值。
`server.*` 以后可在 `/admin` 修改；`worktime.pingcode` 只在 YAML 修改，改后点“重启服务”或重启容器即生效。
MQTT Broker、主题、用户名和密码在设备后台配置；与本服务 SQLite 无关。

| 方法 | 路径 | 鉴权 |
| --- | --- | --- |
| GET | /health | 无 |
| GET | /worktime/summary | 服务 Token |
| GET | /ai/usage | 服务 Token |
| GET | /admin | 页面入口；读取配置需管理账号密码 |
| GET / PUT | /api/settings | 管理账号密码（HTTP Basic） |
| POST | /api/restart | 管理账号密码（HTTP Basic） |
