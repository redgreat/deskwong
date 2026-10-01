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
```

## 2. 固件

- ESP-IDF 5.5.1、LVGL 8.4，目标 ESP32-S3 N16R8。
- `firmware/main/main.cpp` 负责组件初始化与任务编排。
- `net_scheduler` 串行运行天气、工时和 AI 请求，避免共享网络缓冲并发。
- `main_screen.cpp` 是主屏布局唯一实现；`sync_screen.cpp` 是 RaceBox 同步覆盖层。
- 显示内部使用 RGB565，flush 时阈值化为 1-bit 单色；非 8 像素宽图片按行补齐字节。
- RaceBox 当前使用 64 条左右的小型 PSRAM 流式缓存，按 MQTT PUBACK 释放；RBX2 支持轨迹分段和稳定幂等键。

## 3. Web 配置后台

- `web/` 为纯静态 Svelte/Vite 应用，无 SSR。
- 固件提供登录、配置、健康、重启、恢复出厂和 OTA API。
- 配置写入 NVS；敏感字段回读为 `******`。页面包含浅/深色主题、设备状态和各功能分组。

## 4. Go 服务

- `service/` 使用 Go 1.25，提供统一设备接口和独立 `/admin`。
- 配置首次从 YAML 导入，之后以 `/app/data/settings.sqlite` 为准。
- 敏感字段在 SQLite 中用 AES-256-GCM 加密；优先使用 `DESKWONG_SETTINGS_KEY`，否则生成同目录 0600 密钥文件。
- 工时源优先级：`DESKWONG_WORKTIME_UPSTREAM` → PingCode 网页 API → MySQL 兼容查询。
- PingCode 登录令牌仅驻留内存；按月份分页拉取 `workload-logs/mine` 并按 `register_date` 聚合 `man_hour`。
- AI 用量可代理上游，或读取 Codex 登录凭据并归一化为设备契约。

## 5. 构建与发布

- `tools/flash.bat`：固件、Web、SPIFFS 和刷写的一键入口。
- `.github/workflows/build-firmware.yml`：固件持续构建。
- `.github/workflows/release.yml`：标签触发服务测试、amd64/arm64 镜像推送、Release 和固件附件。
- 镜像目标为 GHCR 与由 Quay 机器人账号推导出的命名空间。

## 6. 当前边界

- 服务端 PingCode 真实读取已验证 `2026-10-01 = 2.0h`。
- RBX2 主机测试已通过；最终完成仍以目标固件构建、COM 口刷写和真实整机同步为准。
- 小智服务部署文件已准备，但完整 WSS 对话链路仍需目标环境验收。
