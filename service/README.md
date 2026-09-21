# deskwong service

Go 服务，只做两件事：

1. **MQTT 消费端** —— 订阅设备上传的 RaceBox 数据，批量落库到 PostgreSQL 表 `racebox_messages`（自动建表）。
2. **设备后台网页配置页的两个上游接口** —— `GET /worktime/summary`、`GET /ai/usage`。

设备侧的所有业务配置（天气、工时 API、AI 用量 API、MQTT 上传、RaceBox 蓝牙设备）都在**设备后台网页配置页**维护，不再写进 `conf/config.yml`。

## conf/config.yml 只保留三项

| 段 | 键 | 说明 |
| --- | --- | --- |
| `server` | `listen` | 监听地址，如 `:8001` |
| `server` | `token` / `tokens` | 设备网页配置页里「工时获取 Token」「AI 用量 Token」填的值，Bearer 鉴权 |
| `server` | `allow_cidrs` | 可选，只允许内网网段访问 |
| `database` | `dsn` / `max_conns` / `min_conns` / `retention_days` | 仅供 MQTT 消费端落库，含历史保留天数 |
| `mqtt` | `enabled` `broker` `tls` `username` `password` `client_id` `topic` `qos` `queue_size` `batch_size` `flush_interval_ms` | 消费端订阅与落库参数 |
| `log` | `level` / `format` | 日志 |

## 工时：直连公司 MySQL（PingCode）

配置在 `conf/config.yml` 的 `worktime` 段：

```yaml
worktime:
  expected_daily_hours: 8
  mysql:
    dsn: "user:pass@tcp(host:3306)/pingcode?parseTime=true&loc=Asia%2FShanghai"
    employee_no: "你的工号"
    query_file: "/app/conf/worktime.sql"
    query: ""          # 非空时优先于 query_file，可直接把 SQL 写在这里
    timeout_sec: 10
```

SQL 写在 `conf/worktime.sql`（已随镜像打包，随时可替换），支持占位符：

- `{{.Year}}` `{{.Month}}`
- `{{.EmployeeNo}}` —— 工号，用来区分每个人的工时
- `{{.Start}}` `{{.End}}` —— 当月首末日，`2006-01-02` 格式

结果列要求：**日期列** `date`/`work_date`/`day`/`workday`/`dt`，**工时列** `hours`/`work_hours`/`duration`/`worktime`；
识别不到就取第 1、2 列。做完替换后重启容器即可，不需要改 Go 代码。

优先级：`DESKWONG_WORKTIME_UPSTREAM`（代理）> `worktime.mysql`（直连）。

## 工时 / AI 用量的数据来源（环境变量，不写进 yml）

| 环境变量 | 作用 |
| --- | --- |
| `DESKWONG_WORKTIME_UPSTREAM` | 工时代理上游，如 PingCode 适配器。未设置时 `/worktime/summary` 返回 501，请直接在设备网页配置页填工时 API |
| `DESKWONG_AI_UPSTREAM` | AI 用量的自建适配器；设置后 `/ai/usage` 直接代理它 |
| `DESKWONG_CODEX_ACCESS_TOKEN` | Codex OAuth access token |
| `DESKWONG_CODEX_AUTH_FILE` | `auth.json` 路径，默认 `~/.codex/auth.json` |

未设置 `DESKWONG_AI_UPSTREAM` 时，服务会用 Codex 凭据直接请求
`GET https://chatgpt.com/backend-api/wham/usage`，把
`rate_limit.primary_window`（5 小时）与 `secondary_window`（周）归一化成设备期望的结构：

```json
{"code":0,"data":{"updated_at":1789223400,"plan_type":"pro","providers":[
  {"id":"chatgpt_5h","name":"ChatGPT/Codex","window_minutes":300,"remaining_percent":72,"resets_at":1789223400,"status":"normal"},
  {"id":"chatgpt_weekly","name":"ChatGPT/Codex","window_minutes":10080,"remaining_percent":44,"resets_at":1789741800,"status":"normal"}
]}}
```

结果缓存 60 秒，避免设备高频刷新触发风控。凭据过期时返回 503 并提示重新 `codex login`。

### 要让 Codex 用量跑起来，你需要准备

1. 一台跑过 `codex login` 的机器，会生成 `~/.codex/auth.json`（内含 `tokens.access_token`）。
2. 把这个文件挂进容器（**推荐**，服务每次请求都重新读文件，重新登录后自动生效）：

   ```yaml
   volumes:
     - ~/.codex/auth.json:/root/.codex/auth.json:ro
   ```

   不方便挂载时改用环境变量：`DESKWONG_CODEX_ACCESS_TOKEN`（token 过期后需手动更新）
   或 `DESKWONG_CODEX_AUTH_FILE=/path/auth.json`。
3. 板子后台网页配置页里只填：`AI 用量 API 地址 = http://<服务地址>:8001`，`Token = conf/config.yml 里的 server.token`。
4. 访问 ChatGPT 需要能出公网；容器若在内网，需要走代理或把该请求放到能出网的机器上。

> 注意：`access_token` 有有效期，过期后接口会返回 503。最省事的做法是宿主机上放个定时任务定期 `codex login`，
> 因为挂载的是文件，服务下一次请求就会读到新 token。

## 接口

| 方法 | 路径 | 鉴权 |
| --- | --- | --- |
| GET | `/health` | 否 |
| GET | `/worktime/summary?year=&month=` | Bearer |
| GET | `/ai/usage` | Bearer |

## MQTT：EMQX + Caddy 反向代理

两端角色不同，要求也不同：

| 端 | 连接 | TLS |
| --- | --- | --- |
| 本服务（消费端） | 内网直连 EMQX 1883 | 不需要（内网可信） |
| 设备（上传端） | 可能要走公网 | 需要 |

### 能不能用 Caddy 代理？证书能自动申请吗？

- **Caddy 的 `reverse_proxy` 是 HTTP 层，不能直接代理裸 TCP 的 MQTT 1883。**
- **证书自动申请**：Caddy 默认自动申请 Let's Encrypt，但前提是 **80/443 可达且域名解析到 Caddy 所在机器**（HTTP-01 / TLS-ALPN 挑战）。如果 Caddy 在内网、只靠穿透把端口转出来，80/443 验证做不了 → 要走 **DNS-01**（Caddy 需带 DNS 插件，如 `caddy-dns/cloudflare`）才能全自动。

### 推荐方案：EMQX 开 WebSocket，Caddy 反代

固件里 `CONFIG_MQTT_TRANSPORT_WEBSOCKET=y` 已经打开，esp-mqtt 支持 `ws/wss`。
后台网页的「Broker 地址」直接填完整 URI 即可（代码里含 `://` 就原样使用）：

```
wss://mqtt.example.com/mqtt
```

EMQX 侧（`emqx.conf`，或用 Dashboard 添加监听器）：

```hocon
listeners.ws.default {
  bind = 0.0.0.0:8083
}
# wss 由 Caddy 终止 TLS，EMQX 只需 ws
```

Caddy（在**有公网 80/443** 的机器上）：

```caddyfile
mqtt.example.com {
    reverse_proxy /mqtt* 10.0.0.20:8083
}
```

证书全自动签发与续期，无需额外配置。

### 备选方案

| 方案 | 做法 | 适用 |
| --- | --- | --- |
| caddy-l4 | 自编译带 `caddy-l4` 插件的 Caddy，TLS 终止后转发 TCP 1883 | 设备只能用裸 MQTT |
| frp 直转 1883 | 内网穿透转发 TCP，TLS 由 EMQX 自签证书提供，设备侧信任该 CA | 没有域名/公网机 |
| EMQX 自带 TLS | 在 EMQX 上配证书（自签或 LE），设备 `mqtts://host:8883` | 有公网 IP 且能开 8883 |

> 设备侧 URI 示例：裸 MQTT `mqtt://host:1883`、TLS `mqtts://host:8883`、
> WebSocket `ws://host/mqtt`、WebSocket+TLS `wss://host/mqtt`。前两种填 Broker + 端口，
> 后两种把完整 URI 填在「Broker 地址」里，端口字段留 0 或忽略即可。

## 运行

```bash
docker compose -f service/docker-compose.yml up -d
```

Tag 推送 `v*` 会触发 `.github/workflows/release.yml`：跑 Go 测试、构建多架构镜像推送 GHCR 与阿里云 ACR、创建 GitHub Release。首次发布前需配置仓库 secrets：

- `ALIYUN_REGISTRY`
- `ALIYUN_NAMESPACE`
- `ALIYUN_USERNAME`
- `ALIYUN_PASSWORD`
- `DOCKER_IMAGE_NAME`
