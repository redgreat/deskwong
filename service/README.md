# deskwong service

提供工时 `/worktime/summary`、AI 用量 `/ai/usage`、小智上下文 `/voice/context` 和服务配置后台 `/admin`。
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

后台可从四个应用选择一至两个，并为每个应用设置独立轮询周期；未选应用不会请求上游。

- ChatGPT/Codex：读取 `DESKWONG_CODEX_AUTH_FILE`（默认 `~/.codex/auth.json`）。Docker 应只读挂载整个 `.codex` 目录，因为 Codex 刷新登录会原子替换文件。
- Devin：Linux Docker 推荐在后台选择“组织 ID + Bearer Token”；也可显式挂载客户端目录，优先读取 `DESKWONG_DEVIN_CREDENTIALS_FILE` 中的 `windsurf_api_key`，没有该文件时读取 `DESKWONG_DEVIN_STATE_DB` 的 `windsurfAuthStatus`，通过 Connect RPC `GetUserStatus` 取得日/周剩余额度。
- MiMo Desktop：Linux Docker 推荐在后台选择“OAuth Refresh Token（Linux）”。服务用 refresh token 获取短期 access token并读取 Desktop 官方 `/user/usage`；上游轮换 refresh token 时立即以 AES-256-GCM 加密写回 SQLite，容器重启后继续使用。也可显式挂载 `DESKWONG_MIMO_AUTH_FILE` 读取客户端登录态。
- Claude Code：当前保留外部 API 接口，待有账号后验证。

凭据不打包进镜像，也不写入日志。后台填写的 Token 使用 SQLite 加密字段保存；客户端登录态目录仅以只读方式挂载。

基础 Compose 只保留 ChatGPT/Codex 登录态挂载（宿主目录可用 `DESKWONG_CODEX_AUTH_DIR` 覆盖），不会要求 Linux 主机存在 Devin 或 MiMo Desktop 目录。确需读取桌面客户端登录态时，复制 `docker-compose.client-login.example.yml` 为本机 override，填写其中三个宿主目录环境变量后再启动；不要提交带真实路径或凭据的 override。

## 小智上下文

开发板使用与工时服务相同的根地址，把当前缓存摘要上报到
`POST /voice/context/report`；服务按 `device-id` 只保存每台设备的最新快照。
小智上下文源配置为 `GET https://服务域名/voice/context`，请求头增加
`Authorization: Bearer <context.read_token>`；小智会自动补充 `device-id`。
在智控台对应角色的提示词中保留 `{{ dynamic_context }}`。

推荐角色提示词：

```text
[角色设定]
你是{{assistant_name}}，一名专业、可靠的桌面业务数据专家。
你擅长分析工时、AI用量、天气黄历和RaceBox同步数据，并用简洁中文回答。

[回答原则]
- 先给结论，再给关键数字和必要说明。
- 优先使用以下开发板上下文：
{{ dynamic_context }}
- 回答数据问题时说明数据时间；缓存数据必须明确更新时间。
- 上下文没有的数据就说暂时没有，不猜测、不编造。
- 声纹只用于称呼和个性化；未识别时按匿名用户正常回答。
```

`context.report_token` 与 `context.read_token` 可在 YAML 或 `/admin` 配置；为空时为兼容旧配置临时回落到 `server.token`。生产环境应设置两个不同的随机 Token。接口不临时请求 PingCode、天气或数据库，因此唤醒时只读取 SQLite 快照。

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
| POST | /voice/context/report | 上报 Token + device-id |
| GET | /voice/context | 上下文读取 Token + device-id |
| GET | /admin | 页面入口；读取配置需管理账号密码 |
| GET / PUT | /api/settings | 管理账号密码（HTTP Basic） |
| POST | /api/restart | 管理账号密码（HTTP Basic） |
