# 工程与协议规范

> 协议、安全、测试、构建和发布的单一事实来源。最近更新：2026-10-01。

## 1. 通用约定

- Unix 时间戳均为秒；设备默认 `Asia/Shanghai`，协议中的 UTC 字段按名称解释。
- 所有 URL 明确 scheme；敏感外部请求只允许 HTTPS，局域网设备 API 除外。
- 日志不得输出密码、Token、Cookie、精确轨迹明细；允许输出状态、数量、耗时和脱敏标识。
- 设备配置使用 NVS，服务配置使用 SQLite。敏感字段加密存储，`******` 只表示保留旧值。
- MQTT 数据使用 QoS 1；接收端必须幂等，不能以 publish 调用返回代替 PUBACK。

## 2. HTTP 契约

### 2.1 工时

`GET {base}/worktime/summary?year=2026&month=10`，请求头 `Authorization: Bearer {token}`。

```json
{"code":0,"message":"ok","data":{"year":2026,"month":10,"days":[{"date":"2026-10-01","hours":2}],"total_recorded_hours":2,"total_expected_hours":168}}
```

`days` 只需返回有记录日期；小时为浮点。错误响应使用非零 `code` 和脱敏 `message`。

### 2.2 AI 用量

`GET {base}/ai/usage`，Bearer 鉴权。设备识别 `chatgpt_5h`/300 分钟和 `chatgpt_weekly`/10080 分钟窗口，使用 `remaining_percent` 与 `resets_at`。

```json
{"code":0,"data":{"providers":[{"id":"chatgpt_5h","window_minutes":300,"remaining_percent":72,"resets_at":1789223400},{"id":"chatgpt_weekly","window_minutes":10080,"remaining_percent":44,"resets_at":1789741800}]}}
```

## 3. RaceBox BLE

- Service：`6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- RX 写：`6e400002-b5a3-f393-e0a9-e50e24dcca9e`
- TX 通知：`6e400003-b5a3-f393-e0a9-e50e24dcca9e`
- 状态：`B5 62 FF 22 00 00 21 62`
- 下载：`B5 62 FF 23 00 00 22 65`
- 擦除：`B5 62 FF 24 00 00 23 68`
- 正确下载序列：连接 → 开启 TX notify → 写下载命令 → 接收记录/`0x26`/完成 ACK → 停止 notify → 断开。

## 4. RaceBox MQTT RBX2 二进制

默认主题 `deskwong/racebox/data`，QoS 1。所有整数小端。消息长度 `136 + record_count × 80`。

| 偏移 | 长度 | 字段 | 说明 |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | ASCII `RBX2` |
| 4 | 1 | version | `2` |
| 5 | 1 | flags | bit0 同步末批；bit1 轨迹段末批 |
| 6 | 2 | header_size | `136` |
| 8 | 2 | record_size | `80` |
| 10 | 2 | record_count | 当前最大 720 |
| 12 | 4 | sync_offset | 本批在本次同步中的首序号 |
| 16 | 4 | sync_total | 最终批为实际总数 |
| 20 | 4 | sync_date | 本地日期 `YYYYMMDD` |
| 24 | 16 | sync_id | 每次同步随机 UUID 原始字节 |
| 40 | 48 | device | NUL 结尾设备名称/序列号 |
| 88 | 4 | session_index | 同步内轨迹段序号 |
| 92 | 4 | session_offset | 本批在轨迹段中的首序号 |
| 96 | 4 | session_total | 段尾前 0，段尾为准确总数 |
| 100 | 8 | session_start_utc | `YYYYMMDDhhmmss` 数值 |
| 108 | 8 | session_end_utc | 段尾前 0 |
| 116 | 4 | session_start_itow | 首条记录 iTOW |
| 120 | 4 | session_start_nano | 有符号纳秒 |
| 124 | 4 | reserved | 0 |
| 128 | 4 | payload_crc32 | records 的 IEEE CRC32 |
| 132 | 4 | header_crc32 | `[0,132)` IEEE CRC32 |
| 136 | N×80 | records | RaceBox 原始记录 |

### 4.1 80 字节记录

| 偏移 | 字段 | 单位/类型 |
| ---: | --- | --- |
| 0 | iTOW | uint32 |
| 4..10 | UTC 年月日时分秒 | 原始字段 |
| 16 | nanoseconds | int32 |
| 20 / 23 | fix / satellites | uint8 |
| 24 / 28 | longitude / latitude | 1e-7 degree |
| 32 / 36 | WGS / MSL altitude | mm |
| 40 / 44 | horizontal / vertical accuracy | mm |
| 48 | speed | mm/s；入库转换 km/h |
| 52 | heading | 1e-5 degree |
| 56 / 60 | speed / heading accuracy | 原始单位 |
| 64 | PDOP | 0.01 |
| 68/70/72 | acceleration XYZ | int16 |
| 74/76/78 | rotation XYZ | int16 |

`0x21` 和历史下载阶段的 `0x01` 作为记录；`0x26` 是轨迹段边界。批次不可跨段。稳定 `session_key = rbx2_<device>_<start_utc>_<start_itow>_<start_nano>`，`batch_key` 再追加 `session_offset`。

幂等约束：`imp_racebox(file_name)`、`imp_racebox(session_key)`、`imp_racebox_batch(batch_key)`、`lc_racebox(session_key, record_index)`。只有实际明细达到 `session_total` 才标记 complete。RBX1 仅兼容旧固件，不作为新版本验收协议。

## 5. UI 与资源

- 设备坐标系固定 400×300 横屏；`main_screen.cpp` 和 `sync_screen.cpp` 是布局事实来源。
- 字体和图标必须在目标尺寸生成；1-bit 图片每行按 `(width+7)/8` 字节打包。
- `design/final/` 中最新版设计必须由 `test/ui_preview` 编译生产代码后真实渲染，不接受另写 HTML/绘图代码近似。
- 每次 UI 改动至少生成正常月、六行月、空数据和 RaceBox 同步状态，并人工检查裁切、字形、边距、进度条与 1-bit 可读性。

## 6. 测试规范

- 根目录 `test/` 按 `service/`、`firmware/`、`ui_preview/` 分类；统一入口 `test/run.ps1`。
- 单元测试覆盖解析/聚合、鉴权/脱敏、持久化/迁移、协议边界、CRC、分页、重试和错误路径。
- 集成测试使用本地测试服务器；不得把真实凭据写入 fixture。
- 真实 PingCode 测试由 `PINGCODE_LIVE_USERNAME/PASSWORD` 临时启用，默认跳过且只读。
- 固件主机测试不能替代 IDF 构建和实机；构建不能替代刷写；刷写不能替代业务验收。

## 7. Windows 构建、刷写和发布

- ESP-IDF 使用项目锁定版本和对应独立 Python 环境；发现 pinned submodule 路径错误先修复依赖，不盲目改 CMake。
- 刷写前重新确认 COM 端口；`tools/flash.bat <COM>` 完成全部必要分区。
- 验收需看到各分区 `Hash of data verified`、从 ota_0 启动、PSRAM/WiFi/BLE 正常、无复位循环。
- 发布标签构建 `linux/amd64`、`linux/arm64`；分别查询 GHCR、Quay 清单 digest 后才算发布完成。
