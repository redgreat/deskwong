package main

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

/* DESKWONG_MODE 决定部署形态：full 全量（兼容旧单镜像）、core 国内精简、
 * ai 国外 VPS 纯用量。模式不识别必须启动即失败，避免静默暴露错误的接口面。 */
func TestServiceModeParsing(t *testing.T) {
	t.Setenv("DESKWONG_MODE", "")
	if m, err := serviceMode(); err != nil || m != modeFull {
		t.Fatalf("默认模式应为 full: mode=%q err=%v", m, err)
	}
	t.Setenv("DESKWONG_MODE", "AI")
	if m, err := serviceMode(); err != nil || m != modeAI {
		t.Fatalf("大小写应被归一: mode=%q err=%v", m, err)
	}
	t.Setenv("DESKWONG_MODE", "core")
	if m, err := serviceMode(); err != nil || m != modeCore {
		t.Fatalf("core 模式解析失败: mode=%q err=%v", m, err)
	}
	t.Setenv("DESKWONG_MODE", "bogus")
	if _, err := serviceMode(); err == nil {
		t.Fatal("非法 DESKWONG_MODE 被接受")
	}
}

/* 校验范围随模式裁剪：ai 模式不要求 worktime 段，core 模式不要求 ai_usage 段，
 * 两边都必须有合法的 listen 与设备 Token。 */
func TestValidateConfigModeScopes(t *testing.T) {
	var cfg Config
	cfg.Server.Listen = ":8001"
	cfg.Server.Token = "token"

	if err := validateConfigMode(cfg, modeAI); err == nil {
		t.Fatal("ai 模式下未归一化的 AI 配置应被拒绝")
	}
	normalizeAIConfig(&cfg)
	if err := validateConfigMode(cfg, modeAI); err != nil {
		t.Fatalf("ai 模式合法配置被拒绝: %v", err)
	}

	// core 模式跳过 ai_usage 校验（哪怕段内容非法），但 worktime 超时要合法
	cfg.AIUsage.Providers = []AIProviderConfig{{ID: "bogus"}}
	if err := validateConfigMode(cfg, modeCore); err == nil {
		t.Fatal("core 模式不应跳过 worktime.timeout_sec 校验")
	}
	cfg.Worktime.PingCode.TimeoutSec = 20
	if err := validateConfigMode(cfg, modeCore); err != nil {
		t.Fatalf("core 模式合法配置被拒绝: %v", err)
	}

	if err := validateConfigMode(cfg, modeFull); err == nil {
		t.Fatal("full 模式必须校验 ai_usage 段")
	}
}

/* 路由面随模式裁剪：core 不注册 /ai/usage，ai 不注册工时/黄历/上下文，
 * /health 三种模式都在并回显 mode。 */
func TestRoutesByMode(t *testing.T) {
	var cfg Config
	cfg.Server.Token = "token"
	app := &App{cfg: cfg, client: http.DefaultClient}

	authReq := func(path string) *http.Request {
		r := httptest.NewRequest(http.MethodGet, path, nil)
		r.Header.Set("Authorization", "Bearer token")
		return r
	}
	serve := func(h http.Handler, r *http.Request) *httptest.ResponseRecorder {
		rec := httptest.NewRecorder()
		h.ServeHTTP(rec, r)
		return rec
	}

	core := app.routes(modeCore)
	if rec := serve(core, authReq("/ai/usage")); rec.Code != http.StatusNotFound {
		t.Fatalf("core /ai/usage=%d，应为 404", rec.Code)
	}
	if rec := serve(core, httptest.NewRequest(http.MethodGet, "/almanac", nil)); rec.Code != http.StatusOK {
		t.Fatalf("core /almanac=%d", rec.Code)
	}
	if rec := serve(core, authReq("/worktime/summary")); rec.Code == http.StatusNotFound {
		t.Fatal("core /worktime/summary 不应 404")
	}

	ai := app.routes(modeAI)
	if rec := serve(ai, authReq("/ai/usage")); rec.Code != http.StatusOK {
		t.Fatalf("ai /ai/usage=%d body=%s", rec.Code, rec.Body.String())
	}
	for _, p := range []string{"/worktime/summary", "/almanac", "/voice/context"} {
		if rec := serve(ai, authReq(p)); rec.Code != http.StatusNotFound {
			t.Fatalf("ai %s=%d，应为 404", p, rec.Code)
		}
	}
	if rec := serve(ai, httptest.NewRequest(http.MethodGet, "/health", nil)); rec.Code != http.StatusOK ||
		!strings.Contains(rec.Body.String(), `"mode":"ai"`) {
		t.Fatalf("ai /health=%d body=%s", rec.Code, rec.Body.String())
	}

	full := app.routes(modeFull)
	if rec := serve(full, authReq("/ai/usage")); rec.Code != http.StatusOK {
		t.Fatalf("full /ai/usage=%d", rec.Code)
	}
	if rec := serve(full, httptest.NewRequest(http.MethodGet, "/almanac", nil)); rec.Code != http.StatusOK {
		t.Fatalf("full /almanac=%d", rec.Code)
	}
}
