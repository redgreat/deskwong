# deskwong

微雪 **ESP32-S3-RLCD-4.2**（全反射屏）桌面摆件固件：电子日历、天气/温湿度、工时统计与本地提醒、RaceBox 轨迹经 MQTT 采集上传、AI 用量展示、Svelte 配置后台。

> 需求与实施计划见 [`doc/`](doc/)（先读 `doc/AI_RULES.md` 与 `doc/plan/00-总计划与路线图.md`）。

## 目录结构

```
deskwong/
├── doc/          # 需求规则（rules/）与实施计划（plan/）
├── firmware/     # ESP-IDF v5.5.1 固件（main/ + components/）
├── web/          # Svelte 配置后台前端（Vite 构建）
├── mock/         # 外部服务 mock（工时/AI用量/天气）
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

## 外部接口契约（用户侧实现）

| 接口 | 契约 | 文档 |
| --- | --- | --- |
| 工时 API | `GET {base}/worktime/summary?year=&month=`，Bearer Token | `doc/rules/03` §2.2 |
| AI 用量 API | `GET {base}/ai/usage`，Bearer Token | `doc/rules/05` §2.2 |
| RaceBox MQTT | 发布到 `deskwong/racebox/data`（QoS 1） | `doc/rules/04` §2.4 |
| 天气 | 和风天气（QWeather） | `doc/rules/00` D2 |

联调期可用本地 mock：`python3 mock/mock_server.py`（`http://<电脑IP>:8000`）。

## 开发约定

- 固件框架 **ESP-IDF + LVGL 8.4**；屏幕为单色 1bpp，LVGL 全彩帧缓冲经 flush 转单色刷新。
- 前端 **Svelte + Vite**，构建产物打入 SPIFFS 分区（`/spiffs/www/`）。
- 需求变更直接改 `doc/rules/`，AI 按 `doc/AI_RULES.md` 路由同步 `doc/plan/`。
- 参考：<https://github.com/waveshareteam/ESP32-S3-RLCD-4.2>、<https://github.com/redgreat/racewong>
