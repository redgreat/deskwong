param([switch]$ServiceOnly)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$service = Join-Path $repo "service"
$copied = @()
try {
    Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot "service") -Filter "*_test.go" | ForEach-Object {
        $target = Join-Path $service $_.Name
        Copy-Item -LiteralPath $_.FullName -Destination $target
        $copied += $target
    }
    Push-Location $service
    try {
        go test ./...
        if ($LASTEXITCODE) { throw "go test failed" }
        go vet ./...
        if ($LASTEXITCODE) { throw "go vet failed" }
    } finally { Pop-Location }
} finally {
    foreach ($path in $copied) { if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force } }
}
if ($ServiceOnly) { exit 0 }

$racebox = Join-Path $PSScriptRoot "firmware/racebox"
$raceboxExe = Join-Path ([IO.Path]::GetTempPath()) ("deskwong-racebox-test-" + [guid]::NewGuid().ToString("N") + ".exe")
try {
    & gcc -std=c11 -O0 -g -I (Join-Path $racebox "stubs") -I (Join-Path $repo "firmware/components/app_services") (Join-Path $racebox "racebox_test.c") -o $raceboxExe
    if ($LASTEXITCODE) { throw "RaceBox host test compilation failed" }
    & $raceboxExe
    if ($LASTEXITCODE) { throw "RaceBox host test failed" }
} finally {
    if (Test-Path -LiteralPath $raceboxExe) { Remove-Item -LiteralPath $raceboxExe -Force }
}
$services = Join-Path $repo "firmware/components/app_services"
$holiday = Join-Path $PSScriptRoot "firmware/holiday"
$holidayExe = Join-Path ([IO.Path]::GetTempPath()) ("deskwong-holiday-test-" + [guid]::NewGuid().ToString("N") + ".exe")
try {
    & gcc -std=c11 -O0 -g -I $services (Join-Path $holiday "holiday_test.c") (Join-Path $services "holiday_service.c") (Join-Path $services "calendar_service.c") -o $holidayExe
    if ($LASTEXITCODE) { throw "Holiday host test compilation failed" }
    & $holidayExe
    if ($LASTEXITCODE) { throw "Holiday host test failed" }
} finally {
    if (Test-Path -LiteralPath $holidayExe) { Remove-Item -LiteralPath $holidayExe -Force }
}
$timeUtil = Join-Path $PSScriptRoot "firmware/time"
$timeExe = Join-Path ([IO.Path]::GetTempPath()) ("deskwong-time-util-test-" + [guid]::NewGuid().ToString("N") + ".exe")
try {
    & gcc -std=c11 -O0 -g -I $services (Join-Path $timeUtil "time_test.c") (Join-Path $services "time_util.c") -o $timeExe
    if ($LASTEXITCODE) { throw "Time util host test compilation failed" }
    & $timeExe
    if ($LASTEXITCODE) { throw "Time util host test failed" }
} finally {
    if (Test-Path -LiteralPath $timeExe) { Remove-Item -LiteralPath $timeExe -Force }
}
$weatherHour = Join-Path $PSScriptRoot "firmware/weather_hour"
$weatherHourExe = Join-Path ([IO.Path]::GetTempPath()) ("deskwong-weather-hour-test-" + [guid]::NewGuid().ToString("N") + ".exe")
try {
    & gcc -std=c11 -O0 -g -I $services (Join-Path $weatherHour "weather_hour_test.c") -o $weatherHourExe
    if ($LASTEXITCODE) { throw "Weather hourly host test compilation failed" }
    & $weatherHourExe
    if ($LASTEXITCODE) { throw "Weather hourly host test failed" }
} finally {
    if (Test-Path -LiteralPath $weatherHourExe) { Remove-Item -LiteralPath $weatherHourExe -Force }
}
$wifiReason = Join-Path $PSScriptRoot "firmware/wifi_reason"
$wifiExe = Join-Path ([IO.Path]::GetTempPath()) ("deskwong-wifi-reason-test-" + [guid]::NewGuid().ToString("N") + ".exe")
try {
    & gcc -std=c11 -O0 -g -I (Join-Path $repo "firmware/components/app_net") (Join-Path $wifiReason "wifi_reason_test.c") (Join-Path $repo "firmware/components/app_net/wifi_reason.c") -o $wifiExe
    if ($LASTEXITCODE) { throw "WiFi reason host test compilation failed" }
    & $wifiExe
    if ($LASTEXITCODE) { throw "WiFi reason host test failed" }
} finally {
    if (Test-Path -LiteralPath $wifiExe) { Remove-Item -LiteralPath $wifiExe -Force }
}
$portalRw = Join-Path $PSScriptRoot "firmware/portal_rewrite"
$portalExe = Join-Path ([IO.Path]::GetTempPath()) ("deskwong-portal-rewrite-test-" + [guid]::NewGuid().ToString("N") + ".exe")
try {
    & gcc -std=c11 -O0 -g -I (Join-Path $repo "firmware/components/app_net") (Join-Path $portalRw "portal_rewrite_test.c") (Join-Path $repo "firmware/components/app_net/portal_rewrite.c") -o $portalExe
    if ($LASTEXITCODE) { throw "Portal rewrite host test compilation failed" }
    & $portalExe
    if ($LASTEXITCODE) { throw "Portal rewrite host test failed" }
} finally {
    if (Test-Path -LiteralPath $portalExe) { Remove-Item -LiteralPath $portalExe -Force }
}
$fontAudit = Join-Path $repo "tools/audit_ui_fonts.py"
& python $fontAudit
if ($LASTEXITCODE) { throw "UI font audit failed" }

$voiceSource = Get-Content -Raw -LiteralPath (Join-Path $services "voice_service.c")
$autoListenCount = ([regex]::Matches($voiceSource, 'send_listen\("start",\s*"auto"\)')).Count
if ($autoListenCount -ne 2 -or $voiceSource -match 'send_listen\("start",\s*"manual"\)') {
    throw "Voice listen sequence must use auto mode for initial and TTS follow-up listening"
}
Write-Host "Voice auto-listen protocol inspection passed"
if ($voiceSource -notmatch 'listening idle for 30s' -or
    $voiceSource -notmatch 'retry_seconds\[\].*10, 30, 60, 300') {
    throw "Voice lifecycle timeout/backoff inspection failed"
}
Write-Host "Voice lifecycle timeout/backoff inspection passed"

& node (Join-Path $PSScriptRoot "web/ota_test.mjs")
if ($LASTEXITCODE) { throw "Web OTA image inspection failed" }
& node (Join-Path $PSScriptRoot "web/login_test.mjs")
if ($LASTEXITCODE) { throw "Web login defaults inspection failed" }

$uiSource = Join-Path $PSScriptRoot "ui_preview"
$uiBuild = Join-Path $repo "build/ui_preview"
& cmake -S $uiSource -B $uiBuild
if ($LASTEXITCODE) { throw "UI preview configure failed" }
& cmake --build $uiBuild --config Release
if ($LASTEXITCODE) { throw "UI preview build failed" }
$uiExe = Join-Path $uiBuild "ui_preview.exe"
if (!(Test-Path -LiteralPath $uiExe)) { $uiExe = Join-Path $uiBuild "Release/ui_preview.exe" }
$uiOut = Join-Path $repo "build/ui_preview/states"
New-Item -ItemType Directory -Force -Path $uiOut | Out-Null
& $uiExe (Join-Path $uiOut "normal.pgm") 9
& $uiExe (Join-Path $uiOut "six-row.pgm") 8
& $uiExe (Join-Path $uiOut "empty.pgm") 9 empty
& $uiExe (Join-Path $uiOut "sync.pgm") 9 sync
& $uiExe (Join-Path $uiOut "weather.pgm") 10 weather
& $uiExe (Join-Path $uiOut "weather-empty.pgm") 10 weather-empty
& $uiExe (Join-Path $uiOut "weather-overflow.pgm") 10 weather-overflow
& $uiExe (Join-Path $uiOut "cal.pgm") 9 cal
& $uiExe (Join-Path $uiOut "voice.pgm") 9 voice
if ($LASTEXITCODE) { throw "UI preview rendering failed" }
$forbidden = @(
    Get-ChildItem -LiteralPath (Join-Path $repo "service") -Filter "*_test.go" -File -ErrorAction SilentlyContinue
    Get-ChildItem -LiteralPath (Join-Path $repo "firmware/components") -Filter "*_test.*" -File -Recurse -ErrorAction SilentlyContinue
)
if ($forbidden.Count) { throw "Tests must live under test/: $($forbidden.FullName -join ', ')" }
Write-Host "All centralized tests passed."
