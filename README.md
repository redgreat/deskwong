# deskwong

微雪 **ESP32-S3-RLCD-4.2**（全反射屏）桌面摆件固件：电子日历、天气/温湿度、工时统计与本地提醒、RaceBox 轨迹经 MQTT 采集上传、AI 用量展示、Svelte 配置后台。

> 文档从 [`doc/AI_RULES.md`](doc/AI_RULES.md) 开始；整体需求、架构、规范和计划均为单文件事实来源。

## 目录结构

```
deskwong/
├── doc/          # AI 入口、整体需求、架构、协议规范与计划
├── firmware/     # ESP-IDF v5.5.1 固件（main/ + components/）
├── web/          # Svelte 配置后台前端（Vite 构建）
├── test/         # 服务、固件与 UI 真实渲染测试
├── tools/        # 刷机脚本（flash.bat / make_spiffs.bat）
└── README.md
```

## 硬件

| 资源 | 说明 |
| --- | --- |
| SoC | ESP32-S3-WROOM-1-N16R8（240MHz / 16MB Flash / 8MB PSRAM，WiFi + BLE5） |
| 屏幕 | 4.2" 300×400 单色全反射 LCD（RLCD，1bpp，无需背光） |
| 传感 | SHTC3 温湿度、PCF85063 RTC |
| 音频 | ES8311 + ES7210 双麦（语音/录音，二期） |
| 按键 | KEY（GPIO18）、BOOT（GPIO0） |
| 电源 | 18650 电池座 + 充放电管理（Type-C） |

## 快速开始（Windows 刷机）

### 0. 环境准备

安装 **ESP-IDF v5.5.1**（与官方例程同版本），并安装 Node.js ≥ 18：

- ESP-IDF Windows 安装：<https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/windows-setup.html>
- 装好后在 **ESP-IDF 命令提示符**（`ESP-IDF 5.5 CMD`）里操作。

### 1. 一键编译 + 刷机

```bat
:: 进入项目目录
cd /d <本项目路径>

:: 一键：编译固件 → 构建前端 → 生成 SPIFFS → 烧录
tools\flash.bat
```

> `tools\flash.bat` 默认串口 `COM7`；可用 `tools\flash.bat COM3` 指定其他端口。编译、资源打包或烧录失败会立即停止。

当前首页固件版本为 `0.2.1-ui3`。启动日志和 `/api/health` 可核对实际版本、编译时间及运行分区。`firmware/build/deskwong.bin` 是应用镜像（本项目 ota_0 地址 `0x20000`），不能当作地址 `0x0` 的合并镜像刷写。完整串口脚本会初始化 OTA 选择以启动刚写入的 ota_0。

### 2. 用合并镜像一键刷（推荐，无需编译）

若已通过 GitHub Actions 或本地编译生成了 `deskwong-merged.bin`（单文件 16MB，含 bootloader/分区表/固件/web 资源），只需一条命令：

```bat
:: 需安装 esptool（pip install esptool），COM3 改为实际串口
python -m esptool --chip esp32s3 -b 460800 -p COM3 write_flash 0x0 deskwong-merged.bin
```

也可以用官方 **ESP Flash Download Tool**（图形界面）：ChipType 选 ESP32-S3，把 `deskwong-merged.bin` 地址填 `0x0` 打勾，一键烧录。

### 2. 分步执行（可选）

```bat
:: 编译固件
cd firmware && idf.py build

:: 构建 Svelte 前端 + 生成 SPIFFS 镜像
tools\make_spiffs.bat

:: 烧录（4 个分区：bootloader / 分区表 / 固件 / web 资源）
python -m esptool --chip esp32s3 -b 460800 -p COM3 write_flash ^
  0x0 firmware\build\bootloader\bootloader.bin ^
  0x8000 firmware\build\partition_table\partition-table.bin ^
  0x20000 firmware\build\deskwong.bin ^
  0x820000 firmware\build\spiffs.bin
```

### 3. 首次开机配置

1. 上电后屏幕点亮，显示时间/温湿度/电量占位。
2. 设备默认进入 **AP 配置模式**（WiFi 未配置时）：手机/电脑连接热点 `deskwong-setup`（无密码），浏览器访问 `http://192.168.4.1`。
3. 在配置后台登录（默认 `admin` / `admin`），**立即修改密码**，填写公司 WiFi SSID/密码并保存。
4. 设备连上内网后，浏览器访问 `http://deskwong.local`（或设备 IP）进入配置后台，配置天气位置、工时 API、AI 用量 API、MQTT、提醒时间等。
5. 换网络/换环境时：**长按 BOOT 约 3 秒**听到提示音，设备**立即**开放 `deskwong-setup` 配网热点——不重启、不清配置，原 WiFi 继续连接；热点仅本次开机内有效。手机连接后在后台改好 WiFi，重启生效。
6. 网页认证（captive portal）网络：连接 `deskwong-setup` 后用手机浏览器打开 `http://192.168.4.1/p/start`，设备会探测门户并把认证页代理到热点内（认证请求从设备自身发出，放行落在设备上）。完成后用 `http://192.168.4.1/p/check` 确认放行状态。

运行时按键：主屏短按 KEY 同步 RaceBox；RaceBox 空闲时长按 KEY 进入/退出日历翻页，翻页中短按 KEY/BOOT 查看上月/下月；普通主屏短按 BOOT 开关天气黄历弹窗。长按 BOOT 仍开启配网热点；断电时按住 BOOT 再按 PWR 上电仍进入刷机模式。

## 外部接口契约（用户侧实现）

| 接口 | 契约 | 文档 |
| --- | --- | --- |
| 工时 API | `GET {base}/worktime/summary?year=&month=`，Bearer Token | `doc/SPECIFICATIONS.md` §2.1 |
| AI 用量 API | `GET {base}/ai/usage`，Bearer Token | `doc/SPECIFICATIONS.md` §2.2 |
| 黄历 API | `GET {url}?date=&location=&key=`，返回 `data.yi`/`data.ji` | `doc/SPECIFICATIONS.md` §2.3 |
| RaceBox MQTT | 发布到 `deskwong/racebox/data`（QoS 1） | `doc/SPECIFICATIONS.md` §4 |
| 天气 | 和风天气 / Open-Meteo | `doc/REQUIREMENTS.md` §4 |

## 开发约定

- 固件框架 **ESP-IDF + LVGL 8.4**；屏幕为单色 1bpp，LVGL 全彩帧缓冲经 flush 转单色刷新。
- 前端 **Svelte + Vite**，构建产物打入 SPIFFS 分区（`/spiffs/www/`）。
- 测试统一从 `test/run.ps1` 执行；测试源、桩和预览工具不散落在业务目录。
- 参考：<https://github.com/waveshareteam/ESP32-S3-RLCD-4.2>、<https://github.com/redgreat/racewong>
