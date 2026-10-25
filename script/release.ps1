#!/usr/bin/env pwsh
# Release 脚本：自动递增版本 → 推送标签触发 GitHub Actions（测试+构建+推送镜像）
param([Parameter(Position=0)][string]$Version = '')
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

function die($msg) { Write-Host $msg -ForegroundColor Red; exit 1 }
function ok($msg)  { Write-Host $msg -ForegroundColor Green }

if (git status --porcelain) { die '工作区不干净，请先提交或暂存改动。' }
# 拉取标签。两点注意：
#  1) 保留 stderr 输出：原来用 2>$null 会把真实报错（网络不通 / 标签被拒绝覆盖）
#     全部藏掉，屏幕上只剩一句无意义的"拉取标签失败"，完全无法排查；
#  2) 加重试：访问 github.com 偶发瞬时连不上（Connection refused / timeout），
#     重试通常即可恢复，不该直接把脚本判死。
$fetchErr = $null
$fetchOk  = $false
for ($i = 1; $i -le 3; $i++) {
    $fetchErr = git fetch --tags origin 2>&1
    if (-not $LASTEXITCODE) { $fetchOk = $true; break }
    Write-Host "第 $i 次拉取标签失败，2 秒后重试..." -ForegroundColor Yellow
    Start-Sleep -Seconds 2
}
if (-not $fetchOk) {
    Write-Host ($fetchErr -join "`n") -ForegroundColor Red
    if (($fetchErr -join ' ') -match 'would clobber existing tag') {
        die '拉取标签失败：本地与远程标签指向不同提交（历史曾被重写）。
  若确认以本地这份（已清 .zcode）为准，先执行同步：
    git push --force origin --tags'
    }
    die '拉取标签失败：请检查网络连通性（github.com:443）或代理配置。'
}

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
ok "镜像地址：ghcr.io/redgreat/deskwong:$($Version.TrimStart('v')) 与 Quay 机器人所属命名空间下的 deskwong:$($Version.TrimStart('v'))，并更新各自 :latest"
