# deskwong PC 热点脚本：把本机已联网的网卡（公司以太网 / 家里任意）通过
# Windows 移动热点共享成 2.4GHz WiFi，板子在公司/家里连同一个 SSID，无需切换。
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File tools\hotspot.ps1              # 启动
#   powershell -ExecutionPolicy Bypass -File tools\hotspot.ps1 -Action stop    # 停止
#   powershell -ExecutionPolicy Bypass -File tools\hotspot.ps1 -Action status  # 查看状态
#   powershell -ExecutionPolicy Bypass -File tools\hotspot.ps1 -Action install # 写入开机自启
#   powershell -ExecutionPolicy Bypass -File tools\hotspot.ps1 -Action remove  # 移除开机自启
#
# SSID/密码在 tools\hotspot.local.ps1（已 gitignore，不入仓），改完直接重跑生效。
# 详细说明见 tools\hotspot.md。
param(
    [ValidateSet('start', 'stop', 'status', 'install', 'remove')]
    [string]$Action = 'start'
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# ---- 本地配置（gitignored，含密码，不入仓） ----
$localCfg = Join-Path $scriptDir 'hotspot.local.ps1'
if (-not (Test-Path $localCfg)) {
    throw "缺少本地配置 $localCfg`n请复制 hotspot.local.example.ps1 为 hotspot.local.ps1 并填入 SSID/密码。"
}
. $localCfg
if (-not $HotspotSsid -or -not $HotspotPassphrase) { throw "hotspot.local.ps1 里 SSID/密码为空。" }
if ($HotspotPassphrase.Length -lt 8) { throw "密码至少 8 位（Windows 热点要求）。" }

# ---- WinRT 异步转同步 ----
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$null = [Windows.Networking.Connectivity.NetworkInformation, Windows.Networking.Connectivity, ContentType = WindowsRuntime]
$null = [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager, Windows.Networking.NetworkOperators, ContentType = WindowsRuntime]
$null = [Windows.Networking.NetworkOperators.TetheringWiFiBand, Windows.Networking.NetworkOperators, ContentType = WindowsRuntime]

$rtMethods = [System.WindowsRuntimeSystemExtensions].GetMethods()
$asTaskOp = ($rtMethods | Where-Object {
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and
    $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
$asTaskAction = ($rtMethods | Where-Object {
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and
    $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncAction' })[0]

function AwaitOp($op, $type) {
    $t = $asTaskOp.MakeGenericMethod($type).Invoke($null, @($op))
    $t.Wait() | Out-Null
    $t.Result
}
function AwaitAction($action) {
    $t = $asTaskAction.Invoke($null, @($action))
    $t.Wait() | Out-Null
}

# ---- 上游网卡选择：物理网卡优先，跳过 Mihomo/vEthernet 这类虚拟口 ----
function New-TetheringManager {
    if ($HotspotShareProfileName) {
        $profile = [Windows.Networking.Connectivity.NetworkInformation]::GetConnectionProfiles() |
            Where-Object { $_.ProfileName -eq $HotspotShareProfileName } | Select-Object -First 1
        if (-not $profile) { throw "找不到名为 '$HotspotShareProfileName' 的连接配置。" }
        return [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager]::CreateFromConnectionProfile($profile)
    }
    $connected = [Windows.Networking.Connectivity.NetworkInformation]::GetConnectionProfiles() |
        Where-Object { $_.NetworkAdapter -and $_.GetNetworkConnectivityLevel() -ne 'None' }
    $pick = $connected | Where-Object {
        $_.NetworkAdapter.IanaInterfaceType -eq 6 -and $_.ProfileName -notlike 'vEthernet*' } |
        Select-Object -First 1
    if (-not $pick) { $pick = $connected | Where-Object { $_.NetworkAdapter.IanaInterfaceType -eq 71 } | Select-Object -First 1 }
    if (-not $pick) { $pick = [Windows.Networking.Connectivity.NetworkInformation]::GetInternetConnectionProfile() }
    if (-not $pick) { throw "当前没有已联网的网卡（先确认以太网/WiFi 已连接）。" }
    Write-Host "共享来源: $($pick.ProfileName)"
    return [Windows.Networking.NetworkOperators.NetworkOperatorTetheringManager]::CreateFromConnectionProfile($pick)
}

function Stop-Hotspot($manager) {
    if ($manager.TetheringOperationalState -eq 'On') {
        $r = AwaitOp ($manager.StopTetheringAsync()) `
            ([Windows.Networking.NetworkOperators.NetworkOperatorTetheringOperationResult])
        if ($r.Status -ne 'Success') { Write-Warning "停止热点返回 $($r.Status): $($r.AdditionalErrorMessage)" }
    }
}

function Start-Hotspot($manager) {
    # 改配置必须走 ConfigureAccessPointAsync 显式应用：直接改对象属性会被 Start 重置回 Auto
    Stop-Hotspot $manager
    $config = $manager.GetCurrentAccessPointConfiguration()
    $config.Ssid = $HotspotSsid
    $config.Passphrase = $HotspotPassphrase
    try { $config.Band = [Windows.Networking.NetworkOperators.TetheringWiFiBand]::TwoPointFourGigahertz } # 板子只认 2.4GHz
    catch { Write-Warning "设置 2.4GHz 频段失败：$($_.Exception.Message)" }
    try { $config.AuthenticationKind = [Windows.Networking.NetworkOperators.TetheringWiFiAuthenticationKind]::WPA2PSK } # ESP32 走 WPA2
    catch { }
    AwaitAction ($manager.ConfigureAccessPointAsync($config))

    $r = AwaitOp ($manager.StartTetheringAsync()) `
        ([Windows.Networking.NetworkOperators.NetworkOperatorTetheringOperationResult])
    if ($r.Status -ne 'Success') {
        throw "热点启动失败: $($r.Status) $($r.AdditionalErrorMessage)"
    }
    $c = $manager.GetCurrentAccessPointConfiguration()
    if ($c.Band -ne 'TwoPointFourGigahertz') {
        Write-Warning "频段回落为 $($c.Band)，板子（仅 2.4GHz）可能搜不到热点！"
    }
    Write-Host "热点已开启: SSID=$($c.Ssid) Band=$($c.Band) Auth=$($c.AuthenticationKind) 已连设备=$($manager.ClientCount)"
}

switch ($Action) {
    'start'  { Start-Hotspot (New-TetheringManager) }
    'stop'   { Stop-Hotspot (New-TetheringManager); Write-Host "热点已关闭。" }
    'status' {
        $m = New-TetheringManager
        $c = $m.GetCurrentAccessPointConfiguration()
        Write-Host "状态: $($m.TetheringOperationalState) | SSID=$($c.Ssid) | Band=$($c.Band) | 已连设备=$($m.ClientCount)"
    }
    'install' {
        $startup = [Environment]::GetFolderPath('Startup')
        $cmdPath = Join-Path $startup 'deskwong-hotspot.cmd'
        $scriptPath = Join-Path $scriptDir 'hotspot.ps1'
        "@echo off`r\npowershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$scriptPath`" -Action start" |
            Set-Content -Path $cmdPath -Encoding ASCII
        Write-Host "已写入开机自启: $cmdPath"
    }
    'remove' {
        $startup = [Environment]::GetFolderPath('Startup')
        $cmdPath = Join-Path $startup 'deskwong-hotspot.cmd'
        if (Test-Path $cmdPath) { Remove-Item $cmdPath -Force; Write-Host "已移除开机自启。" }
        else { Write-Host "未发现自启项。" }
    }
}
