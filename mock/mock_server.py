#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
deskwong 外部服务 mock（本地联调用）
模拟：工时 API、AI 用量聚合 API、天气 API
契约对齐 doc/rules/03 §2.2 与 doc/rules/05 §2.2

运行：python3 mock_server.py [port]   # 默认 8000
"""
import json
import sys
import time
import datetime
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000


def now_iso():
    return datetime.datetime.now().astimezone().isoformat(timespec="seconds")


def worktime_summary(year, month):
    """契约：doc/rules/03 §2.2"""
    today = datetime.date.today()
    days = []
    recorded = 0.0
    for d in range(1, 29):
        dt = datetime.date(year, month, d)
        # 仅工作日记录 8h
        if dt.weekday() < 5 and dt <= today:
            days.append({"date": dt.isoformat(), "hours": 8.0})
            recorded += 8.0
    expected = 8.0 * 22  # 示例：22 个工作日
    return {
        "code": 0,
        "message": "ok",
        "data": {
            "year": year,
            "month": month,
            "days": days,
            "total_recorded_hours": recorded,
            "total_expected_hours": expected,
        },
    }


def ai_usage():
    """契约：doc/rules/05 §2.2"""
    return {
        "code": 0,
        "message": "ok",
        "data": {
            "updated_at": now_iso(),
            "providers": [
                {"id": "chatgpt_5h", "name": "ChatGPT", "label": "5小时",
                 "window_minutes": 300, "remaining_percent": 72,
                 "resets_at": int(time.time()) + 5 * 3600, "status": "normal"},
                {"id": "chatgpt_weekly", "name": "ChatGPT", "label": "每周",
                 "window_minutes": 10080, "remaining_percent": 44,
                 "resets_at": int(time.time()) + 7 * 86400, "status": "normal"},
                {"id": "deepseek", "name": "DeepSeek", "label": "今日 Token",
                 "value": 1234567, "unit": "tokens", "display": "123.5万", "status": "normal"},
                {"id": "codex", "name": "Codex", "label": "剩余额度",
                 "value": 450, "unit": "credits", "display": "450", "status": "warning"},
                {"id": "mimo", "name": "Mimo", "label": "剩余额度",
                 "value": 120, "unit": "次", "display": "120 次", "status": "normal"},
            ],
        },
    }


def weather():
    """天气（和风风格简化）"""
    return {
        "code": 0,
        "message": "ok",
        "data": {
            "location": "北京",
            "temp": 24,
            "temp_min": 18,
            "temp_max": 28,
            "humidity": 55,
            "text": "多云",
            "wind_dir": "东南风",
            "wind_scale": "2级",
        },
    }


class Handler(BaseHTTPRequestHandler):
    def _send(self, obj):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/worktime/summary":
            qs = {}
            if "?" in self.path:
                for kv in self.path.split("?")[1].split("&"):
                    k, _, v = kv.partition("=")
                    qs[k] = v
            y = int(qs.get("year", datetime.date.today().year))
            m = int(qs.get("month", datetime.date.today().month))
            self._send(worktime_summary(y, m))
        elif path == "/ai/usage":
            self._send(ai_usage())
        elif path == "/weather":
            self._send(weather())
        elif path == "/":
            self._send({"code": 0, "message": "deskwong mock", "data": {
                "endpoints": ["/worktime/summary?year=&month=", "/ai/usage", "/weather"]}})
        else:
            self._send({"code": 404, "message": "not found"})

    def log_message(self, *args):
        print("[mock]", self.path)


if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", PORT), Handler)
    print(f"deskwong mock listening on http://0.0.0.0:{PORT}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
