# 0.2.1-ui3 后台重启修复与界面验证

## 实机复现和修复

连接设备热点后，`GET /api/health` 成功，而 `GET /` 立即断开并重启。
串口明确记录 `spi_flash_disable_interrupts_caches_and_other_cpu` 中
`esp_task_stack_is_sane_cache_disabled()` 断言失败，栈位于 0x3c... PSRAM。
HTTP 任务要读取 SPIFFS、保存 NVS 和执行 OTA，任务栈必须位于片内 DRAM。
本次将 HTTP 12 KB 栈改为 MALLOC_CAP_INTERNAL，保留 LVGL 的 PSRAM 对象池。

同时修复密码输入框留空保存时误清空已有密码的问题；空字符串和脱敏占位都保留旧值。

## 界面与接口

- 工时 `记录小时数/月总工时 百分比`，去掉 h。
- ChatGPT 图标 + 额度；5小时显示 HH:MM，每周显示 MM/DD 和 HH:MM；额度百分比仅用于条形图。
- 聚合 API 通过 window_minutes=300/10080、remaining_percent（或 used_percent）、resets_at（Unix 秒）提供窗口信息。
- 上海时区按 CST-8 转换显示；未提供重置时间时显示占位。
- 正常、无数据、六行月份真实 LVGL 渲染预览在 design/final/ui3-preview*.png。
- ChatGPT 图标源：https://upload.wikimedia.org/wikipedia/commons/0/04/ChatGPT_logo.svg，提取结形标志转为 24×24 单色 alpha。

## 验证结果

- ESP-IDF 编译通过并已通过 COM7 刷入；校验通过。
- 实机 /api/health 返回 firmware_version=0.2.1-ui3、ota_0、ap_mode=true、ip=192.168.4.1。
- 首页 + 两个 CSS/JS 资源连续 10 轮加载成功。
- 登录、读取配置、NVS 保存及留空密码后再次登录成功；串口没有断言或崩溃。
- 登录凭据已验证为项目默认 admin/admin，未修改密码。
- 主机额度解析回归通过：5小时/每周窗口、剩余量/已用量转换、重置时间、旧记录、无效字段及网络失败。
- 设备当前尚未配置路由器 Wi-Fi，只能报告配置热点地址，不能声称已获得路由器内网 IP。
- 本轮测试后电脑已恢复原 Wi-Fi。
- 固件 SHA256：606cdfda5dd37ea9e62bc08b692f857c4cfaab95f5d2e59040cf41a92deef364。
