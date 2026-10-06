# 系统架构与代码现状

> 描述当前实现，不承载未来需求。最近更新：2026-10-01。

## 1. 总体结构

```text
ESP32-S3 firmware
  ├─ app_config：NVS 配置与脱敏
  ├─ app_net：httpd、WiFi STA/AP、mDNS
  ├─ app_services：日历、天气、工时、AI、提醒、音频、MQTT、RaceBox
  ├─ app_ui：400×300 LVGL 主屏和 RaceBox 同步弹窗
  └─ BSP/codec：RLCD、I2C、按键、ADC、ES7210/ES8311

web (Svelte/Vite) ──构建──> SPIFFS /spiffs/www
service (Go) ──> /worktime/summary、/ai/usage、/admin
RaceBox ─BLE─> firmware ─MQTT RBX2─> agentwong/emqx_pg_ingest ─> PostgreSQL
firmware cached summaries ─HTTPS POST─> service context snapshot store
XiaoZhi context provider ─HTTPS GET + device-id─> service ─> prompt dynamic_context
```

## 2. 固件

- ESP-IDF 5.5.1、LVGL 8.4，目标 ESP32-S3 N16R8。
- `firmware/main/main.cpp` 负责组件初始化与任务编排。
- `net_scheduler` 串行运行天气、工时和 AI 请求，避免共享网络缓冲并发。
- 语音唤醒任务固定在 core 1 以低优先级运行，避免与固定在 core 0 的 WiFi/TCPIP/NimBLE 争抢；联网后后台任务预取 OTA 地址/token 并建立 WSS/hello，唤醒热路径只发送 detect/start。进入会话后，麦克风任务在 core 1 编码并以有界超时发送 Opus；下行 Opus 回调只复制入队，由独立 32KB PSRAM 栈任务解码和写 I2S。WebSocket 任务本身使用 16KB PSRAM 栈，禁止长期占用紧张的内部 RAM；创建失败时才回退内部栈。WebSocket 文本与音频共用独立 TX 互斥串行发送，上行网络等待不持有语音生命周期互斥锁。网络回调只保存最新 UI 快照，200ms UI 任务在取得 LVGL 锁后补绘，避免一次锁失败丢失弹窗。唤醒监听每 10 秒输出一次音量、推理耗时和栈余量，语音上行每 50 帧输出慢帧与发送失败计数，供实机区分采集、CPU 和网络故障。
- 工时按月经 NVS 缓存（`deskwong` 命名空间的 `wt_YYYYMM` 快照）：开机先显示缓存或本地应收工时，网络任务返回后覆盖，拉取失败保留旧值，主屏不出现 `--`；缓存写入由内部 RAM 栈的小任务完成，避免 flash cache 关闭时访问 PSRAM 栈。
- `main_screen.cpp` 是主屏布局唯一实现；`sync_screen.cpp` 是 RaceBox 同步覆盖层。
- `weather_almanac_screen.cpp` 是天气黄历覆盖层：共享 RaceBox 弹窗的黑白卡片视觉语言，显示今天起 7 天、当前起 7 个小时和当天宜忌；温度与日期/小时由四个轻量文本行对象绘制，每列按字库实际像素宽度独立居中，避免双行标签在实屏上产生左对齐错觉，同时控制 128KB LVGL 对象池占用。按键任务维护主屏、日历翻页和天气黄历三种界面状态。
- 显示内部使用 RGB565，flush 时阈值化为 1-bit 单色；非 8 像素宽图片按行补齐字节。
- RaceBox 当前使用 64 条左右的小型 PSRAM 流式缓存，按 MQTT PUBACK 释放；RBX2 支持轨迹分段和稳定幂等键。
- 语音上下文不直接从语音回调读取各业务组件，也不让公网服务回连设备。主任务从已经展示/持久化的工时、AI、天气黄历和 RaceBox 当日计数生成轻量快照，通过网络调度器合并上报；上报失败只保留待发送标志，不影响语音、UI 或原业务刷新。

## 3. Web 配置后台

- `web/` 为纯静态 Svelte/Vite 应用，无 SSR。
- 固件提供登录、配置、健康、重启、恢复出厂和 OTA API。
- Web 登录页不内置默认账号，用户名与密码状态均从空值开始；保留标准浏览器密码管理器语义，实际认证账号只由设备 NVS 配置决定。
- 固件版本由 Git 标签注入 ESP-IDF app 描述；本地构建使用带 `dirty`/开发标记的 Git 描述。健康接口与后台读取同一份 app 描述。Web OTA 只接受项目名为 `deskwong` 的 app 镜像，浏览器与设备分别预检镜像头、项目和版本；factory 合并镜像只用于 USB 全量刷机。
- 配置写入 NVS；敏感字段回读为 `******`。页面包含浅/深色主题、设备状态和各功能分组。
- 黄历 API 地址与 Key 由天气分组配置；设备向地址追加 `date`、`location`、`key` 查询参数，Key 与其他敏感字段一样脱敏回读。

## 4. Go 服务

- `service/` 使用 Go 1.25，提供统一设备接口和独立 `/admin`。
- 配置首次从 YAML 导入，之后以 `/app/data/settings.sqlite` 为准。
- 敏感字段在 SQLite 中用 AES-256-GCM 加密；优先使用 `DESKWONG_SETTINGS_KEY`，否则生成同目录 0600 密钥文件。
- 工时源优先级：`DESKWONG_WORKTIME_UPSTREAM` → PingCode 网页 API → MySQL 兼容查询。
- PingCode 登录令牌仅驻留内存；按月份分页拉取 `workload-logs/mine` 并按 `register_date` 聚合 `man_hour`。
- AI 用量可代理上游，或读取 Codex 登录凭据并归一化为设备契约。
- 服务保存按 `device-id` 隔离的最新开发板上下文快照，并提供小智上下文源 GET 接口。GET 只读本地快照，不同步扇出外部依赖，保证唤醒 Prompt 构建延迟稳定；上报鉴权和上下文读取鉴权使用独立用途的 Bearer Token。

## 5. 构建与发布

- `tools/flash.bat`：固件、Web、SPIFFS 和刷写的一键入口。
- `.github/workflows/build-firmware.yml`：固件持续构建。
- `.github/workflows/release.yml`：标签触发服务测试、amd64/arm64 镜像推送、Release 和固件附件。
- Release 固件附件面向用户收敛为 `deskwong-ota-v*.bin`、`deskwong-factory-v*.bin`、`manifest.json` 和 `SHA256SUMS`；内部 bootloader/分区/SPIFFS 分件不再单独发布。
- 镜像目标为 GHCR 与由 Quay 机器人账号推导出的命名空间。

## 6. 当前边界

- 服务端 PingCode 真实读取已验证 `2026-10-01 = 2.0h`。
- RBX2 主机测试已通过；最终完成仍以目标固件构建、COM 口刷写和真实整机同步为准。
- 小智服务部署文件已准备，但完整 WSS 对话链路仍需目标环境验收。
