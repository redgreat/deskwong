# deskwong one-click build + flash (Windows PowerShell)
# Usage: .\script\flash.ps1 -Port COM7
#        .\script\flash.ps1 -Port COM7 -AppOnly   (skip web/SPIFFS)
param(
    [string]$Port = "",
    [switch]$AppOnly
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$idf  = "C:\Espressif\frameworks\esp-idf-v5.5.1"
$py   = "C:\Users\wangcw\.espressif\python_env\idf5.5_py3.13_env\Scripts\python.exe"

if (-not (Test-Path $idf)) { throw "ESP-IDF not found: $idf" }
if (-not (Test-Path $py))  { throw "IDF python env not found: $py" }

$env:IDF_PATH       = $idf
$env:IDF_TOOLS_PATH = "C:\Users\wangcw\.espressif"
# 避免 idf.py 在 GBK 控制台下因特殊字符抛 UnicodeEncodeError
$env:PYTHONUTF8     = "1"
$env:PYTHONIOENCODING = "utf-8"

# 首次配置（或 build/ 被删除后重新全量配置）时，CMake 需要从 PATH 找到交叉编译器
$toolchain = Get-ChildItem "$env:IDF_TOOLS_PATH\tools\xtensa-esp-elf\*\xtensa-esp-elf\bin" `
    -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if ($toolchain) {
    $env:PATH = "$($toolchain.FullName);$env:PATH"
} else {
    Write-Host "WARN: xtensa toolchain not found under $env:IDF_TOOLS_PATH\tools" -ForegroundColor Yellow
}

if (-not $Port) {
    $ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    if (-not $ports) { throw "No serial port detected. Connect the board first." }
    $Port = $ports[-1]
    Write-Host "Auto-selected port: $Port (available: $($ports -join ', '))" -ForegroundColor Yellow
}

Write-Host "==> Building web" -ForegroundColor Cyan
Push-Location "$root\web"
npm run build
if ($LASTEXITCODE -ne 0) { Pop-Location; throw "web build failed" }
Pop-Location

Write-Host "==> Building firmware" -ForegroundColor Cyan
Push-Location "$root\firmware"
& $py "$idf\tools\idf.py" build
if ($LASTEXITCODE -ne 0) { Pop-Location; throw "firmware build failed" }

$spiffsBin = "$root\firmware\spiffs_www.bin"
if (-not $AppOnly) {
    Write-Host "==> Generating SPIFFS image" -ForegroundColor Cyan
    # 固件读的是 /spiffs/www/<file>，所以必须把 dist 放进 www/ 子目录再打包
    $stage = "$root\firmware\build\spiffs_stage"
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Path "$stage\www" | Out-Null
    Copy-Item -Path "$root\web\dist\*" -Destination "$stage\www" -Recurse -Force
    & $py "$idf\components\spiffs\spiffsgen.py" 0x7E0000 $stage $spiffsBin
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "spiffsgen failed" }
}

Write-Host "==> Flashing firmware to $Port" -ForegroundColor Cyan
& $py "$idf\tools\idf.py" -p $Port flash
if ($LASTEXITCODE -ne 0) { Pop-Location; throw "firmware flash failed" }

if (-not $AppOnly) {
    Write-Host "==> Flashing SPIFFS (web assets)" -ForegroundColor Cyan
    & $py -m esptool --chip esp32s3 -p $Port -b 460800 --before default_reset --after hard_reset `
        write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x820000 $spiffsBin
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "spiffs flash failed" }
}
Pop-Location

Write-Host "==> Done. Monitor with: & '$py' '$idf\tools\idf.py' -p $Port monitor" -ForegroundColor Green
