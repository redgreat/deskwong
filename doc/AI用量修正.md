 Write-Host "=== /health ==="; curl.exe -s -m 15 https://ai.wongcw.cn/health; Write-Host; Write-Host "=== /ai/usage Bearer AtSZyt ==="; curl.exe -s -m 30 -H "Authorization: Bearer AtSZyt6eez4JRM4u2clkryP1PROxjWxI" https://ai.wongcw.cn/ai/usage; Write-Host
 
chmod 644 /home/wangcw/.codex/auth.json
