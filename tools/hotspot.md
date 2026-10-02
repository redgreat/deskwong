# PC 热点（deskwong 板子专用 WiFi）

把本机已联网的网卡共享成 2.4GHz WiFi（Windows 11 移动热点），SSID 固定为
`wangcw`，板子在公司/家里连同一个名字，**无需切换配置**。

- 公司：热点上游 = 有线以太网（已认证），板子经 PC 出网，**完全绕开公司 WiFi 的
  5GHz/认证限制**。
- 家里：热点上游 = PC 当时的联网网卡，自动探测，无需改脚本。

## 文件说明

| 文件 | 是否入仓 | 说明 |
| --- | --- | --- |
| `tools/hotspot.ps1` | 是 | 主脚本，不含任何密码 |
| `tools/hotspot.local.example.ps1` | 是 | 配置模板 |
| `tools/hotspot.local.ps1` | **否（gitignore）** | 真实 SSID/密码，只存在本机 |

## 首次配置

1. 复制模板：`copy tools\hotspot.local.example.ps1 tools\hotspot.local.ps1`
2. 编辑 `hotspot.local.ps1` 填入 SSID 和密码（密码至少 8 位）。
   `$HotspotShareProfileName` 留空即可（自动选当前联网的网卡）。

## 日常使用

```bat
:: 启动热点
powershell -ExecutionPolicy Bypass -File tools\hotspot.ps1

:: 查看状态（运行中/已连设备数）
powershell -ExecutionPolicy Bypass -File tools\hotspot.ps1 -Action status

:: 关闭热点（不想暴露 SSID 时随手关）
powershell -ExecutionPolicy Bypass -File tools\hotspot.ps1 -Action stop
```

改了 `hotspot.local.ps1` 后直接重新运行 start 即可，脚本会自动重启热点应用新配置。

## 开机自启

```bat
powershell -ExecutionPolicy Bypass -File tools\hotspot.ps1 -Action install
```

会在当前用户的"启动"文件夹写入 `deskwong-hotspot.cmd`（登录后静默拉起热点）。
取消自启用 `-Action remove`，或直接删那个 cmd 文件。

## 建议的一次性系统设置

设置 → 网络和 Internet → 移动热点 → 关闭 **"当没有设备连接时自动关闭移动热点"**。
否则板子断连几分钟后 Windows 会把热点关掉。

## 板子侧（一次性）

长按 BOOT 3 秒 → 手机连 `deskwong-setup` → 后台 `http://192.168.4.1` → WiFi 填
热点名/密码 → 保存 → 系统重启。之后回家/到公司只要 PC 在线，板子自动连。

## 已知限制（实测结论，2026-10-02）

- **隐藏 SSID 做不到**：本机 WinRT 热点配置只有 `Ssid/Passphrase/Band/AuthenticationKind`
  四个属性，系统没有"隐藏"选项；老的 `netsh hostednetwork` 在本机网卡（AX211）驱动
  也不支持（承载网络：否）。所以 `wangcw` 会出现在其他设备的 WiFi 列表里，
  安全性依赖 WPA2 密码；介意暴露时用 `-Action stop` 随手关。
- 频段固定 **2.4GHz**（脚本强制设置），因为 ESP32-S3 只支持 2.4GHz；同时这也意味着
  手机若连这个热点会略慢，属于预期行为。
- 板子侧固件已支持连接**隐藏 SSID**（全信道扫描），将来若改用可隐藏的 AP 无需改板子。
- 上游是 WiFi（家里）时走 Wi-Fi Direct 并发共享，个别环境吞吐较低；公司以太网上游无此问题。
