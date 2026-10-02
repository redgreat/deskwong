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
$forbidden = @(
    Get-ChildItem -LiteralPath (Join-Path $repo "service") -Filter "*_test.go" -File -ErrorAction SilentlyContinue
    Get-ChildItem -LiteralPath (Join-Path $repo "firmware/components") -Filter "*_test.*" -File -Recurse -ErrorAction SilentlyContinue
)
if ($forbidden.Count) { throw "Tests must live under test/: $($forbidden.FullName -join ', ')" }
Write-Host "All centralized tests passed."
