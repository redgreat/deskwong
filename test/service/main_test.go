package main

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
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
	app := &App{cfg: cfg, client: upstream.Client()}

	unauthorized := httptest.NewRecorder()
	app.authorize(app.aiUsage)(unauthorized, httptest.NewRequest(http.MethodGet, "/ai/usage", nil))
	if unauthorized.Code != http.StatusUnauthorized {
		t.Fatalf("status=%d", unauthorized.Code)
	}

	ok := httptest.NewRequest(http.MethodGet, "/ai/usage", nil)
	ok.Header.Set("Authorization", "Bearer test-token")
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

func TestWorktimeExpectedHours(t *testing.T) {
	app := &App{client: &http.Client{}}
	ctx := context.Background()
	cases := []struct {
		year  int
		month time.Month
		want  float64
	}{
		{2026, time.March, 176},     // 无节假日：22 个工作日
		{2026, time.September, 176}, // 中秋 9/25-9/27 放假、9/20 调休 → 22 个工作日
		{2026, time.October, 144},   // 国庆 10/1-10/7 放假、10/10 调休 → 18 个工作日
		{2026, time.February, 128},  // 春节 2/15-2/23 放假、2/14 与 2/28 调休 → 16 个工作日
	}
	for _, c := range cases {
		if got := app.expectedHours(ctx, c.year, c.month, 8); got != c.want {
			t.Errorf("expectedHours(%d, %d, 8) = %v, want %v", c.year, c.month, got, c.want)
		}
	}
	// 未配置每日工时时按 8 小时兜底
	if got := app.expectedHours(ctx, 2026, time.March, 0); got != 176 {
		t.Errorf("expectedHours with default daily = %v, want 176", got)
	}
	// 调休上班的周末要计入，法定节假日要排除
	if !cnWorkdayAt(time.Date(2026, 10, 10, 0, 0, 0, 0, time.Local)) {
		t.Error("2026-10-10 调休上班应算工作日")
	}
	if cnWorkdayAt(time.Date(2026, 10, 1, 0, 0, 0, 0, time.Local)) {
		t.Error("2026-10-01 国庆节不应算工作日")
	}
	// 离线回落：接口不可用时必须退回内置表，不能退成 0
	offline := &App{client: &http.Client{Transport: roundTripFunc(func(*http.Request) (*http.Response, error) {
		return nil, errNoNetwork
	})}}
	if got := offline.expectedHours(ctx, 2026, time.October, 8); got != 144 {
		t.Errorf("offline expectedHours(2026, 10) = %v, want 144", got)
	}
	// 整年接口：解析放假与调休集合，错误路径不 panic
	good := `{"code":0,"holiday":{"01-01":{"holiday":true,"name":"元旦"},"01-04":{"holiday":false,"name":"元旦后补班"}}}`
	hy, err := parseTimorHolidayYear(2026, []byte(good))
	if err != nil || !hy.complete {
		t.Fatalf("parse=%v complete=%v err=%v", hy, hy.complete, err)
	}
	if !hy.off["2026-01-01"] || !hy.workdays["2026-01-04"] {
		t.Fatalf("holiday sets wrong: %+v", hy)
	}
	if _, err := parseTimorHolidayYear(2026, []byte(`{"code":-1}`)); err == nil {
		t.Error("bad code must fail")
	}
	if _, err := parseTimorHolidayYear(2026, []byte(`{oops`)); err == nil {
		t.Error("bad json must fail")
	}
	// 年历走公共接口 stub：缓存命中只请求一次，命中后按放假/调休计算
	calls := 0
	stub := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		calls++
		if !strings.HasSuffix(r.URL.Path, "/year/2031/") {
			t.Errorf("unexpected holiday path %s", r.URL.Path)
		}
		_, _ = w.Write([]byte(`{"code":0,"holiday":{"01-01":{"holiday":true,"name":"元旦"},"01-03":{"holiday":false,"name":"元旦后补班"}}}`))
	}))
	defer stub.Close()
	remote := &App{client: stub.Client()}
	t.Setenv("DESKWONG_HOLIDAY_API_BASE", stub.URL)
	// 2031-01-01 是周三：接口标记放假 → 不算工作日；01-03 是周五：接口标记调休 → 仍然算工作日。
	// 该月自然工作日 23 天，仅 1/1 被接口放假剔除 → 22 天 × 8 = 176。
	if got := remote.expectedHours(ctx, 2031, time.January, 8); got != 176 {
		t.Errorf("remote expectedHours(2031, 1) = %v, want 176", got)
	}
	if got := remote.expectedHours(ctx, 2031, time.January, 8); got != 176 || calls != 1 {
		t.Errorf("holiday year must be cached: got=%v calls=%d", got, calls)
	}
}

type roundTripFunc func(*http.Request) (*http.Response, error)

func (f roundTripFunc) RoundTrip(r *http.Request) (*http.Response, error) { return f(r) }

var errNoNetwork = errors.New("no network")

func TestFetchCodexUsageSendsAccountID(t *testing.T) {
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if got := r.Header.Get("Authorization"); got != "Bearer test-access" {
			t.Errorf("Authorization=%q", got)
		}
		if got := r.Header.Get("ChatGPT-Account-Id"); got != "acct-test" {
			t.Errorf("ChatGPT-Account-Id=%q", got)
		}
		_, _ = w.Write([]byte(`{"plan_type":"plus","rate_limit":{"primary_window":{"used_percent":1,"limit_window_seconds":18000,"reset_at":1790000000}}}`))
	}))
	defer upstream.Close()

	dir := t.TempDir()
	authPath := filepath.Join(dir, "auth.json")
	auth := `{"tokens":{"access_token":"test-access","account_id":"acct-test"}}`
	if err := os.WriteFile(authPath, []byte(auth), 0600); err != nil {
		t.Fatal(err)
	}
	t.Setenv("DESKWONG_CODEX_AUTH_FILE", authPath)
	t.Setenv("DESKWONG_CODEX_ACCESS_TOKEN", "")
	t.Setenv("DESKWONG_CODEX_USAGE_URL", upstream.URL)
	if _, err := fetchCodexUsage(t.Context(), upstream.Client()); err != nil {
		t.Fatal(err)
	}
}
