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
$worktime = Join-Path $PSScriptRoot "firmware/worktime"
$worktimeExe = Join-Path ([IO.Path]::GetTempPath()) ("deskwong-worktime-test-" + [guid]::NewGuid().ToString("N") + ".exe")
try {
    & gcc -std=c11 -O0 -g -I (Join-Path $worktime "stubs") -I $services (Join-Path $worktime "worktime_test.c") (Join-Path $worktime "stubs/http_stub.c") (Join-Path $services "worktime_service.c") (Join-Path $services "holiday_service.c") (Join-Path $services "calendar_service.c") -o $worktimeExe
    if ($LASTEXITCODE) { throw "Worktime expected hours host test compilation failed" }
    & $worktimeExe
    if ($LASTEXITCODE) { throw "Worktime expected hours host test failed" }
} finally {
    if (Test-Path -LiteralPath $worktimeExe) { Remove-Item -LiteralPath $worktimeExe -Force }
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
if ($voiceSource -notmatch 'followup_waiting \? 10000 : 30000' -or
    $voiceSource -notmatch 'stage=%s elapsed_ms=' -or
    $voiceSource -notmatch 'retry_seconds\[\].*10, 30, 60, 300') {
    throw "Voice lifecycle timeout/backoff inspection failed"
}
Write-Host "Voice lifecycle timeout/backoff inspection passed"
$mainSource = Get-Content -Raw -LiteralPath (Join-Path $repo "firmware/main/main.cpp")
$mainScreenSource = Get-Content -Raw -LiteralPath (Join-Path $repo "firmware/components/app_ui/main_screen.cpp")
if ($mainSource -notmatch 'bool active_dialog = voice_state == VOICE_CONNECTING' -or
    $mainSource -match 'g_voice_ui_error_until' -or
    $mainScreenSource -match 'LV_SYMBOL_BELL|LV_SYMBOL_AUDIO|LV_SYMBOL_WARNING' -or
    $mainScreenSource -notmatch 's_voice_icon_bits' -or
    $mainScreenSource -notmatch 'connected \? lv_color_black\(\) : lv_color_white\(\)') {
    throw "Voice topbar status / non-blocking failure UI inspection failed"
}
Write-Host "Voice topbar status / non-blocking failure UI inspection passed"
$wakeSource = Get-Content -Raw -LiteralPath (Join-Path $services "voice_wake.c")
if ($wakeSource -notmatch 'set_det_threshold' -or
    $wakeSource -notmatch 'WAKE_COOLDOWN_US' -or
    $wakeSource -notmatch 'cooldown_drops' -or
    $wakeSource -notmatch 'wake threshold update configured=') {
    throw "Wake threshold/cooldown inspection failed"
}
Write-Host "Wake threshold/cooldown inspection passed"
$audioSource = Get-Content -Raw -LiteralPath (Join-Path $services "audio_service.c")
$audioHeader = Get-Content -Raw -LiteralPath (Join-Path $services "audio_service.h")
if ($voiceSource -match 'audio_service_set_volume\s*\(\s*100\s*\)' -or
    $voiceSource -notmatch 'audio_service_voice_begin\s*\(s\.cfg \? s\.cfg->voice_volume : 60\)' -or
    $voiceSource -notmatch 'audio_service_voice_end\s*\(\)' -or
    $audioHeader -notmatch 'audio_service_voice_begin' -or
    $audioHeader -notmatch 'audio_service_voice_end' -or
    $audioSource -notmatch 'static int s_system_volume = 60' -or
    $audioSource -notmatch 's_voice_active \? s_voice_volume : s_system_volume' -or
    $audioSource -notmatch 'void audio_service_set_system_volume\(int vol\)' -or
    $audioHeader -notmatch 'audio_service_set_system_volume') {
    throw "Voice-only volume / wake threshold UI inspection failed"
}
Write-Host "Voice-only volume / wake threshold UI inspection passed"

$configHeader = Get-Content -Raw -LiteralPath (Join-Path $repo "firmware/components/app_config/app_config.h")
$configSource = Get-Content -Raw -LiteralPath (Join-Path $repo "firmware/components/app_config/app_config.cpp")
$httpSource = Get-Content -Raw -LiteralPath (Join-Path $repo "firmware/components/app_net/http_server.cpp")
$webSource = Get-Content -Raw -LiteralPath (Join-Path $repo "web/src/pages/Dashboard.svelte")
if ($mainSource -notmatch 'worktime_expected_hours_elapsed\(wt->year, wt->month, now->day\)' -or
    $mainSource -notmatch 'snap\.worktime\.expected_hours = display_expected_hours' -or
    $configHeader -notmatch 'remind_sign_hold_seconds' -or
    $configHeader -notmatch 'remind_worktime_hold_seconds' -or
    $configSource -notmatch '"rem_sign_hd"' -or
    $configSource -notmatch '"rem_wt_hd"' -or
    $httpSource -notmatch '"remind_sign_hold_seconds"' -or
    $httpSource -notmatch '"remind_worktime_hold_seconds"' -or
    $httpSource -notmatch 'audio_service_set_ring_volume\(pending\.voice_volume\)' -or
    $webSource -notmatch 'remind_sign_hold_seconds' -or
    $webSource -notmatch 'remind_worktime_hold_seconds' -or
    $configHeader -notmatch 'system_volume' -or
    $configSource -notmatch '"sys_vol"' -or
    $httpSource -notmatch '"system_volume"' -or
    $httpSource -notmatch 'audio_service_set_system_volume\(pending\.system_volume\)' -or
    $webSource -notmatch 'system_volume' -or
    $webSource -notmatch 'bind:value=\{cfg\.system_volume\}') {
    throw "Worktime elapsed expected hours / reminder hold config inspection failed"
}
Write-Host "Worktime elapsed expected hours / reminder hold config inspection passed"
if ($mainSource -notmatch 'g_remind_shown_tick' -or
    $mainSource -notmatch 'main_screen_hide_remind\(\)' -or
    $mainSource -notmatch 'AUDIO_CUE_SIGNIN' -or
    $mainSource -notmatch 'AUDIO_CUE_SIGNOUT' -or
    $mainSource -notmatch 'AUDIO_CUE_WORKTIME' -or
    $mainSource -match 'audio_service_beep\(\);' -or
    $audioHeader -notmatch 'audio_service_set_ring_volume' -or
    $audioSource -notmatch 's_ring_volume' -or
    $audioSource -notmatch 'AUDIO_CUE_SIGNIN' -or
    $audioSource -notmatch 'AUDIO_CUE_SIGNOUT' -or
    $audioSource -notmatch 'AUDIO_CUE_WORKTIME') {
    throw "Reminder ring melody / auto dismiss inspection failed"
}
Write-Host "Reminder ring melody / auto dismiss inspection passed"

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
& $uiExe (Join-Path $uiOut "voice-ok.pgm") 9 voice-ok
& $uiExe (Join-Path $uiOut "voice-fail.pgm") 9 voice-fail
& $uiExe (Join-Path $uiOut "battery-full.pgm") 9 full
& $uiExe (Join-Path $uiOut "battery-low.pgm") 9 low
& $uiExe (Join-Path $uiOut "voice-off.pgm") 9 voice-off
& $uiExe (Join-Path $uiOut "ap-voice.pgm") 9 ap-voice
if ($LASTEXITCODE) { throw "UI preview rendering failed" }
$statusCheck = Join-Path $uiSource "status_bar_check.py"
foreach ($state in @("battery-full", "battery-low", "normal", "empty", "voice-ok", "voice-off", "ap-voice")) {
    & python $statusCheck (Join-Path $uiOut "$state.pgm") --assert
    if ($LASTEXITCODE) { throw "Top bar layout check failed: $state" }
}
Write-Host "Top bar layout check passed"
$forbidden = @(
    Get-ChildItem -LiteralPath (Join-Path $repo "service") -Filter "*_test.go" -File -ErrorAction SilentlyContinue
    Get-ChildItem -LiteralPath (Join-Path $repo "firmware/components") -Filter "*_test.*" -File -Recurse -ErrorAction SilentlyContinue
)
if ($forbidden.Count) { throw "Tests must live under test/: $($forbidden.FullName -join ', ')" }
Write-Host "All centralized tests passed."
