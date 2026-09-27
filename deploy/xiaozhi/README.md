# 小智后端部署

本目录部署 `xinnan-tech/xiaozhi-esp32-server` 完整版。宿主机端口为：WebSocket `8031`、智控台 `8032`、OTA/视觉接口 `8033`。

## 数据库和 Redis

当前上游 `web_latest` 不能直接使用 PostgreSQL：manager-api 只打包 MySQL JDBC 驱动，官方 Liquibase 变更和 SQL 也按 MySQL 验证。仅把 URL 换成 `jdbc:postgresql:` 无法工作。若必须使用 PG，需要自行维护 manager-api 分支，加入 PostgreSQL 驱动并迁移全部变更集与 MySQL 方言 SQL；这不属于部署参数调整。

本 Compose 使用宿主机上的外部 MySQL 和 Redis，不再读取 `.env`。先编辑 `docker-compose.yml` 中这些值：

- `SPRING_DATASOURCE_DRUID_URL`
- `SPRING_DATASOURCE_DRUID_USERNAME`
- `SPRING_DATASOURCE_DRUID_PASSWORD`
- `SPRING_DATA_REDIS_HOST`
- `SPRING_DATA_REDIS_PORT`
- `SPRING_DATA_REDIS_PASSWORD`
- `SPRING_DATA_REDIS_DATABASE`

Linux 上容器通过 `host.docker.internal` 访问宿主机。MySQL/Redis 必须监听 Docker 网桥可达地址，不能只监听 `127.0.0.1`；同时用防火墙限制为本机和 Docker 网段访问。

创建 MySQL 数据库和账号：

```sql
CREATE DATABASE xiaozhi_esp32_server CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'xiaozhi'@'%' IDENTIFIED BY '换成长随机密码';
GRANT ALL PRIVILEGES ON xiaozhi_esp32_server.* TO 'xiaozhi'@'%';
FLUSH PRIVILEGES;
```

## 准备和启动

```bash
mkdir -p data models/SenseVoiceSmall uploadfile
curl -fL https://raw.githubusercontent.com/xinnan-tech/xiaozhi-esp32-server/main/main/xiaozhi-server/config_from_api.yaml -o data/.config.yaml
curl -fL https://modelscope.cn/models/iic/SenseVoiceSmall/resolve/master/model.pt -o models/SenseVoiceSmall/model.pt
docker compose config --quiet
docker compose pull
docker compose up -d
docker compose ps
docker compose logs -f server console
```

浏览器打开 `http://部署机地址:8032`。第一个注册账号是管理员。在“参数字典 → 参数管理”复制 `server.secret`，然后修改 `data/.config.yaml`：

```yaml
manager-api:
  url: http://console:8002/xiaozhi
  secret: 从智控台复制的server.secret
```

执行 `docker compose restart server`。在智控台配置 LLM、ASR、TTS 的密钥并创建智能体。

## NPS/NPC 和 Caddy

推荐链路：

```text
设备 wss://voice.example.com/xiaozhi/v1/
  -> 公网 Caddy :443（TLS 与 WebSocket Upgrade）
  -> 公网 NPS TCP 端口 18031
  -> NPC 隧道
  -> 本机 127.0.0.1:8031
  -> xiaozhi-server:8000
```

NPC 只做 TCP 透传，不解析 WS；Caddy 自动处理 WebSocket Upgrade，并把公网 `https/wss` 终止为隧道内的普通 `http/ws`。将 `npc.example.conf` 放到后端所在机器并替换 NPS 地址、端口和 vkey；将 `Caddyfile.example` 放到公网 NPS/Caddy 机器，替换两个域名。

NPS 公网机只需对外开放 80/443 和 NPC 控制端口。`18031~18033` 应由防火墙限制为仅本机 Caddy 可访问。DNS A/AAAA 记录指向公网 Caddy 所在机器。最终板端填写：

```text
wss://voice.example.com/xiaozhi/v1/
```

智控台使用独立域名 `https://xiaozhi-admin.example.com`，建议再加 Caddy `basic_auth`、IP 白名单或 VPN，不要裸露注册入口。

## 分层测试

### 1. 容器和依赖

```bash
docker compose ps
docker compose logs --tail=200 console
docker compose logs --tail=200 server
docker compose exec console sh -c 'nc -zvw3 host.docker.internal 3306 && nc -zvw3 host.docker.internal 6379'
curl -i http://127.0.0.1:8032/xiaozhi/
curl -i http://127.0.0.1:8033/xiaozhi/ota/
```

验收标准：两个容器为 `Up`；console 日志出现 `Started AdminApplication`，没有 JDBC、Liquibase 或 Redis 错误；server 日志打印 WebSocket 监听地址。

### 2. WebSocket 握手

安装 `wscat` 后执行：

```bash
npx wscat -c 'ws://127.0.0.1:8031/xiaozhi/v1/?device-id=02:00:00:00:00:01&client-id=server-smoke-test'
```

连接后发送：

```json
{"type":"hello","version":1,"features":{"mcp":true},"transport":"websocket","audio_params":{"format":"opus","sample_rate":16000,"channels":1,"frame_duration":60}}
```

服务端应返回 `type=hello`、`transport=websocket` 和 `session_id`。如果开启了 WebSocket 鉴权，还需使用 `Authorization: Bearer ...`、`Protocol-Version`、`Device-Id`、`Client-Id` 请求头测试。

### 3. NPC 透传

在公网 NPS/Caddy 机器测试：

```bash
nc -zv 127.0.0.1 18031
npx wscat -c 'ws://127.0.0.1:18031/xiaozhi/v1/?device-id=02:00:00:00:00:02&client-id=npc-smoke-test'
```

能收到 hello，说明 Docker 和 NPC 正常，问题不在 Caddy/TLS。

### 4. 公网 WSS

```bash
curl -Iv https://voice.example.com/
npx wscat -c 'wss://voice.example.com/xiaozhi/v1/?device-id=02:00:00:00:00:03&client-id=wss-smoke-test'
```

再次发送 hello JSON 并收到服务端 hello，说明 DNS、证书、Caddy、NPC 和服务端整条链路可用。普通浏览器或 `curl` 不能完成 WebSocket 协议测试；首页返回 404 是本示例 Caddyfile 的预期行为。

最后使用上游仓库的 `main/xiaozhi-server/test/test_page.html` 做麦克风、ASR、LLM、TTS 完整对话测试。浏览器必须通过 HTTPS 打开测试页才能稳定取得麦克风权限，WebSocket 地址填写公网 `wss://` 地址。完整对话成功后再接开发板。

## 运维

```bash
docker compose logs --tail=200 server console
docker compose restart server
docker compose pull && docker compose up -d
```

升级前备份 `data/` 和 `uploadfile/`，并单独备份外部 MySQL。上游模板新增字段时逐项合并，不要用旧 `.config.yaml` 整体覆盖新模板。
