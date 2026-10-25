package main

import (
	"net/http"
	"net/http/httptest"
	"testing"
)

/* 配置只由 conf/config.yml 提供、没有在线编辑入口，
 * 因此启动期校验必须足够严格：坏配置直接启动失败，不静默上线。 */
func TestValidateConfigRejectsBadValues(t *testing.T) {
	base := func() Config {
		var cfg Config
		cfg.Server.Listen = ":8001"
		cfg.Server.Token = "server-token"
		cfg.Worktime.PingCode.TimeoutSec = 20
		normalizeAIConfig(&cfg)
		return cfg
	}
	if err := validateConfig(base()); err != nil {
		t.Fatalf("合法配置被拒绝: %v", err)
	}

	bad := base()
	bad.Server.Listen = "8001"
	if err := validateConfig(bad); err == nil {
		t.Fatal("缺少冒号的监听地址被接受")
	}

	bad = base()
	bad.Server.Token = " "
	if err := validateConfig(bad); err == nil {
		t.Fatal("空设备 Token 被接受")
	}

	bad = base()
	bad.Worktime.PingCode.BaseURL = "http://insecure.example.com"
	bad.Worktime.PingCode.Username = "u"
	bad.Worktime.PingCode.Password = "p"
	if err := validateConfig(bad); err == nil {
		t.Fatal("非 HTTPS 的 PingCode 地址被接受")
	}

	bad = base()
	bad.Worktime.PingCode.TimeoutSec = 0
	if err := validateConfig(bad); err == nil {
		t.Fatal("超时为 0 被接受")
	}
}

/* 设备端（开发板后台）自行从已启用的应用里挑两个显示，
 * 所以服务端允许启用全部 4 个，不再强制"四选二"。 */
func TestValidateConfigAllowsAllProvidersEnabled(t *testing.T) {
	var cfg Config
	cfg.Server.Listen = ":8001"
	cfg.Server.Token = "server-token"
	cfg.Worktime.PingCode.TimeoutSec = 20
	normalizeAIConfig(&cfg)
	for i := range cfg.AIUsage.Providers {
		p := &cfg.AIUsage.Providers[i]
		p.Enabled = true
		switch p.ID {
		case "claude":
			p.AuthMode = "external_api"
			p.APIBase = "https://example.com"
		default:
			p.AuthMode = "client_login"
		}
	}
	if err := validateConfig(cfg); err != nil {
		t.Fatalf("四个应用全部启用被拒绝: %v", err)
	}
	if got := len(enabledAIProviders(cfg)); got != len(aiCatalog) {
		t.Fatalf("启用数量=%d 期望=%d", got, len(aiCatalog))
	}
}

func TestServerTokenAuthorize(t *testing.T) {
	var cfg Config
	cfg.Server.Token = "server-token"
	app := &App{cfg: cfg}
	for _, path := range []string{"/worktime/summary", "/ai/usage"} {
		// 正确的服务 Token 放行
		r := httptest.NewRequest("GET", path, nil)
		r.Header.Set("Authorization", "Bearer server-token")
		w := httptest.NewRecorder()
		app.authorize(func(w http.ResponseWriter, r *http.Request) { w.WriteHeader(204) })(w, r)
		if w.Code != 204 {
			t.Fatalf("%s with valid token status %d", path, w.Code)
		}
		// 错误 Token 一律 401（已无独立工时 Token）
		r = httptest.NewRequest("GET", path, nil)
		r.Header.Set("Authorization", "Bearer wrong-token")
		w = httptest.NewRecorder()
		app.authorize(func(w http.ResponseWriter, r *http.Request) { w.WriteHeader(204) })(w, r)
		if w.Code != 401 {
			t.Fatalf("%s with invalid token status %d", path, w.Code)
		}
	}
}
