# 04-RaceBox数据采集

> 模块：RaceBox 数据采集 | 状态：草案 | 最近更新：2026-09-06

## 变更记录
| 日期 | 版本 | 变更内容 | 变更人 |
| --- | --- | --- | --- |
| 2026-09-06 | v1.0 | 初版 | 项目负责人 |
| 2026-09-06 | v1.1 | D5 上传方式改为 MQTT（QoS 1），定稿 topic 与 payload 契约 | 项目负责人 |

---

## 1. 目标

把原 Python 项目 [redgreat/racewong](https://github.com/redgreat/racewong)（clone 自 andress-cz/racebox）的能力移植到本板：**按键触发 → BLE 连接 RaceBox → 下载 standalone 记录 → 解析 → MQTT 推送到队列（用户自建 Broker 接收入库）→ 擦除设备内数据**，做成一个可挂到自定义按键上的应用。

## 2. 功能需求（做什么）

### 2.1 采集流程（按键触发）
1. 用户在配置后台把某个按键（KEY/扩展按键）映射为「RaceBox 采集」。
2. 按键触发后，主屏显示采集状态（连接中 → 查询 → 下载中 → 解析 → 上传中 → 擦除 → 完成/失败）。
3. 流程步骤：
   - BLE 扫描并连接 RaceBox（支持「上次设备记忆」，见 `conf/last_device.json` 思路）
   - 查询设备状态（剩余记录数/存储占用）
   - 下发**下载命令**，逐条/分块接收记录
   - 解析 UBX/NMEA 协议数据
   - **通过 MQTT 推送到用户自建 Broker**（D5，用户负责订阅接收入库）
   - 下发**擦除命令**清理设备内数据（可配置 auto_erase，默认擦除）

### 2.2 协议参考（来自 racewong）
- 服务/特征 UUID（Nordic UART）：
  - UART 服务 `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
  - RX 特征（写）`6e400002-b5a3-f393-e0a9-e50e24dcca9e`
  - TX 特征（通知）`6e400003-b5a3-f393-e0a9-e50e24dcca9e`
  - NMEA TX `00001103-0000-1000-8000-00805f9b34fb`
- 命令（UBX 风格，`B5 62` 头 + 校验和）：
  - 状态查询 `B5 62 FF 22 00 00 21 62`
  - 下载 `B5 62 FF 23 00 00 22 65`
  - 擦除 `B5 62 FF 24 00 00 23 68`

### 2.3 数据字段（对应 `lc_racebox` 表）
`itow, year, month, day, hour, minute, second, time_accuracy, nanoseconds, fix_status, numberof_svs, longitude, latitude, wgs_altitude, msl_altitude, horizontal_accuracy, vertical_accuracy, speed, heading, speed_accuracy, heading_accuracy, pdop, gforce_x, gforce_y, gforce_z, rotation_rate_x, rotation_rate_y, rotation_rate_z`

### 2.4 MQTT 上传契约（RBX1）
- 连接参数（配置后台设置）：`Broker 地址、端口、Client ID、用户名、口令、TLS 开关、Topic 前缀`。
- 主题设计（默认前缀 `deskwong/`，可配置）：
  - 数据：`deskwong/racebox/data`（QoS 1）
  - 状态：`deskwong/racebox/status`（QoS 0）
- 数据消息使用 `RBX1` 二进制格式：固定 96 字节头部加最多 48 条原始记录，
  完整字段、CRC、幂等键和 Python 消费示例见
  [`doc/racebox-mqtt-binary.md`](../racebox-mqtt-binary.md)。旧 JSON payload 已停用。

<!-- 历史 JSON 示例，仅供字段名称参考，不再由固件发布。
```jsonc
{
  "device_id": "deskwong-01",
  "imp_stamp": "b2f1c8e0-...",          // 本次导入唯一 UUID
  "file_name": "20260906_100000",        // RaceBox 记录文件名
  "start_time": "2026-09-06T10:00:00+08:00",
  "end_time": "2026-09-06T10:05:00+08:00",
  "count": 300,
  "records": [
    {
      "itow": 123456,
      "year": 2026, "month": 9, "day": 6,
      "hour": 10, "minute": 0, "second": 1,
      "time_accuracy": 0, "nanoseconds": 0,
      "fix_status": 3, "numberof_svs": 12,
      "longitude": 116.397128, "latitude": 39.916527,
      "wgs_altitude": 50.0, "msl_altitude": 45.0,
      "horizontal_accuracy": 0.5, "vertical_accuracy": 1.0,
      "speed": 0.0, "heading": 0.0,
      "speed_accuracy": 0, "heading_accuracy": 0, "pdop": 100,
      "gforce_x": 0.0, "gforce_y": 0.0, "gforce_z": 1.0,
      "rotation_rate_x": 0.0, "rotation_rate_y": 0.0, "rotation_rate_z": 0.0
    }
  ]
}
```
-->

- 状态消息 payload（JSON）：

```jsonc
{
  "device_id": "deskwong-01",
  "event": "downloaded|uploaded|erased|failed",   // 事件类型
  "imp_stamp": "b2f1c8e0-...",
  "file_name": "20260906_100000",
  "count": 300,
  "message": "ok / 错误原因",
  "ts": "2026-09-06T10:06:00+08:00"
}
```

- QoS 与可靠性：
  - 数据消息用 **QoS 1**（至少一次送达）。
  - **本地缓冲**：Broker 不可用/断网时，消息先落 TF 卡/Flash 队列，重连后按序补发。
  - **去重**：接收端以 `imp_stamp + file_name` 幂等入库，避免重复。
- 用户侧：自建 MQTT Broker，订阅 `deskwong/racebox/data` 消费并写入数据库（表结构参照 `lc_racebox`）。

### 2.5 擦除
- 上传成功后（收到发布确认）按配置执行擦除（`auto_erase`，默认开启）；擦除失败需显式提示，不静默丢数据。
- 未成功发布的数据**不擦除**（安全原则：先确认入队，再清设备）。

## 3. 交互需求

- 采集过程中主屏展示进度与状态；失败给出原因（连接失败/下载失败/上传失败/擦除失败）。
- 支持在配置后台查看最近一次采集结果与错误日志（脱敏）。

## 4. 依赖

| 依赖 | 说明 |
| --- | --- |
| BLE（NimBLE/Bluedroid） | 板载 BLE 5 |
| MQTT Broker（D5） | 用户自建，接收入库 |
| 按键映射 | 配置后台 |
| 存储 | TF 卡（断网缓冲） |

## 5. 非功能需求

- 可靠性：任何一步失败不丢数据、不误擦除；断网有本地缓冲与补发。
- 可观测：状态机清晰，日志完整（脱敏）。
- 资源：BLE 与 WiFi 共用天线，采集期间注意射频调度（避免与天气/工时刷新冲突）。

## 6. 验收标准

1. 按键触发后能扫描并连接 RaceBox（含上次设备记忆）。
2. 能查询状态、下载记录、解析出符合 `lc_racebox` 字段的数据。
3. 数据通过 MQTT 正确推送到 Broker（QoS 1）；断网时落 TF 卡并可补发；去重生效。
4. 上传成功后按配置擦除设备数据；未成功发布不擦除。
5. 采集状态全程在主屏可见，失败有原因与日志。

## 7. 决策点状态

- D5（MQTT 上传）已定论，topic/payload 见 §2.4；用户需自建 Broker 并订阅 `deskwong/racebox/data` 入库。
