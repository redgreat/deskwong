# deskwong service

提供工时 `/worktime/summary`、AI 用量 `/ai/usage`、小智上下文 `/voice/context`。
RaceBox 由设备直接上传到 MQTT Broker；本服务不再订阅 RaceBox 或连接 PostgreSQL。

## 镜像与部署模式

同一份 Dockerfile 构建两个镜像，由 `DESKWONG_MODE` 决定进程暴露哪些接口：

| 模式 | 镜像 | 部署位置 | 接口面 |
| --- | --- | --- | --- |
| `full`（默认） | `ghcr.io/redgreat/deskwong` | 兼容旧单镜像部署 | 全部接口 |
| `core` | `ghcr.io/redgreat/deskwong`（compose 里设 `DESKWONG_MODE=core`） | 国内机 | 工时、黄历、小智上下文，**不含** `/ai/usage` |
| `ai` | `ghcr.io/redgreat/deskwong-ai`（镜像内已固化） | 国外 VPS | 只有 `/health` 与 `/ai/usage` |

拆分的动机：ChatGPT/Devin 等上游在国内不可直连，过去靠 `host.docker.internal` 代理绕行。
现在 AI 用量独立成 `deskwong-ai` 镜像放到国外 VPS，国内机不再挂载 Codex 凭据也不需要代理。
`ai` 模式不打开 SQLite（上下文接口不注册），只需 `conf/config.yml` 与一个数据卷保存
轮换后的 refresh token；`core` 模式不校验 `ai_usage` 配置段。

- 国内机：`service/docker-compose.yml`（core 模式，配置样例 `conf/config.yml.sample`）。
- 国外 VPS：把 `service/docker-compose.ai.yml` 重命名为 `docker-compose.yml`（或 `docker compose -f` 指定），
  配置样例 `conf/config.ai.yml.sample`；VPS 直连上游，不需要代理环境变量。
- 设备对接：开发板后台的「AI 用量 API 地址/密钥」直接指向 VPS（如 `https://ai.example.com`），
  工时/黄历/上下文仍指向国内机；两边的 `server.token` 互相独立。
- `docker-compose.client-login.example.yml` 只在 AI 容器需要桌面客户端登录态时用于
  VPS 侧 override。

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
  **「组织 ID + Bearer Token」必须是浏览器会话凭据，不是 Devin 设置页里的服务 API Key（`apk_…`）**，获取方法见下面 [Devin 凭据获取](#devin-凭据获取)。
- MiMo Desktop：Linux Docker 推荐 `oauth_refresh`。服务用 refresh token 获取短期 access token 并读取 Desktop 官方 `/user/usage`；上游轮换 refresh token 时立即写入数据卷 JSON，容器重启后继续使用。也可用 `client_login` 显式挂载 `DESKWONG_MIMO_AUTH_FILE` 读取客户端登录态。
- Claude Code：当前保留外部 API 接口（`external_api` + `api_base`），待有账号后验证。

凭据不打包进镜像，也不写入日志。客户端登录态目录仅以只读方式挂载。

### Devin 凭据获取

自助套餐（Free/Pro/Max/Teams）的日/周额度**只有 `app.devin.ai` 的内部接口
`GET https://app.devin.ai/api/<内部组织 ID>/billing/quota/usage` 会返回**；官方
`api.devin.ai/v3` 只有企业版的 ACU consumption 分析接口，没有日/周剩余百分比。
而这个内部接口只认**浏览器会话 token**，Devin 设置页 → Devin API → API Keys 里生成的
`apk_…` 服务 Key 对它无效（会返回 `401 {"detail":"Unauthenticated"}`）。开源项目
[CodexBar](https://github.com/steipete/CodexBar/blob/main/docs/devin.md) 也是这么取的。

取一次会话凭据（浏览器里操作，约 1 分钟）：

1. 用 Chrome / Edge 等 Chromium 浏览器登录 <https://app.devin.ai>，选中你的组织。
2. 打开该组织的 **Usage & Limits** 页面（首次进一次，让组织信息落到 localStorage）。
3. F12 → Network，筛选 `billing/quota/usage`，刷新页面后点一条状态 **200** 的请求。
4. 在 Request Headers 里复制两个值：
   - `Authorization`（含 `Bearer ` 前缀）→ 填到 `api_token`
   - `x-cog-org-id`（形如 `org_GQ6LhcfkW1TSinM6` 或 `org-…`）→ 填到 `username`
5. 写进 `conf/config.yml` 后 `./script/redeploy.sh` 重启容器（conf 是只读挂载）。

注意：这是**会话 token，会过期**，过期后接口回到 401，按上面步骤重新取一次即可；
服务日志里会带上上游返回的原文方便定位。`username` 不要填组织 slug（URL 里那种
`wangcw-xxxx`），必须填 `x-cog-org-id` 的内部 ID。

AI 侧 Compose 只保留 ChatGPT/Codex 登录态挂载（宿主目录可用 `DESKWONG_CODEX_AUTH_DIR` 覆盖），不会要求 VPS 存在 Devin 或 MiMo Desktop 目录。确需读取桌面客户端登录态时，复制 `docker-compose.client-login.example.yml` 为 VPS 上的 override，填写其中三个宿主目录环境变量后再启动；不要提交带真实路径或凭据的 override。

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
# 国内机（core 模式：工时/黄历/小智上下文）
docker compose -f service/docker-compose.yml up -d

# 国外 VPS（ai 模式：仅 /ai/usage；也可先把文件重命名为 docker-compose.yml 再 up）
docker compose -f service/docker-compose.ai.yml up -d
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
