<#
.SYNOPSIS
    测试 AI 用量接口 https://ai.wongcw.cn/ai/usage，并按应用逐个诊断。

.DESCRIPTION
    模拟固件 app_services/http_util.c 的请求方式：
      - 方法 GET
      - 头部 Authorization: Bearer <token>（对应开发板后台配置的 AI 用量密钥）
      - Accept-Encoding: identity、User-Agent: deskwong/1.0

    当前服务端不轮询不缓存，每次请求实时查上游，返回结构是
      data.apps[]（每个应用）→ windows[]（该应用的额度窗口）：
      {"code":0,"message":"ok","data":{"apps":[
        {"id":"chatgpt","name":"ChatGPT","status":"live","observed_at":1789223400,
         "windows":[{"id":"chatgpt_5h","label":"5小时","window_minutes":300,
                     "remaining_percent":72,"resets_at":1789223400}]}]}}

    脚本会输出每个应用的状态与窗口，并明确指出四种典型问题：
      1. 应用根本不在返回里      → 服务端 conf.yml 该应用 enabled: false（或改了没重启）
      2. status=unavailable      → 服务端查上游失败，看 error 字段
      3. status=stale            → 这次查失败了，返回的是上一次成功结果
      4. windows 为空            → 拿不到额度数据，开发板不会画这个应用

.PARAMETER Url
    接口地址，默认 https://ai.wongcw.cn/ai/usage

.PARAMETER Token
    Bearer Token（即开发板后台「AI 用量 → 密钥」）。
    不传则尝试读环境变量 AIUSAGE_TOKEN。

.PARAMETER Raw
    打印接口返回的原始 JSON。

.EXAMPLE
    .\script\test_ai_usage.ps1 -Token "你的token"
    $env:AIUSAGE_TOKEN="你的token"; .\script\test_ai_usage.ps1
    .\script\test_ai_usage.ps1 -Raw
#>
param(
    [string]$Url = "https://ai.wongcw.cn/ai/usage",
    [string]$Token = $env:AIUSAGE_TOKEN,
    [switch]$Raw
)

$ErrorActionPreference = "Stop"
$kExpectedApps = @('chatgpt', 'devin', 'claude', 'mimo')

Write-Host "== 测试 AI 用量接口 ==" -ForegroundColor Cyan
Write-Host "GET $Url"

$headers = @{
    "Accept-Encoding" = "identity"
    "User-Agent"      = "deskwong/1.0"
}
if ($Token) {
    $headers["Authorization"] = "Bearer $Token"
} else {
    Write-Warning "未提供 Token（无 Authorization 头），服务端会返回 401。可用 -Token 或先设置 `$env:AIUSAGE_TOKEN。"
}

# 兼容 PS5.1(WebException) 与 PS7(HttpResponseException)：统一取出状态码与响应体
$status = $null
$body   = $null
try {
    $r = Invoke-WebRequest -Uri $Url -Headers $headers -TimeoutSec 20 -UseBasicParsing -ErrorAction Stop
    $status = [int]$r.StatusCode
    $body   = $r.Content
} catch {
    $ex   = $_.Exception
    $resp = $null
    if ($ex.Response) { $resp = $ex.Response }
    elseif ($ex.InnerException -and $ex.InnerException.Response) { $resp = $ex.InnerException.Response }

    if (-not $resp) {
        Write-Host "请求异常（无法连接 / 超时）: $($ex.Message)" -ForegroundColor Red
        exit 1
    }
    try { $status = [int]$resp.StatusCode } catch {}
    try { $body = $ex.ErrorDetails.Message } catch {}
    if (-not $body -and $resp.Content) {
        try { $body = $resp.Content.ReadAsStringAsync().GetAwaiter().GetResult() } catch {}
    }
    if (-not $body -and $resp.GetResponseStream) {
        try { $body = [System.IO.StreamReader]::new($resp.GetResponseStream()).ReadToEnd() } catch {}
    }
}

Write-Host "HTTP 状态码: $(if ($null -ne $status) { $status } else { '?' })" `
    -ForegroundColor $(if ($status -eq 200) { "Green" } else { "Yellow" })
if ($Raw) { Write-Host "--- 原始响应 ---"; Write-Host $body }

if ($status -ne 200) {
    if ($status -eq 401 -or $status -eq 403) {
        Write-Host "HTTP $status —— 未授权：Token 缺失或与服务端 server.token 不一致" -ForegroundColor Red
    } else {
        Write-Host "HTTP $status —— 接口返回非成功状态码（服务端未起来 / 域名解析问题？）" -ForegroundColor Red
    }
    exit 1
}

if (-not $body) { Write-Host "结果: 2xx 但无响应体 ❌" -ForegroundColor Red; exit 1 }

$json = $body | ConvertFrom-Json -ErrorAction SilentlyContinue
if (-not $json) { Write-Host "响应不是合法 JSON ❌" -ForegroundColor Red; exit 1 }

if ($json.code -and $json.code -ne 0) {
    Write-Host "业务返回错误: code=$($json.code) message=$($json.message)" -ForegroundColor Red
    exit 1
}

$apps = @()
if ($json.data -and $json.data.apps) {
    $apps = @($json.data.apps)
} elseif ($json.data -and $json.data.providers) {
    Write-Host "服务端返回的是旧格式 data.providers —— 镜像还没更新到最新版，请先 redeploy" -ForegroundColor Yellow
    $apps = @($json.data.providers)
} else {
    Write-Host "JSON 合法，但缺少 data.apps 字段 ⚠️" -ForegroundColor Yellow
    if (-not $Raw) { Write-Host $body }
    exit 2
}

Write-Host "返回应用数: $($apps.Count)" -ForegroundColor $(if ($apps.Count -gt 0) { "Green" } else { "Red" })

function Format-Reset([int64]$ts) {
    if (-not $ts -or $ts -le 0) { return "--:--" }
    return ([DateTimeOffset]::FromUnixTimeSeconds($ts).ToLocalTime()).ToString("MM-dd HH:mm")
}

$problems = 0
foreach ($a in $apps) {
    $name   = $(if ($a.name) { $a.name } elseif ($a.id) { $a.id } else { "(无名)" })
    $state  = $(if ($a.status) { $a.status } else { "(无状态)" })
    $badge  = switch ($state) {
        'live'        { 'Green'; break }
        'cached'      { 'Cyan'; break }
        'stale'       { 'Yellow'; break }
        default       { 'Red' }
    }
    $observed = if ($a.observed_at) { Format-Reset ([int64]$a.observed_at) } else { "-" }
    Write-Host ("`n[{0}] status={1}  采样={2}" -f $name, $state, $observed) -ForegroundColor $badge

    $wins = @()
    if ($a.windows) { $wins = @($a.windows) }
    if ($wins.Count -eq 0) {
        Write-Host "    ⚠ 没有额度窗口（windows 为空），开发板不会显示这个应用" -ForegroundColor Yellow
        $problems++
    } else {
        foreach ($w in $wins) {
            $rem = if ($null -ne $w.remaining_percent) { "{0}%" -f $w.remaining_percent } else { "-%" }
            Write-Host ("    - {0,-6} 剩余 {1,-6} 重置 {2}" -f $w.label, $rem, (Format-Reset ([int64]$w.resets_at)))
        }
    }
    if ($state -eq 'unavailable') {
        Write-Host "    ⚠ 服务端这次没查到：服务器 conf.yml 里这个应用 enabled=false，或上游查询失败" -ForegroundColor Red
        $problems++
    } elseif ($state -eq 'stale') {
        Write-Host "    ⚠ 本次查询失败，上面是上一次成功的结果" -ForegroundColor Yellow
        $problems++
    }
    if ($a.error) {
        Write-Host "    服务端原因: $($a.error)" -ForegroundColor Yellow
    }
}

$missing = @()
if ($apps.Count -lt $kExpectedApps.Count) {
    $returned = @($apps | ForEach-Object { $_.id })
    $missing = @($kExpectedApps | Where-Object { $returned -notcontains $_ })
}
if ($missing.Count -gt 0) {
    Write-Host ""
    Write-Host "未在返回中的应用: $($missing -join ', ')" -ForegroundColor Yellow
    Write-Host "  → 服务端 conf.yml 的 ai_usage.providers 里这些应用是 enabled: false；" -ForegroundColor Yellow
    Write-Host "    改成 true 后用 script/redeploy.sh 重新部署才会生效（conf 是只读挂载，改完必须重启容器）。" -ForegroundColor Yellow
}

Write-Host ""
if ($problems -gt 0) {
    Write-Host "结果: 接口通了，但有 $problems 个应用没有可用数据 ❌" -ForegroundColor Red
    Write-Host "服务器排查: cd 到 docker-compose.yml 目录，然后" -ForegroundColor Cyan
    Write-Host "  docker compose logs --tail=200 deskwong-service | grep -iE 'ai usage|devin|mimo|claude'" -ForegroundColor Cyan
    exit 3
}
Write-Host "结果: 所有返回的应用都有实时数据 ✅" -ForegroundColor Green
