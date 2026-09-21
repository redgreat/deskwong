# 实机显示排查（2026-09-12）

## 已确认的原因

1. 从 COM7 读取整片 Flash，ota_0 内的应用描述为 `15b18ca-dirty`，编译时间 `Sep 12 2026 00:03:57`。与上一轮本地新版产物不同；照片中的周日首列和细体数字属于旧页面。
2. 直接刷入新版并读取实机日志后，确认启动 `0.2.0-ui2`，但发现 128 KB LVGL 静态内存池占用了片内 RAM，音频 DMA 分配失败，随后 mDNS 栈申请失败触发 abort 重启。主机预览无法揭示这一设备资源问题。
3. 原串口脚本在编译/资源构建失败后仍继续刷写，且不能清楚确认产物版本。

## 参考项目核对

| 项目 | 核对内容 | 本项目结论 |
| --- | --- | --- |
| [wickenzh](https://github.com/wickenzh/ESP32-S3-RLCD-4.2/blob/35d8f97584f8669c0c9cc33fbb1719b735c6f2f1/RLCD_CLOCK/CMakeLists.txt) | LV_MEM_POOL_ALLOC 把 LVGL 对象池放 PSRAM；未压缩中文字库；局部 x 范围刷新 | 使用 LVGL 原生内存池分配钩子，把 128 KB 池放 PSRAM，保留 TLSF 管理和内存统计 |
| [wudingjian](https://github.com/wudingjian/waveshare-s3-rlcd-4.2) | 此仓库提供固件和构建说明；按 README 跟进 [实际源码](https://github.com/ZhouhaoJiang/xiaozhi-esp32/tree/main/main/boards/waveshare-s3-rlcd-4.2)，核对 RGB565 到 1bpp 转换 | 400×300、0x7fff 阈值与本项目一致，不是文字单独消失的原因 |
| [gpt-kanban](https://github.com/ThomasAnderson01/gpt-kanban/blob/e4ef216e4277348e1b1b8a85855bf2687ea238cd/firmware/src/display/rlcd_driver.cpp) | SPI 引脚、0x36=0x48、0x3A=0x11、全帧传输与像素映射 | 初始化/像素排列一致；未盲目替换驱动 |
| [s3-rlcd-deck](https://github.com/Vectorking-50kg/s3-rlcd-deck/blob/c010603ba888d22219484329db8ed10f00c8f669/firmware/components/board_support/deck_rlcd_panel.cpp) | SPI 10 MHz、异步传输完成回调、0x2A/0x2B 地址窗口；未压缩字库 | 本项目保持传输完成后才复用帧缓冲，字库使用未压缩格式 |
| [gb-emulator](https://github.com/tigerxu255-lgtm/esp32-s3-rlcd-gb-emulator/blob/0ed9478d0b57a17752fbe0f42f161b31c3af720a/components/board_rlcd/board_rlcd.cpp) | 每字节 2×4 像素、y 反转、SPI 地址窗口、PSRAM 帧缓冲 | 映射与本项目一致，不能靠更换布局或阈值修复旧固件/内存不足 |

## 本轮修复

- 固定 LVGL 对象池通过 `LV_MEM_POOL_ALLOC` 分配到 PSRAM，释放 128 KB 片内静态占用。
- mDNS 非必需服务失败时降级为 IP 访问，避免该服务单独失败就重启整个界面。
- 固件版本 `0.2.0-ui2`；启动日志及 `/api/health` 输出版本、编译日期时间、启动分区与地址。
- 串口脚本检查构建、打包和刷写退出码，失败立即停止；完整串口更新选择 ota_0，默认本机 COM7，也可传入端口。
- 整片 Flash 备份存于被 Git 忽略的 `build/device-before-display-fix.bin`，含设备配置，勿发布；SHA256 为 `3b1a1d5dd7864d2a593045979483603729d507ebec2a721fecaa0dd1984b115c`。
- 本次设备更新只写 ota_0 应用分区 `0x20000`，设备原本就选择 ota_0；保留 NVS、OTA 选择和 SPIFFS。

## 验证状态

最终设备启动与稳定性日志见本轮后续记录。

### 实机验证结果

- 已通过 COM7 直接写入最终应用，esptool 返回 `Hash of data verified`。
- 最终应用 SHA256：`6ca923fe4a0644f92c05639568a95412969fd7a0eb4a910c411b2aa8bb8c5cc0`，大小 1628512 字节。
- 重启后实机日志：`UI2 firmware=0.2.0-ui2 ... partition=ota_0 address=0x20000`。
- 实机确认：`UI pool: 131072 bytes in PSRAM`、`first display frame flushed (400x300)`。
- 实机完成启动：`boot done; internal free=70167 PSRAM free=7358696`。
- 45 秒捕获中只出现 1 次启动、0 次 abort、0 次 Guru Meditation；音频 DMA 和 mDNS 分配错误已消失。
- 日志：`build/ui2-device-fixed-boot.log`。当前为 AP 模式（192.168.4.1），未配置/不可访问的联网数据保持占位。
- 已请用户确认物理屏幕上的新版字段；串口证据不等于相机/肉眼图像验收。
