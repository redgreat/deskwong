# 工程与协议规范

> 协议、安全、测试、构建和发布的单一事实来源。最近更新：2026-10-12。

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
{"code":0,"message":"ok","data":{"year":2026,"month":10,"days":[{"date":"2026-10-01","hours":2}],"total_recorded_hours":2,"total_expected_hours":144}}
```

`days` 只需返回有记录日期；小时为浮点。`total_expected_hours` 按中国大陆法定工作日（排除自然周末与法定节假日，计入调休上班日）× 每日标准工时（`expected_daily_hours`，默认 8）计算；设备端用同一份节假日表在服务端缺失时兜底，主屏固定显示 `已记录/应记录 百分比`，没有记录即 `0%`，不出现 `--`。主屏“应记录”与百分比分母按本月已过工作日折算：设备端统计请求月 1 日至今（含今天，由本地 RTC/NTP 日期决定）的法定工作日 × 每日标准工时；仅当展示月不是当前月（日历翻页浏览历史月份）或电量刚点亮尚无当天日期时，才退回服务端/兜底的整月口径。小智上下文快照的 `month_expected_hours` 必须与主屏同口径，避免语音播报与屏幕数字不一致。错误响应使用非零 `code` 和脱敏 `message`。

### 2.2 AI 用量

`GET {base}/ai/usage`，Bearer 鉴权。设备识别 `chatgpt_5h`/300 分钟和 `chatgpt_weekly`/10080 分钟窗口，使用 `remaining_percent` 与 `resets_at`。

```json
{"code":0,"data":{"providers":[{"id":"chatgpt_5h","window_minutes":300,"remaining_percent":72,"resets_at":1789223400},{"id":"chatgpt_weekly","window_minutes":10080,"remaining_percent":44,"resets_at":1789741800}]}}
```

### 2.3 黄历

设备请求后台配置的黄历地址，并追加 `date=YYYY-MM-DD&location={weather_location}&key={almanac_key}`。响应至少包含以下一种等价结构；`yi`/`ji` 可为字符串或字符串数组，兼容字段名 `suit`/`avoid`：

```json
{"code":0,"data":{"yi":["出行","会友","学习"],"ji":["动土","搬家"]}}
```

请求失败不得清空上次成功的宜忌。密钥不得写入日志，配置接口只能返回 `******`。

### 2.4 小智动态上下文

开发板向 deskwong 服务上报聚合快照：

`POST {base}/voice/context/report`，请求头包含 `Authorization: Bearer {device-token}`，请求体中的 `device_id` 必须与 Token 授权设备一致。上报为幂等覆盖，不保存历史轨迹或声纹特征。

```json
{"device_id":"94:a9:90:dd:12:38","observed_at":1791259200,"worktime":{"today_hours":2,"month_recorded_hours":42,"month_expected_hours":144},"ai_usage":[{"name":"ChatGPT 5小时","remaining_percent":72,"resets_at":1791266400}],"weather":{"text":"晴","temperature_c":23,"humidity_percent":40,"almanac_yi":"出行、学习","almanac_ji":"动土"},"racebox":{"date":"2026-10-06","synced_points":750,"sync_complete":true}}
```

小智服务读取快照：

`GET {base}/voice/context`，请求头必须包含小智自动传入的 `device-id`，并使用上下文专用 `Authorization: Bearer {context-token}`。成功响应必须符合官方上下文源 `{code,data}` 契约；`data` 使用简短中文键值，便于直接注入系统提示词，例如：

```json
{"code":0,"msg":"success","data":{"工时":"今天 2 小时，本月 42/144 小时","AI用量":"ChatGPT 5小时剩余72%，18:00重置","天气":"晴，23℃，湿度40%","今日黄历":"宜：出行、学习；忌：动土","RaceBox":"今天已同步750条，已完成","数据时间":"2026-10-06 10:40:00"}}
```

- GET 不得现场请求任何上游服务；目标服务端处理时间不超过 200ms，单次 JSON 响应建议不超过 2KB。
- 未知设备、鉴权失败分别返回 HTTP 404/401；没有任何快照时返回非零 `code`。单模块缺失不使整个响应失败。
- 服务端按 `observed_at` 判断新鲜度：默认超过 15 分钟标注更新时间，超过 24 小时省略易误导的天气、AI 和 RaceBox 状态；工时月汇总可保留但必须标注日期。
- 日志只记录脱敏设备标识、状态、耗时和快照年龄，不记录 Token、声纹信息、工时日明细或轨迹内容。

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

- Web 登录组件初始化时用户名和密码状态都为空；HTML 中不得设置 `admin` 等账号 `value`，登录失败后保留用户本次输入以便纠错，但页面刷新、退出登录或 token 失效重新进入登录页时不得由应用代码填入默认用户名。保留标准 `autocomplete="username"` / `autocomplete="current-password"`，允许浏览器密码管理器按用户选择填充真实账号。
- 设备坐标系固定 400×300 横屏；`main_screen.cpp` 和 `sync_screen.cpp` 是布局事实来源。
- 字体和图标必须在目标尺寸生成；1-bit 图片每行按 `(width+7)/8` 字节打包。
- `design/final/` 中最新版设计必须由 `test/ui_preview` 编译生产代码后真实渲染，不接受另写 HTML/绘图代码近似。
- 每次 UI 改动至少生成正常月、六行月、空数据和 RaceBox 同步状态，并人工检查裁切、字形、边距、进度条与 1-bit 可读性。
- 主屏顶部状态栏的有效高度为 `y=0..31`、视觉中心线为 `y=16`。电量/WiFi、小智连接状态、分隔线、RaceBox 图标及完成标记、轨迹点胶囊、日历翻页图标、离线 IP 胶囊和右侧时间，都应以实际可见字形或图形边界为准，使垂直中心落在 `y=16±1px`；不能只按字体行框机械居中。企微提醒铃不再创建常驻对象。小智状态使用15×15自绘1-bit机器人图标，成功为透明/白底黑色墨迹，失败或等待重连时保持相同位图并切换成黑底白色墨迹，不得使用字体音乐符号或警告三角；电量/网络文本标签宽度必须自适应，不得锁死 90px（锁死时 100% 电量的 WiFi 墨迹会伸进预留宽度里，图标按标签框定位就会被顶住）；语音图标紧跟在电量标签实际宽度右缘之后固定 6px（渲染出的字形墨迹间隙为 8px，标签框右缘比墨迹右缘多约 2px），间距不随电量位数变化，9% 与 100% 都保持同一观感，并保底不越过 x=116 分隔线；不要改成"在文字右缘与分隔线之间居中"，那样低电量时图标会飘到两段空白中间，看起来另起一列；纵向位图墨迹 14 行，顶部取 y=10 使墨迹中心落在 y=16.5，与 WiFi 符号和百分比数字同一条视觉中心线，不得沿用 y=9（整体高 1px，看起来和网络符号不在同一行）。语音关闭时隐藏，不灰显也不加斜杠标记，不得推动RaceBox、轨迹点或时间的位置。顶栏排版用 `test/ui_preview/status_bar_check.py` 量测真实渲染像素：相邻元素最小水平间隙 ≥4px，各元素墨迹纵向中心在 y=16±2 内。28px时间需针对字形上偏补偿下移，顶部至少保留2px可见留白，底部不得碰到y=32分隔线。正常、空数据、RaceBox同步、日历翻页、离线/AP及语音成功/失败状态均需用生产LVGL预览检查。
- 主屏月工时百分比胶囊高 20px，放在顶部横线下沿 `y=34` 与月总进度条上沿 `y=64` 之间的 30px 标题区域内垂直居中，即胶囊顶部 `y=39`；胶囊上下各保留 5px，不能与下方进度条相贴。月工时数字行与胶囊的视觉基线需协调，并在正常月、六行月和空数据预览中核对。
- 天气黄历弹窗为 400×300 主屏上的 380×264 覆盖卡片：第一排 7 列显示今天起连续 7 天，第二排 7 列显示当前起连续 7 个小时，第三排显示详细宜忌。天气单元不画边框，以近正方形留白组织“图标与阴晴 / 温度 / 日期或小时”三层；日期天气、小时天气和黄历之间只用轻量虚线分隔，右下角不显示辅助小字。天气温度使用 `℃`（U+2103），日预报为 `最高℃/最低℃`，小时预报为 `温度℃`。温度与日期/小时必须作为独立文本行绘制，每列分别取得文本实际像素宽度后以 `列左边界 + (52 - 文本宽度) / 2` 定位；不得把两行合并进同一标签后仅设置 `LV_TEXT_ALIGN_CENTER`。允许每个文本行共用一个轻量绘制对象，以满足 128KB LVGL 对象池安全余量。验收时同时检查生产 LVGL 预览和开发板实屏，最终以实屏位置为准。
- 按键优先级：RaceBox 运行态高于普通页面；同步中 `KEY` 短按切换同步层、长按取消。空闲态长按 `KEY` 进入/退出日历，日历内 `KEY`/`BOOT` 短按上月/下月，30 秒无操作退出；普通主屏 `BOOT` 短按开关天气黄历、长按约 3 秒开配网热点。`BOOT + PWR` 上电进入 ROM 下载模式必须保留。
- 语音 UI 事件采用可重放状态，不得只在网络回调中单次尝试获取 LVGL 锁；UI 任务须在锁可用时补绘最新状态、问题和回答。唤醒命中到首次显示“连接中”目标不超过 1 秒。问题与回答正文使用独立 GB2312 14px 字体并保持两行省略。
- WebSocket 所有文本与二进制发送必须经同一发送互斥或单一发送队列串行化；该发送同步不得复用会阻塞 stop/teardown/UI 的生命周期互斥锁。发送失败需保留错误原因并安全清理会话，不能让 `tools/list` 等 MCP 回复与 Opus 上行并发破坏连接。
- 语音连接生命周期与一次对话解耦：联网后由后台任务完成 OTA 预取、WSS hello 和断线退避重连；空闲预连接不得读取或上传麦克风数据，只有唤醒/调试 start 后才发送 listen 并启动 Opus。OTA 地址/token 只存内存并带有效期，鉴权失败时清缓存重取；WSS 空闲保活失败不得阻塞 UI/WakeNet。实机验收分别记录冷连接和已预连接热唤醒耗时。
- 服务端二进制 Opus 帧不得在 WebSocket 事件任务中同步解码或播放；事件任务只做有界复制入队，独立大栈播放任务完成解码、重采样和 I2S 写入。显示刷新单次 DMA 像素分块不超过 1KB，避免常驻 WSS 与 TLS 并发时因内部 RAM 碎片造成弹窗刷新失败。
- 语音开始和每次 TTS stop 后的续听消息使用 `{"type":"listen","state":"start","mode":"auto"}`。`manual` 模式只有在实现按键按住说话或本地端点检测并发送 `listen stop` 时才可使用。实机验收必须看到每轮上行后返回 STT、TTS，不能只以 Opus 帧发送成功判定追问链路通过。
- 语音服务记录最近一次 start/STT/TTS 活动时刻；处于 listening 且 30 秒无下行活动时，由非麦克风任务执行 stop/teardown 并恢复 WakeNet。后台预连接以有效 IPv4 为前提，失败退避序列至少为 10/30/60/300 秒，成功 hello 后复位退避；日志记录阶段与错误但不得打印 token。
- 语音问题/回答标签更新文本后，按 LVGL 实际内容高度重新计算 Y：在各自 30px 正文区域内将单行或两行文本整体垂直居中；内容高度超过区域时保持两行裁切规则，不允许覆盖分隔线或标题栏。
- WakeNet 初始化后记录实际检测模式和阈值；后台调整范围必须限制在 ESP-SR 支持区间，修改后无需保存原始音频。唤醒候选命中后进入至少3秒 refractory/cooldown，避免同一段电视或尾音重复触发；统计候选命中、冷却丢弃和无语音超时次数。
- `voice_wake_threshold` 按百分数直接映射为 WakeNet `set_det_threshold(value / 100.0, 1)`，合法范围 `40–99`，低值更灵敏。监听任务检测配置值变化后在同一任务内安全热更新，网页保存无需重启；日志必须输出配置值、`set_det_threshold` 返回值和回读实际值。实机调优至少对 80/70/60 三档各做近讲与正常使用距离测试，记录命中/漏检/误唤醒，不以单次喊话判断。
- 免打扰时段由 `voice_quiet_hh_start` / `voice_quiet_hh_end` 决定（0–23，两者相等表示关闭，默认 23–7），跨午夜判断统一走 `time_util_hour_in_range`（有宿主单元测试覆盖边界，含 23→7 与同日区间）。RTC 时间不可信时必须返回"不静默"，否则掉电未对时会把设备永久静音。监听任务在进入/退出该时段时各打一条日志，并输出"当前时间 + 配置区间 + 判定结果"三要素：日志上"时段没配对"与"RTC 时间不对"表现完全一样（都是不静默），但改法不同，缺了这三要素无法定位。
- 唤醒命中前先过帧能量门限（`WAKE_MIN_FRAME_RMS = 200`，左声道 RMS），不达标的帧不进入 WakeNet 推理；命中冷却为 10 秒（原 3 秒过短，一次误命中后环境噪声仍在，会连着触发数轮）。监听健康日志必须带 quiet 帧数，用于判断门限是否设得过严导致漏检。
- 服务端把每一次 hello 都计作一条新会话，因此后台预热/重连会直接在用户后台留下"只有开场白"的记录。免打扰时段内不得预热也不得重连（已建立的连接保持不动，不主动断）；链路在 60 秒内反复断开时视为不稳定，重连必须递增退避而不是把退避清零，否则退化成每秒一次重连并刷屏会话记录。握手日志必须带累计连接次数与"距上次断开秒数"，这是判断会话记录是否由重连产生的直接依据。
- 音频输出音量按「是否小智语音」分成两路：`voice_volume`（智能语音音量，0-100，默认 60）只用于小智下行播放；`system_volume`（普通操作音量，0-100，默认 60）是按键音、开关机音、RaceBox 状态音以及企微签到/签退、工时记录提醒铃声的直接播放音量——非小智声音的音量与波形幅度（幅度 9000 的正弦）完全一致，一律按 `system_volume` 原值输出，提醒铃声只用旋律长短和音高组合区分（约 1 秒、签到上行三音/签退下行三音/工时双音重复），不得为提醒铃声单独提高音量或波形幅度，`system_volume=0` 时全部静音。音频服务分别维护普通操作音量和当前小智音量：进入 TTS 播放时切换到 `voice_volume`，会话正常结束、断线、超时和启动失败均恢复普通操作音量；提示音（含提醒铃声）在小智会话期间插播后也必须恢复到当时的小智音量，不能把全局输出永久改成 100。禁止在语音 start 路径写死全局音量 100。
- 企微签到/签退与工时记录提醒到达时播放一段多音提示音乐（签到上行、签退下行、记录工时双音重复，各约 1 秒），铃声使用 `voice_volume` 配置值；提醒条按 `remind_sign_hold_seconds`（企微签到/签退）与 `remind_worktime_hold_seconds`（工时记录）停留，两者默认 300 秒，合法范围 5–3600 秒；到达时长未按 `KEY` 即自动隐藏，`KEY` 短按可提前关闭。
- 声纹服务返回结果应包含匿名 speaker ID、similarity、threshold 与 decision。命中已注册说话人时仅用于个性化和按 speaker ID 隔离记忆；未命中、低置信度、网络错误、超时或空结果统一标记 anonymous，仍允许调用普通 LLM/MCP，但不得读取或写入某个已注册用户的私有记忆。阈值校准报告至少包含注册人的留出样本和其他家庭成员样本的最低/最高/分位相似度，生产阈值留出防止错误归属的安全间隔。
- 正常追问窗口从设备确认 TTS 播放结束后开始，默认10秒；窗口到期发送会话结束/abort并恢复WakeNet。30秒无下行活动只保留为卡死保护。新的唤醒必须生成新 turn 链，服务端不得复用超过追问窗口的旧 conversation memory。
- 延迟日志统一使用单调时钟计算并携带 `session_id`、递增 `turn_id`、`stage`、`elapsed_ms`。板端至少覆盖 `wake_detected`、`listen_sent`、`first_uplink`、`stt_received`、`tts_text_received`、`first_audio_received`、`playback_started`；服务端至少覆盖 `vad_end`、`asr_done`、`speaker_done`、`context_done`、`llm_first_token`、`tts_first_chunk`。验收取不少于10轮的P50/P95，不能用单轮最快值。

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
- 固件版本的单一来源是 Git 标签 `vMAJOR.MINOR.PATCH`；Release 构建把去掉前导 `v` 的版本写入 ESP-IDF `esp_app_desc.version`，设备 `/api/health`、后台页面和 Release 文件名必须一致。本地非标签构建使用明确的开发版后缀，不得伪装成正式版本。
- GitHub Release 固件资产固定包含：`deskwong-ota-vMAJOR.MINOR.PATCH.bin`（仅 app 镜像，供后台 OTA）、`deskwong-factory-vMAJOR.MINOR.PATCH.bin`（含 bootloader、分区表、otadata、app、SPIFFS，供 USB/esptool 全量刷机）、`SHA256SUMS` 和描述偏移/用途/版本的 manifest；不再把容易误选的内部组件 BIN 单独作为面向用户的资产。
- `POST /api/ota` 只接收 app 镜像。服务端在擦写前检查 ESP32-S3 镜像头、app 描述的项目名 `deskwong`、目标版本和 Content-Length 上限；写入后由 `esp_ota_end` 完整校验，通过后才设置下一启动分区。merged/factory、bootloader、partition-table、SPIFFS 或损坏文件必须返回明确错误且不得改变启动分区。
- 日历自动返回路径不得从 PSRAM 栈任务直接访问 NVS/Flash；需要恢复当月工时缓存时，必须切换到内部 RAM 栈任务执行，或只更新目标月份并交给既有内部 RAM 工作任务处理。验收需包含超时前后完整串口日志、复位原因和持续运行观察，不能只验证界面回到主屏。
