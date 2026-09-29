# RaceBox MQTT 二进制协议（RBX2）

默认 Topic：`deskwong/racebox/data`。所有整数均为小端，轨迹记录固定 80 字节；消息使用 MQTT QoS 1，所以消费端必须幂等。

RBX2 把“一次蓝牙同步”和“设备中的一段轨迹”分开建模：`sync_id` 每次同步随机生成，只用于排查传输；`session_key` 由设备标识、首条记录 UTC、iTOW 和纳秒生成，同一段轨迹重复下载时保持不变。MQTT 批次不会跨越轨迹段。

## 1. 消息结构

消息长度为 `136 + record_count * 80`。

| 偏移 | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 4 | magic | ASCII `RBX2` |
| 4 | 1 | version | `2` |
| 5 | 1 | flags | bit0=本次同步最后一批；bit1=本轨迹段最后一批 |
| 6 | 2 | header_size | `136` |
| 8 | 2 | record_size | `80` |
| 10 | 2 | record_count | 本消息记录数，当前最大 720 |
| 12 | 4 | sync_offset | 本批首条记录在本次同步中的序号 |
| 16 | 4 | sync_total | 非最终批可为设备预报上限；最终批为实际总数 |
| 20 | 4 | sync_date | 触发同步的本地日期 `YYYYMMDD` |
| 24 | 16 | sync_id | UUID 原始 16 字节；每次同步随机生成 |
| 40 | 48 | device | NUL 结尾的设备名称/序列号 |
| 88 | 4 | session_index | 本次同步内轨迹段序号，从 0 开始 |
| 92 | 4 | session_offset | 本批首条记录在轨迹段内的序号 |
| 96 | 4 | session_total | 段尾前为 0，段尾批为准确总数 |
| 100 | 8 | session_start_utc | 首条记录的 `YYYYMMDDhhmmss` 数值 |
| 108 | 8 | session_end_utc | 段尾前为 0，段尾批为末条时间 |
| 116 | 4 | session_start_itow | 首条记录 iTOW |
| 120 | 4 | session_start_nano | 首条记录纳秒，有符号 |
| 124 | 4 | reserved | 0 |
| 128 | 4 | payload_crc32 | 所有记录字节的 IEEE CRC32 |
| 132 | 4 | header_crc32 | `[0,132)` 的 IEEE CRC32 |
| 136 | N×80 | records | 原始 RaceBox 记录 |

## 2. 轨迹段与流式上传

- `0x21` 和设备历史下载阶段出现的 `0x01` 都作为 80 字节轨迹记录接收。
- RaceBox `0x26` 是精确的轨迹段边界；下载完成 ACK 也会关闭最后一个尚未关闭的段。
- 为避免缓存数十万条记录，固件边下载边上传。开放轨迹段只保留最后 720 条；收到 `0x26` 后，最后一批携带准确的 `session_total` 和结束时间。
- 消费端生成稳定键：`rbx2_<device>_<start_utc>_<start_itow>_<start_nano>`；批次键再追加 `_<session_offset>`。
- 最终文件名与旧 Python 消费端一致：`<首条UTC>_<末条UTC>`。段尾前暂用包含首条 iTOW/纳秒的唯一 `_open_` 名称，段尾事务统一改名。

## 3. 幂等规则

消费端应同时落实三层约束：

1. `imp_racebox.session_key` 唯一：同一轨迹段重复同步只保留一份。
2. `imp_racebox_batch.batch_key` 唯一：MQTT QoS 1 重投不重复处理批次。
3. `lc_racebox(session_key, record_index)` 唯一：记录级最终防线。

如果数据库已存在旧 Python 消费端按最终文件名导入的数据：短轨迹在首个最终批直接跳过；长轨迹先前产生的 `_open` 临时数据在段尾事务中删除，保留旧数据。

只有实际明细数达到 `session_total` 时，导入状态才从 `receiving` 变为 `complete`。中断后的再次同步可以补齐缺失批次。

## 4. 80 字节记录

记录字段布局沿用 RBX1/RaceBox 原始格式。主要偏移：iTOW 0、UTC 年月日时分秒 4..10、纳秒 16、fix 20、卫星数 23、经纬度 24/28、海拔 32/36、精度 40/44、速度 48、航向 52、速度/航向精度 56/60、PDOP 64、三轴加速度 68/70/72、三轴角速度 74/76/78。

速度原始单位为 mm/s，插件写入 `lc_racebox.speed` 前转换为 km/h。

## 5. RBX1 兼容

插件仍能解码 RBX1（96 字节头），并接受 flags bit0 的最终批标记，供旧固件过渡使用。RBX1 只有随机 `import_id + offset` 的消息级去重，无法识别跨同步的同一轨迹段；新固件必须使用 RBX2。
