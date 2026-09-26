#!/usr/bin/env pwsh
# Release 脚本：自动递增版本 → 推送标签触发 GitHub Actions（测试+构建+推送镜像）
param([Parameter(Position=0)][string]$Version = '')
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

function die($msg) { Write-Host $msg -ForegroundColor Red; exit 1 }
function ok($msg)  { Write-Host $msg -ForegroundColor Green }

if (git status --porcelain) { die '工作区不干净，请先提交或暂存改动。' }
git fetch --tags origin 2>$null
if ($LASTEXITCODE) { die '拉取标签失败' }

if ([string]::IsNullOrWhiteSpace($Version)) {
  $latest = git tag --list 'v[0-9]*.[0-9]*.[0-9]*' --sort=-version:refname | Select-Object -First 1
  if (-not $latest) { $Version = 'v0.0.1' }
  elseif ($latest -match '^v(\d+)\.(\d+)\.(\d+)$') {
    $Version = "v$($Matches[1]).$($Matches[2]).$([int]$Matches[3] + 1)"
  }
}
if ($Version -notmatch '^v\d+\.\d+\.\d+$') { die "版本格式错误: $Version（需要 v主版本.次版本.修订号）" }
if (git tag --list $Version) { die "标签 $Version 已存在" }

git tag -a $Version -m "Release $Version"
if ($LASTEXITCODE) { die '创建标签失败' }
git push origin $Version
if ($LASTEXITCODE) { die '推送标签失败' }

ok "已推送 $Version，GitHub Actions 将测试、推送 GHCR 镜像并创建 GitHub Release。"
ok "镜像地址：ghcr.io/redgreat/deskwong:$($Version.TrimStart('v')) 与 :latest"
