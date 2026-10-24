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

func TestMiMoLinuxOAuthRotationIsEncryptedAndPersisted(t *testing.T) {
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

	var cfg Config
	normalizeAIConfig(&cfg)
	for i := range cfg.AIUsage.Providers {
		if cfg.AIUsage.Providers[i].ID == "mimo" {
			cfg.AIUsage.Providers[i].RefreshToken = "initial-refresh"
		}
	}
	store, err := openSettings(filepath.Join(t.TempDir(), "settings.sqlite"), cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer store.Close()
	app := &App{client: server.Client(), settings: store}
	got, err := app.fetchMiMoOAuth(context.Background(), "initial-refresh")
	if err != nil || got.Windows[0].RemainingPercent != 80 {
		t.Fatalf("result=%+v err=%v", got, err)
	}
	saved, err := readSettings(store)
	if err != nil {
		t.Fatal(err)
	}
	var refresh string
	for _, p := range saved.AIUsage.Providers {
		if p.ID == "mimo" {
			refresh = p.RefreshToken
		}
	}
	if refresh != "rotated-refresh" {
		t.Fatalf("saved refresh token=%q", refresh)
	}
	var raw string
	if err = store.QueryRow("SELECT body FROM settings WHERE id=1").Scan(&raw); err != nil {
		t.Fatal(err)
	}
	if strings.Contains(raw, "initial-refresh") || strings.Contains(raw, "rotated-refresh") {
		t.Fatal("MiMo refresh token was stored in plaintext")
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

func containsJSONField(body []byte, field string) bool {
	for i := 0; i+len(field) <= len(body); i++ {
		if string(body[i:i+len(field)]) == field {
			return true
		}
	}
	return false
}
