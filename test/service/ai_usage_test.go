package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync/atomic"
	"testing"
	"time"
)

func TestAISelectionValidationAndOrdering(t *testing.T) {
	var cfg Config
	normalizeAIConfig(&cfg)
	if err := validateAIConfig(cfg); err != nil {
		t.Fatal(err)
	}
	cfg.AIUsage.Providers[1].Enabled = true
	cfg.AIUsage.Providers[1].Order = -1
	selected := enabledAIProviders(cfg)
	if len(selected) != 2 || selected[0].ID != "devin" || selected[1].ID != "chatgpt" {
		t.Fatalf("selection order = %+v", selected)
	}
	cfg.AIUsage.Providers[2].Enabled = true
	if err := validateAIConfig(cfg); err == nil {
		t.Fatal("three enabled providers must be rejected")
	}
}

func TestDevinClientLoginAndManualQuota(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method == http.MethodPost {
			_, _ = w.Write([]byte(`{"userStatus":{"planStatus":{"dailyQuotaRemainingPercent":64,"weeklyQuotaRemainingPercent":31,"dailyQuotaResetAtUnix":1800000000,"weeklyQuotaResetAtUnix":1800500000,"planInfo":{"hideDailyQuota":false}}}}`))
			return
		}
		if r.URL.Path != "/api/org_test/billing/quota/usage" || r.Header.Get("x-cog-org-id") != "org_test" {
			t.Fatalf("unexpected Devin request %s headers=%v", r.URL.Path, r.Header)
		}
		_, _ = w.Write([]byte(`{"daily_percentage":12,"weekly_percentage":42,"daily_reset_at":"2026-11-01T00:00:00Z","weekly_reset_at":1800500000}`))
	}))
	defer server.Close()
	credentials := filepath.Join(t.TempDir(), "credentials.toml")
	if err := os.WriteFile(credentials, []byte("windsurf_api_key = \"secret\"\napi_server_url = \""+server.URL+"\"\n"), 0600); err != nil {
		t.Fatal(err)
	}
	t.Setenv("DESKWONG_DEVIN_CREDENTIALS_FILE", credentials)
	app := &App{client: server.Client()}
	got, err := app.fetchDevinClient(context.Background())
	if err != nil || len(got.Windows) != 2 || got.Windows[0].RemainingPercent != 64 || got.Windows[1].RemainingPercent != 31 {
		t.Fatalf("client result=%+v err=%v", got, err)
	}
	dbPath := filepath.Join(t.TempDir(), "state.vscdb")
	db, err := sql.Open("sqlite", dbPath)
	if err != nil {
		t.Fatal(err)
	}
	if _, err = db.Exec("CREATE TABLE ItemTable(key TEXT PRIMARY KEY,value TEXT); INSERT INTO ItemTable VALUES('windsurfAuthStatus','{\"apiKey\":\"db-secret\"}')"); err != nil {
		t.Fatal(err)
	}
	_ = db.Close()
	t.Setenv("DESKWONG_DEVIN_CREDENTIALS_FILE", filepath.Join(t.TempDir(), "missing.toml"))
	t.Setenv("DESKWONG_DEVIN_STATE_DB", dbPath)
	t.Setenv("DESKWONG_DEVIN_API_SERVER", server.URL)
	got, err = app.fetchDevinClient(context.Background())
	if err != nil || len(got.Windows) != 2 {
		t.Fatalf("state db result=%+v err=%v", got, err)
	}
	old := devinWebBase
	devinWebBase = server.URL
	defer func() { devinWebBase = old }()
	got, err = app.fetchDevinWeb(context.Background(), "org_test", "Bearer token")
	if err != nil || got.Windows[0].RemainingPercent != 88 || got.Windows[1].RemainingPercent != 58 {
		t.Fatalf("manual result=%+v err=%v", got, err)
	}
}

func TestMiMoDesktopOAuthUsage(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("Authorization") != "Bearer access-token" {
			t.Fatalf("authorization=%q", r.Header.Get("Authorization"))
		}
		_, _ = w.Write([]byte(`{"code":0,"data":{"percent":27.5,"resetDate":"2026-11-01T00:00:00Z"}}`))
	}))
	defer server.Close()
	auth := filepath.Join(t.TempDir(), "auth.json")
	body := `{"xiaomi":{"type":"oauth","access_token":"access-token","refresh_token":"refresh-token","expires_at":` + strconv.FormatInt(time.Now().Add(time.Hour).UnixMilli(), 10) + `}}`
	if err := os.WriteFile(auth, []byte(body), 0600); err != nil {
		t.Fatal(err)
	}
	t.Setenv("DESKWONG_MIMO_AUTH_FILE", auth)
	old := mimoUsageURL
	mimoUsageURL = server.URL
	defer func() { mimoUsageURL = old }()
	got, err := (&App{client: server.Client()}).fetchMiMoClient(context.Background())
	if err != nil || len(got.Windows) != 1 || got.Windows[0].RemainingPercent != 72.5 || got.Windows[0].ResetsAt == 0 {
		t.Fatalf("result=%+v err=%v", got, err)
	}
}

/* 上游轮换下来的 Refresh Token 落在数据卷文件（conf 是只读挂载），
 * 启动时由 loadPersistedRefreshTokens 覆盖 conf.yml 里的旧值。
 * 过去这一步依赖 SQLite 里的可编辑配置 + /admin 后台，现已移除。 */
func TestMiMoLinuxOAuthRotationIsPersisted(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Path {
		case "/oauth":
			if err := r.ParseForm(); err != nil || r.Form.Get("refresh_token") != "initial-refresh" {
				t.Fatalf("refresh form=%v err=%v", r.Form, err)
			}
			_, _ = w.Write([]byte(`{"access_token":"rotated-access","refresh_token":"rotated-refresh","expires_in":3600}`))
		case "/usage":
			if r.Header.Get("Authorization") != "Bearer rotated-access" {
				t.Fatalf("authorization=%q", r.Header.Get("Authorization"))
			}
			_, _ = w.Write([]byte(`{"code":0,"data":{"percent":20,"resetDate":"2026-11-01T00:00:00Z"}}`))
		default:
			http.NotFound(w, r)
		}
	}))
	defer server.Close()
	oldOAuth, oldUsage := mimoOAuthURL, mimoUsageURL
	mimoOAuthURL, mimoUsageURL = server.URL+"/oauth", server.URL+"/usage"
	defer func() { mimoOAuthURL, mimoUsageURL = oldOAuth, oldUsage }()

	refreshFile := filepath.Join(t.TempDir(), "ai_refresh_token.json")
	t.Setenv("DESKWONG_REFRESH_TOKEN_FILE", refreshFile)

	app := &App{client: server.Client()}
	got, err := app.fetchMiMoOAuth(context.Background(), "initial-refresh")
	if err != nil || got.Windows[0].RemainingPercent != 80 {
		t.Fatalf("result=%+v err=%v", got, err)
	}
	raw, err := os.ReadFile(refreshFile)
	if err != nil {
		t.Fatalf("轮换后的 refresh token 未落盘: %v", err)
	}
	if !strings.Contains(string(raw), "rotated-refresh") {
		t.Fatalf("轮换结果未写入: %s", raw)
	}
	// 模拟重启：conf.yml 里的旧值应被数据卷中的轮换结果覆盖
	var cfg Config
	normalizeAIConfig(&cfg)
	loadPersistedRefreshTokens(&cfg)
	var reload string
	for _, p := range cfg.AIUsage.Providers {
		if p.ID == "mimo" {
			reload = p.RefreshToken
		}
	}
	if reload != "rotated-refresh" {
		t.Fatalf("重启后载入的 refresh token=%q", reload)
	}
}

func TestExternalAIAdapterPreservesZeroAndStatus(t *testing.T) {
	var calls atomic.Int32
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		calls.Add(1)
		if r.URL.Path != "/ai/usage" {
			t.Errorf("path=%s", r.URL.Path)
		}
		_, _ = w.Write([]byte(`{"code":0,"data":{"apps":[{"id":"devin","name":"ignored","status":"cached","observed_at":1234,"windows":[{"id":"daily","label":"日","remaining_percent":0}]}]}}`))
	}))
	defer upstream.Close()
	app := &App{client: upstream.Client()}
	got, err := app.fetchExternalAI(context.Background(), AIProviderConfig{ID: "devin", AuthMode: "external_api", APIBase: upstream.URL})
	if err != nil {
		t.Fatal(err)
	}
	if calls.Load() != 1 || got.Status != "cached" || got.ObservedAt != 1234 || len(got.Windows) != 1 || got.Windows[0].RemainingPercent != 0 {
		t.Fatalf("normalized app = %+v", got)
	}
	body, err := json.Marshal(got)
	if err != nil || !containsJSONField(body, `"remaining_percent":0`) {
		t.Fatalf("zero quota omitted: %s err=%v", body, err)
	}
}

/* 服务端不轮询上游、不长期缓存：/ai/usage 每次被请求时才实时查启用应用，
 * 失败时回退最近一次成功结果并标记 stale。 */
func TestAIUsageFetchesUpstreamOnRequest(t *testing.T) {
	var calls atomic.Int32
	upstream := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		calls.Add(1)
		_, _ = w.Write([]byte(`{"code":0,"data":{"apps":[{"id":"devin","name":"Devin","status":"live","windows":[{"id":"daily","label":"日","remaining_percent":61}]}]}}`))
	}))
	defer upstream.Close()

	var cfg Config
	normalizeAIConfig(&cfg)
	for i := range cfg.AIUsage.Providers {
		p := &cfg.AIUsage.Providers[i]
		p.Enabled = p.ID == "devin"
		if p.ID == "devin" {
			p.AuthMode = "external_api"
			p.APIBase = upstream.URL
		}
	}
	if err := validateAIConfig(cfg); err != nil {
		t.Fatal(err)
	}
	app := &App{cfg: cfg, client: upstream.Client()}

	rec := httptest.NewRecorder()
	app.aiUsage(rec, httptest.NewRequest(http.MethodGet, "/ai/usage", nil))
	if rec.Code != 200 || calls.Load() != 1 {
		t.Fatalf("code=%d calls=%d body=%s", rec.Code, calls.Load(), rec.Body.String())
	}
	if !strings.Contains(rec.Body.String(), `"remaining_percent":61`) {
		t.Fatalf("missing live quota: %s", rec.Body.String())
	}

	/* 不缓存：第二次请求必须再次实时查上游，而不是回放上次结果 */
	app.aiUsage(rec, httptest.NewRequest(http.MethodGet, "/ai/usage", nil))
	if calls.Load() != 2 {
		t.Fatalf("second request calls=%d (must not serve from cache)", calls.Load())
	}

	/* 上游不可用：整体仍返回 200，该应用回退最近一次成功结果并标记 stale */
	upstream.Close()
	rec = httptest.NewRecorder()
	app.aiUsage(rec, httptest.NewRequest(http.MethodGet, "/ai/usage", nil))
	if rec.Code != 200 || !strings.Contains(rec.Body.String(), `"status":"stale"`) {
		t.Fatalf("stale fallback code=%d body=%s", rec.Code, rec.Body.String())
	}
}

func containsJSONField(body []byte, field string) bool {
	for i := 0; i+len(field) <= len(body); i++ {
		if string(body[i:i+len(field)]) == field {
			return true
		}
	}
	return false
}
