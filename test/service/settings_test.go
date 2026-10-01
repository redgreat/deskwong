package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
)

func TestSettingsPersistAndRestartBoundary(t *testing.T) {
	var seed Config
	seed.Server.Listen = ":8001"
	seed.Server.Token = "admin-secret"
	seed.Server.Username = "admin"
	seed.Server.Password = "admin-secret"
	seed.Worktime.PingCode.BaseURL = "https://zhongrui.pingcode.com"
	seed.Worktime.PingCode.Username = "test-user"
	seed.Worktime.PingCode.Password = "ping-secret"
	path := filepath.Join(t.TempDir(), "settings.sqlite")
	db, err := openSettings(path, seed)
	if err != nil {
		t.Fatal(err)
	}
	cfg, err := readSettings(db)
	if err != nil {
		t.Fatal(err)
	}
	var raw string
	if err := db.QueryRow("SELECT body FROM settings WHERE id=1").Scan(&raw); err != nil {
		t.Fatal(err)
	}
	for _, secret := range []string{"admin-secret", "ping-secret"} {
		if strings.Contains(raw, secret) {
			t.Fatalf("SQLite contains plaintext secret %q", secret)
		}
	}
	app := &App{cfg: cfg, settings: db}
	mux := http.NewServeMux()
	app.registerAdmin(mux)
	request := func(method, password string, body []byte) *httptest.ResponseRecorder {
		r := httptest.NewRequest(method, "/api/settings", bytes.NewReader(body))
		r.SetBasicAuth("admin", password)
		w := httptest.NewRecorder()
		mux.ServeHTTP(w, r)
		return w
	}
	if w := request("GET", "wrong-password", nil); w.Code != 401 {
		t.Fatal("wrong password allowed to administer")
	}
	w := request("GET", "admin-secret", nil)
	if w.Code != 200 || strings.Contains(w.Body.String(), "ping-secret") || strings.Contains(w.Body.String(), "admin-secret") {
		t.Fatalf("secrets exposed or read failed: %d", w.Code)
	}
	var response struct {
		Data Config `json:"data"`
	}
	json.Unmarshal(w.Body.Bytes(), &response)
	if response.Data.Worktime.PingCode.Password != "******" {
		t.Fatal("PingCode password was not masked")
	}
	response.Data.Worktime.PingCode.Username = "changed-user"
	body, _ := json.Marshal(response.Data)
	if w = request("PUT", "admin-secret", body); w.Code != 200 {
		t.Fatal(w.Body.String())
	}
	if app.cfg.Worktime.PingCode.Username != "test-user" {
		t.Fatal("save changed active config before restart")
	}
	db.Close()
	// Reopen with different seed: persisted values must win over initial YAML.
	seed.Worktime.PingCode.Username = "wrong"
	db, err = openSettings(path, seed)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	saved, err := readSettings(db)
	if err != nil {
		t.Fatal(err)
	}
	if saved.Worktime.PingCode.Username != "changed-user" || saved.Worktime.PingCode.Password != "ping-secret" || saved.Server.Token != "admin-secret" || saved.Server.Password != "admin-secret" {
		t.Fatal("persistent config or masked secrets were lost")
	}
	app.settings = db
	bad := saved
	bad.Worktime.PingCode.BaseURL = "http://insecure.example.com"
	body, _ = json.Marshal(bad)
	if w = request("PUT", "admin-secret", body); w.Code != 400 {
		t.Fatal("invalid config accepted")
	}
	again, _ := readSettings(db)
	if again.Worktime.PingCode.Username != "changed-user" {
		t.Fatal("invalid save changed persistence")
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
