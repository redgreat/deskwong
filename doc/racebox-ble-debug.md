# RaceBox BLE 下载诊断（2026-09-25）

ESP-IDF 5.5.1 的 NimBLE 在 `BLE_GATTS` 条件下编译 ATT notification
接收入口。只开启 GATT Client 时，RaceBox 已连接、MTU 247、订阅成功，
但业务回调收不到通知，随后出现 `ACL buf alloc failed` / `MBUF alloc stuck`。
`sdkconfig.defaults` 必须保留 GATT Server 及其 Peripheral 角色依赖。
现有 build 的 sdkconfig 也需要开启这些选项。

修复后的实机日志 `build/racebox-gatts-verified.log` 已收到下载响应、历史帧
和完成 ACK，未再出现 ACL 耗尽。该轮旧计数为 751（包含下载前的实时帧），
设备报告上限为 753；不能把旧计数当作精确历史条数。

按官方协议修正后：

- 仅 `FF/21` 计入历史记录，忽略 `FF/01` 实时数据。
- `FF/23` 的数量为预计上限，实际条数可以略少。
- 仅接受负载 `FF 23` 的 `FF/02` 作为下载完成确认。
- 完成后断开 BLE，停止累加，进度显示 100%。
- 下载完成不设置 MQTT 上传成功标志。

协议来源：<https://www.racebox.pro/products/mini-micro-protocol-documentation?k=67c166d0bda80de96505efba>

最终固件已刷入 COM7 并通过哈希校验。用户按 KEY 的实机复测日志
`build/racebox-final-live.log`：`download complete received=750 expected=753`，
随后 `disconnect requested rc=0`、`disconnected reason=534 notifications=275`。
未出现 ACL 耗尽或传输超时，完成后不再计入实时帧。

当前接收路径仍只校验和统计记录，尚未接通记录持久化和 MQTT 上传。
下载验证不代表端到端同步完成；不得据此擦除 RaceBox 原始数据。
