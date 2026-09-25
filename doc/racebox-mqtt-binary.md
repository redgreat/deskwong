# RaceBox MQTT 二进制消费协议（RBX1）

固件从 RBX1 版开始直接上传 RaceBox 的 80 字节原始记录，不再把每个字段展开成 JSON。
每条 MQTT 消息最多包含 48 条记录，使用 QoS 1。750 条记录通常为 16 条 MQTT
消息、约 61.5 KB；接收端不需要解压库，只需按小端序解码。

默认 Topic：`deskwong/racebox/data`。

## 1. 消息头

前 96 字节是固定消息头，其后紧跟 `count * record_size` 字节的记录区。所有整数均为
little-endian（小端序）。

| 偏移 | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 4 | magic | ASCII `RBX1` |
| 4 | 1 | version | 当前为 `1` |
| 5 | 1 | flags | 当前为 `0`，表示原始记录且未压缩 |
| 6 | 2 | header_size | 当前为 `96` |
| 8 | 2 | record_size | 当前为 `80` |
| 10 | 2 | count | 本消息的记录数，最大 48 |
| 12 | 4 | offset | 本批首条记录在本次同步中的序号，从 0 开始 |
| 16 | 4 | total | 本次同步实际下载的记录总数 |
| 20 | 4 | sync_date | 同步日期，格式 `YYYYMMDD` |
| 24 | 16 | import_id | 本次同步的随机 128 位 ID；用于重组和幂等去重 |
| 40 | 48 | device_name | UTF-8 设备名，以 `NUL` 结尾，剩余字节补 0 |
| 88 | 4 | payload_crc32 | 记录区的标准 CRC-32/IEEE |
| 92 | 4 | header_crc32 | 头部 0..91 字节的 CRC-32/IEEE |
| 96 | 变长 | records | `count` 个连续的 80 字节记录 |

QoS 1 可能重复投递。建议数据库唯一键使用
`(import_id, record_index)`，其中 `record_index = offset + 批内序号`。
收到所有 `0..total-1` 记录后，本次同步才算完整。

## 2. 单条 80 字节记录

| 偏移 | 类型 | 数据库字段 | 换算 |
|---:|---|---|---|
| 0 | uint32 | itow | 原值 |
| 4 | uint16 | year | 原值 |
| 6,7 | uint8 | month, day | 原值 |
| 8,9,10 | uint8 | hour, minute, second | 原值 |
| 12 | uint32 | time_accuracy | 原值 |
| 16 | int32 | nanoseconds | 原值 |
| 20 | uint8 | fix_status | 原值 |
| 23 | uint8 | numberof_svs | 原值 |
| 24 | int32 | longitude | 除以 `1e7` |
| 28 | int32 | latitude | 除以 `1e7` |
| 32 | int32 | wgs_altitude | 除以 `1000`，米 |
| 36 | int32 | msl_altitude | 除以 `1000`，米 |
| 40 | uint32 | horizontal_accuracy | 除以 `1000`，米 |
| 44 | uint32 | vertical_accuracy | 除以 `1000`，米 |
| 48 | int32 | speed | 除以 `1000`，m/s |
| 52 | int32 | heading | 除以 `1e5`，度 |
| 56 | uint32 | speed_accuracy | 原值 |
| 60 | uint32 | heading_accuracy | 原值 |
| 64 | uint16 | pdop | 原值 |
| 68,70,72 | int16 | gforce_x/y/z | 各除以 `1000` |
| 74,76,78 | int16 | rotation_rate_x/y/z | 各除以 `100` |

未列出的字节为 RaceBox 保留字段，消费端应忽略。

## 3. Python 解码示例

```python
import struct
import uuid
import zlib

HEADER_SIZE = 96
RECORD_SIZE = 80

def decode_record(raw: bytes) -> dict:
    assert len(raw) == RECORD_SIZE
    u32 = lambda off: struct.unpack_from("<I", raw, off)[0]
    i32 = lambda off: struct.unpack_from("<i", raw, off)[0]
    u16 = lambda off: struct.unpack_from("<H", raw, off)[0]
    i16 = lambda off: struct.unpack_from("<h", raw, off)[0]
    return {
        "itow": u32(0), "year": u16(4), "month": raw[6], "day": raw[7],
        "hour": raw[8], "minute": raw[9], "second": raw[10],
        "time_accuracy": u32(12), "nanoseconds": i32(16),
        "fix_status": raw[20], "numberof_svs": raw[23],
        "longitude": i32(24) / 1e7, "latitude": i32(28) / 1e7,
        "wgs_altitude": i32(32) / 1000.0,
        "msl_altitude": i32(36) / 1000.0,
        "horizontal_accuracy": u32(40) / 1000.0,
        "vertical_accuracy": u32(44) / 1000.0,
        "speed": i32(48) / 1000.0, "heading": i32(52) / 1e5,
        "speed_accuracy": u32(56), "heading_accuracy": u32(60),
        "pdop": u16(64),
        "gforce_x": i16(68) / 1000.0,
        "gforce_y": i16(70) / 1000.0,
        "gforce_z": i16(72) / 1000.0,
        "rotation_rate_x": i16(74) / 100.0,
        "rotation_rate_y": i16(76) / 100.0,
        "rotation_rate_z": i16(78) / 100.0,
    }

def decode_rbx1(payload: bytes) -> dict:
    if len(payload) < HEADER_SIZE or payload[:4] != b"RBX1":
        raise ValueError("不是 RBX1 消息")
    version, flags = payload[4], payload[5]
    header_size, record_size, count = struct.unpack_from("<HHH", payload, 6)
    offset, total, sync_date = struct.unpack_from("<III", payload, 12)
    if version != 1 or flags != 0 or header_size != 96 or record_size != 80:
        raise ValueError("不支持的 RBX1 版本或编码")
    if len(payload) != header_size + count * record_size:
        raise ValueError("消息长度错误")
    data_crc, header_crc = struct.unpack_from("<II", payload, 88)
    records_raw = payload[header_size:]
    if zlib.crc32(payload[:92]) & 0xffffffff != header_crc:
        raise ValueError("消息头 CRC 错误")
    if zlib.crc32(records_raw) & 0xffffffff != data_crc:
        raise ValueError("记录区 CRC 错误")
    import_id = str(uuid.UUID(bytes=payload[24:40]))
    device_name = payload[40:88].split(b"\0", 1)[0].decode("utf-8")
    records = []
    for i in range(count):
        row = decode_record(records_raw[i*80:(i+1)*80])
        row.update(import_id=import_id, record_index=offset+i,
                   device_name=device_name, sync_date=sync_date)
        records.append(row)
    return {"import_id": import_id, "offset": offset, "total": total,
            "device_name": device_name, "sync_date": sync_date,
            "records": records}
```

Paho MQTT 的 `msg.payload` 本身就是 `bytes`，直接调用
`batch = decode_rbx1(msg.payload)`，再使用数据库批量插入即可。插入时使用
`INSERT ... ON DUPLICATE KEY UPDATE` 或 `INSERT IGNORE`，以应对 QoS 1 重复消息。

## 4. 完整性与失败处理

1. 先校验两个 CRC，再写数据库。
2. 按 `import_id` 分组，按 `record_index` 去重。
3. 同一个 `import_id` 收到的 `total` 应保持一致。
4. 数据库中该 `import_id` 的不同 `record_index` 数量等于 `total` 时，标记导入完成。
5. 连接中断时设备会对当前批次最多重试 3 次；重复消息不会代表新增轨迹点。

