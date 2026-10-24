<#
.SYNOPSIS
    测试 AI 用量接口 https://ai.wongcw.cn/ai/usage 是否请求成功。

.DESCRIPTION
    模拟固件 app_services/http_util.c 的请求方式：
      - 方法 GET
      - 头部 Authorization: Bearer <token>（对应后台配置 aiusage_token）
      - Accept-Encoding: identity、User-Agent: deskwong/1.0
    校验 HTTP 状态码 + 返回 JSON 是否含 data.providers，并列出各套餐剩余额度。
    同时识别业务层错误（如 code=1001 unauthorized）。

.PARAMETER Url
    接口地址，默认 https://ai.wongcw.cn/ai/usage

.PARAMETER Token
    Bearer Token（即后台配置的「AI 用量 Token / aiusage_token」）。
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

Write-Host "== 测试 AI 用量接口 ==" -ForegroundColor Cyan
Write-Host "GET $Url"

# 与固件 http_util.c 保持一致，避免服务端按 gzip 默认行为处理
$headers = @{
    "Accept-Encoding" = "identity"
    "User-Agent"      = "deskwong/1.0"
}
if ($Token) {
    $headers["Authorization"] = "Bearer $Token"
} else {
    Write-Warning "未提供 Token（无 Authorization 头），服务端可能返回 401。可用 -Token 或先设置 `$env:AIUSAGE_TOKEN。"
}

# 兼容 PS5.1(WebException) 与 PS7(HttpResponseException)：统一取出状态码与响应体
$status = $null
$body   = $null
try {
    $r = Invoke-WebRequest -Uri $Url -Headers $headers -TimeoutSec 15 -UseBasicParsing -ErrorAction Stop
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
    # PS7: HttpResponseException.Response 是 HttpResponseMessage，用 Content 读取
    try { $body = $ex.ErrorDetails.Message } catch {}
    if (-not $body -and $resp.Content) {
        try { $body = $resp.Content.ReadAsStringAsync().GetAwaiter().GetResult() } catch {}
    }
    # PS5.1: WebException.Response 是 HttpWebResponse，用 GetResponseStream 读取
    if (-not $body -and $resp.GetResponseStream) {
        try { $body = [System.IO.StreamReader]::new($resp.GetResponseStream()).ReadToEnd() } catch {}
    }
}

Write-Host "HTTP 状态码: $(if ($status -ne $null) { $status } else { '?' })" `
    -ForegroundColor $(if ($status -eq 200) { "Green" } else { "Yellow" })
if ($Raw) { Write-Host "--- 原始响应 ---"; Write-Host $body }

# 非 2xx：通常是未带 Token / Token 无效，直接按状态码给出可读结论
if ($status -ne 200) {
    if ($status -eq 401 -or $status -eq 403) {
        Write-Host "HTTP $status —— 未授权：需要带 Token（用 -Token 传入后台配置的 aiusage_token）" -ForegroundColor Red
    } else {
        Write-Host "HTTP $status —— 接口返回非成功状态码" -ForegroundColor Red
    }
    exit 1
}

if (-not $body) {
    Write-Host "结果: 2xx 但无响应体 ❌" -ForegroundColor Red
    exit 1
}

$json = $body | ConvertFrom-Json -ErrorAction SilentlyContinue
if (-not $json) {
    Write-Host "响应不是合法 JSON ❌" -ForegroundColor Red
    exit 1
}

# 业务层错误（例如 {"code":1001,"message":"unauthorized"}）
if ($json.code -and $json.code -ne 0) {
    Write-Host "业务返回错误: code=$($json.code) message=$($json.message)" -ForegroundColor Red
    exit 1
}

if ($json.data -and $json.data.providers) {
    $providers = @($json.data.providers)
    Write-Host "JSON 合法，含 data.providers，共 $($providers.Count) 个套餐" -ForegroundColor Green
    foreach ($p in $providers) {
        $name = if ($p.name) { $p.name } elseif ($p.id) { $p.id } else { "(无名)" }
        $rem  = if ($null -ne $p.remaining_percent) { "{0}%" -f $p.remaining_percent } else { "-" }
        $lbl  = if ($p.label) { "（$p.label）" } else { "" }
        Write-Host ("  - {0}{1}  剩余 {2}" -f $name, $lbl, $rem)
    }
    Write-Host "结果: 请求成功 ✅" -ForegroundColor Green
} else {
    Write-Host "JSON 合法，但缺少 data.providers 字段 ⚠️" -ForegroundColor Yellow
    if (-not $Raw) { Write-Host $body }
    exit 2
}
