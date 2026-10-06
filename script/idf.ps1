# deskwong idf.py wrapper: same env setup as flash.ps1, runs any idf.py subcommand.
# Usage: .\script\idf.ps1 reconfigure
#        .\script\idf.ps1 -Port COM7 monitor
#        .\script\idf.ps1 build
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$CmdArgs,
    [string]$Port = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$idf  = "C:\Espressif\frameworks\esp-idf-v5.5.1"
$py   = "C:\Users\wangcw\.espressif\python_env\idf5.5_py3.13_env\Scripts\python.exe"

if (-not (Test-Path $idf)) { throw "ESP-IDF not found: $idf" }
if (-not (Test-Path $py))  { throw "IDF python env not found: $py" }

$env:IDF_PATH       = $idf
$env:IDF_TOOLS_PATH = "C:\Users\wangcw\.espressif"
$env:PYTHONUTF8     = "1"
$env:PYTHONIOENCODING = "utf-8"

# MSYS-origin terminals make idf.py exit silently; drop those env vars (see flash.ps1)
Remove-Item Env:MSYSTEM         -ErrorAction SilentlyContinue
Remove-Item Env:MSYS2_PATH_TYPE -ErrorAction SilentlyContinue
Remove-Item Env:SHELL           -ErrorAction SilentlyContinue

# full reconfigure needs the cross toolchain on PATH
$toolchain = Get-ChildItem "$env:IDF_TOOLS_PATH\tools\xtensa-esp-elf\*\xtensa-esp-elf\bin" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if ($toolchain) {
    $env:PATH = "$($toolchain.FullName);$env:PATH"
} else {
    Write-Host "WARN: xtensa toolchain not found under $env:IDF_TOOLS_PATH\tools" -ForegroundColor Yellow
}

$allArgs = @()
if ($Port)    { $allArgs += @("-p", $Port) }
if ($CmdArgs) { $allArgs += $CmdArgs }

Set-Location "$root\firmware"
& $py "$idf\tools\idf.py" @allArgs
exit $LASTEXITCODE
