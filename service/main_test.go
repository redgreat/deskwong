package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"
)

func TestAuthorization(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_, _ = w.Write([]byte(`{"code":0,"data":{"providers":[]}}`))
	}))
	defer upstream.Close()
	t.Setenv("DESKWONG_AI_UPSTREAM", upstream.URL)

	var cfg Config
	cfg.Server.Token = "test-token"
	cfg.Server.Tokens = []string{"device-b"}
	app := &App{cfg: cfg, client: upstream.Client()}

	unauthorized := httptest.NewRecorder()
	app.authorize(app.aiUsage)(unauthorized, httptest.NewRequest(http.MethodGet, "/ai/usage", nil))
	if unauthorized.Code != http.StatusUnauthorized {
		t.Fatalf("status=%d", unauthorized.Code)
	}

	ok := httptest.NewRequest(http.MethodGet, "/ai/usage", nil)
	ok.Header.Set("Authorization", "Bearer device-b")
	rec := httptest.NewRecorder()
	app.authorize(app.aiUsage)(rec, ok)
	if rec.Code != http.StatusOK {
		t.Fatalf("valid token rejected: %d body=%s", rec.Code, rec.Body.String())
	}
}

func TestWorktimeRequiresUpstream(t *testing.T) {
	var cfg Config
	cfg.Server.Token = "test-token"
	app := &App{cfg: cfg, client: &http.Client{}}
	t.Setenv("DESKWONG_WORKTIME_UPSTREAM", "")

	req := httptest.NewRequest(http.MethodGet, "/worktime/summary?year=2026&month=9", nil)
	req.Header.Set("Authorization", "Bearer test-token")
	rec := httptest.NewRecorder()
	app.authorize(app.worktime)(rec, req)
	if rec.Code != http.StatusNotImplemented {
		t.Fatalf("status=%d body=%s", rec.Code, rec.Body.String())
	}
}

func TestCodexProvidersNormalization(t *testing.T) {
	var u codexUsage
	raw := `{"plan_type":"pro","rate_limit":{"primary_window":{"used_percent":28,"limit_window_seconds":18000,"reset_at":1789223400},"secondary_window":{"used_percent":56,"limit_window_seconds":604800,"reset_at":"2030-01-02T03:04:05Z"}}}`
	if err := json.Unmarshal([]byte(raw), &u); err != nil {
		t.Fatal(err)
	}
	providers := codexProviders(u)
	if len(providers) != 2 {
		t.Fatalf("providers=%d", len(providers))
	}
	if providers[0]["id"] != "chatgpt_5h" || providers[0]["window_minutes"] != 300 || providers[0]["remaining_percent"] != 72.0 {
		t.Fatalf("5h provider: %+v", providers[0])
	}
	if providers[1]["id"] != "chatgpt_weekly" || providers[1]["window_minutes"] != 10080 || providers[1]["remaining_percent"] != 44.0 {
		t.Fatalf("weekly provider: %+v", providers[1])
	}
	if providers[1]["resets_at"] != int64(1893553445) { // 2030-01-02T03:04:05Z
		t.Fatalf("resets_at=%v", providers[1]["resets_at"])
	}
}

func TestRetentionSQLGuard(t *testing.T) {
	// 未配置数据库时 flush / retention 必须安全退出，不能 panic
	var cfg Config
	app := &App{cfg: cfg}
	app.flush(nil, []mqttMessage{{topic: "t", body: []byte(`{}`), at: time.Now()}})
}
