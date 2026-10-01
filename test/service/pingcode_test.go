package main

import (
	"crypto/md5"
	"encoding/hex"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"
)

func TestPingCodeWorktimeLoginPaginationAndAggregation(t *testing.T) {
	requests := 0
	server := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Path {
		case "/api/typhon/team/signin":
			var body map[string]string
			_ = json.NewDecoder(r.Body).Decode(&body)
			sum := md5.Sum([]byte("secret"))
			if body["signin_name"] != "user" || body["password"] != hex.EncodeToString(sum[:]) {
				t.Fatalf("unexpected login payload: %+v", body)
			}
			_, _ = w.Write([]byte(`{"code":0,"data":{"value":{"access_token":"token"}}}`))
		case "/api/ladon/workload-logs/mine":
			if r.Header.Get("Authorization") != "Bearer token" {
				t.Fatalf("missing access token")
			}
			var body struct {
				Query struct {
					Pagination struct {
						To struct {
							Page int `json:"pi"`
						} `json:"to"`
					} `json:"pagination"`
				} `json:"query"`
			}
			_ = json.NewDecoder(r.Body).Decode(&body)
			requests++
			if body.Query.Pagination.To.Page == 0 {
				_, _ = w.Write([]byte(`{"code":0,"data":{"value":[{"register_date":1790784000,"man_hour":1.5}],"count":2,"page_index":0,"page_size":1,"page_count":2}}`))
			} else {
				_, _ = w.Write([]byte(`{"code":0,"data":{"value":[{"register_date":1790784000,"man_hour":0.5}],"count":2,"page_index":1,"page_size":1,"page_count":2}}`))
			}
		default:
			http.NotFound(w, r)
		}
	}))
	defer server.Close()
	var cfg Config
	cfg.Worktime.PingCode.BaseURL = server.URL
	cfg.Worktime.PingCode.Username = "user"
	cfg.Worktime.PingCode.Password = "secret"
	cfg.Worktime.PingCode.TimeoutSec = 5
	app := &App{cfg: cfg, client: server.Client()}
	days, total, err := app.queryPingCodeWorktime(t.Context(), 2026, time.October)
	if err != nil {
		t.Fatal(err)
	}
	if requests != 2 || len(days) != 1 || total != 2 || days[0]["date"] != "2026-10-01" {
		t.Fatalf("requests=%d days=%+v total=%v", requests, days, total)
	}
}

func TestPingCodeLiveOctoberFirst(t *testing.T) {
	username, password := os.Getenv("PINGCODE_LIVE_USERNAME"), os.Getenv("PINGCODE_LIVE_PASSWORD")
	if username == "" || password == "" {
		t.Skip("live PingCode credentials not configured")
	}
	var cfg Config
	cfg.Worktime.PingCode.BaseURL = "https://zhongrui.pingcode.com"
	cfg.Worktime.PingCode.Username = username
	cfg.Worktime.PingCode.Password = password
	cfg.Worktime.PingCode.TimeoutSec = 30
	app := &App{cfg: cfg, client: &http.Client{Timeout: 30 * time.Second}}
	days, total, err := app.queryPingCodeWorktime(t.Context(), 2026, time.October)
	if err != nil {
		t.Fatal(err)
	}
	found := false
	for _, day := range days {
		if day["date"] == "2026-10-01" {
			found = true
			if day["hours"] != 2.0 {
				t.Fatalf("2026-10-01 hours=%v, want 2", day["hours"])
			}
		}
	}
	if !found {
		t.Fatalf("2026-10-01 missing; days=%d total=%v", len(days), total)
	}
}
