# deskwong service

提供工时 `/worktime/summary`、AI 用量 `/ai/usage`、小智上下文 `/voice/context`。
RaceBox 由设备直接上传到 MQTT Broker；本服务不再订阅 RaceBox 或连接 PostgreSQL。

## 配置方式

**没有 Web 管理后台**（原 `/admin` 与 `/api/settings` 已彻底删除）。所有配置只能在
`conf/config.yml`（或 docker-compose 环境变量）里修改，改完重启容器生效：

```sh
docker compose -f service/docker-compose.yml restart deskwong-service
```

- `server.token`：设备调用 `/worktime/summary`、`/ai/usage` 的固定 Bearer Token，设备后台填同一个值。
- `context.report_token` / `context.read_token`：小智上下文上报/读取 Token；留空时回落到 `server.token`，生产环境务必设置两个不同的随机值。
- `worktime.pingcode`：公司工时数据源。
- `ai_usage.providers`：四个 AI 应用的开关、顺序与认证方式；服务端不轮询上游，设备每次请求 `GET /ai/usage` 时才实时查询。

配置样例见 `conf/config.yml.sample`。

## 持久化

只有一个 SQLite：`voice_context_snapshots`（按 `device-id` 保存每台设备最新小智上下文快照）。
路径由 `DESKWONG_DB` 指定，默认 `/app/data/deskwong.sqlite`，权限 0600，开启 WAL。

MiMo 使用 `oauth_refresh` 时，上游轮换下来的 refresh token 不能写回只读的 `conf/config.yml`，
而是写入数据卷的 `/app/data/ai_refresh_token.json`（可用
`DESKWONG_REFRESH_TOKEN_FILE` 覆盖），容器重启后自动加载覆盖 conf 里的初始值。

Compose 使用命名卷 `deskwong-settings`，容器重建和断电重启后仍保留；不要删除此卷。
设备网页的 WiFi/MQTT/AI 应用选择等参数使用 ESP32 Flash NVS 保存，同样断电保留。

## PingCode 工时

在 `conf/config.yml` 的 `worktime.pingcode` 填写公司 PingCode 域名（base_url）、登录账号和密码。
服务使用与网页相同的登录接口获取短期访问令牌，再按月分页读取
`/api/ladon/workload-logs/mine`，按 `register_date` 汇总 `man_hour` 后输出既有
`/worktime/summary` 契约。访问令牌只保存在内存，不写入 SQLite 或日志。
数据源优先级为 `DESKWONG_WORKTIME_UPSTREAM` 代理、PingCode 网页 API；每日标准工时固定 8 小时。

## AI 用量

`/ai/usage` 返回所有 `enabled: true` 的应用（最多四个），每个应用含多个额度窗口
（`windows[]`，如 5 小时 / 每周）。**选哪两个显示由开发板网页后台决定**，服务端不做"四选二"限制。

- ChatGPT/Codex：读取 `DESKWONG_CODEX_AUTH_FILE`（默认 `~/.codex/auth.json`）。Docker 应只读挂载整个 `.codex` 目录，因为 Codex 刷新登录会原子替换文件。
- Devin：Linux Docker 推荐 `manual_token`（组织 ID + Bearer Token）；也可用 `client_login` 挂载客户端目录，优先读取 `DESKWONG_DEVIN_CREDENTIALS_FILE` 中的 `windsurf_api_key`，没有该文件时读取 `DESKWONG_DEVIN_STATE_DB` 的 `windsurfAuthStatus`，通过 Connect RPC `GetUserStatus` 取得日/周剩余额度。
- MiMo Desktop：Linux Docker 推荐 `oauth_refresh`。服务用 refresh token 获取短期 access token 并读取 Desktop 官方 `/user/usage`；上游轮换 refresh token 时立即写入数据卷 JSON，容器重启后继续使用。也可用 `client_login` 显式挂载 `DESKWONG_MIMO_AUTH_FILE` 读取客户端登录态。
- Claude Code：当前保留外部 API 接口（`external_api` + `api_base`），待有账号后验证。

凭据不打包进镜像，也不写入日志。客户端登录态目录仅以只读方式挂载。

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

## 运行

```sh
docker compose -f service/docker-compose.yml up -d
```

初次部署先修改 `conf/config.yml` 的 `server.token` 和 `worktime.pingcode`，不要使用示例值。
MQTT Broker、主题、用户名和密码在设备后台配置；与本服务无关。

| 方法 | 路径 | 鉴权 |
| --- | --- | --- |
| GET | /health | 无 |
| GET | /worktime/summary | 服务 Token |
| GET | /ai/usage | 服务 Token |
| POST | /voice/context/report | 上报 Token + device-id |
| GET | /voice/context | 上下文读取 Token + device-id |
