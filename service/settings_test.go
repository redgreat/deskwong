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
	seed.Server.Tokens = []string{"device-only"}
	seed.Worktime.MySQL.DSN = "user:secret@tcp(localhost:3306)/pingcode"
	path := filepath.Join(t.TempDir(), "settings.sqlite")
	db, err := openSettings(path, seed)
	if err != nil {
		t.Fatal(err)
	}
	cfg, err := readSettings(db)
	if err != nil {
		t.Fatal(err)
	}
	app := &App{cfg: cfg, settings: db}
	mux := http.NewServeMux()
	app.registerAdmin(mux)
	request := func(method, token string, body []byte) *httptest.ResponseRecorder {
		r := httptest.NewRequest(method, "/api/settings", bytes.NewReader(body))
		r.Header.Set("Authorization", "Bearer "+token)
		w := httptest.NewRecorder()
		mux.ServeHTTP(w, r)
		return w
	}
	if w := request("GET", "device-only", nil); w.Code != 401 {
		t.Fatal("device token allowed to administer")
	}
	w := request("GET", "admin-secret", nil)
	if w.Code != 200 || strings.Contains(w.Body.String(), "user:secret") || strings.Contains(w.Body.String(), "admin-secret") {
		t.Fatalf("secrets exposed or read failed: %d", w.Code)
	}
	var response struct {
		Data Config `json:"data"`
	}
	json.Unmarshal(w.Body.Bytes(), &response)
	response.Data.Worktime.MySQL.EmployeeNo = "ZR16060018"
	response.Data.Worktime.MySQL.Query = "SELECT '2026-09-25' AS date, 8 AS hours"
	body, _ := json.Marshal(response.Data)
	if w = request("PUT", "admin-secret", body); w.Code != 200 {
		t.Fatal(w.Body.String())
	}
	if app.cfg.Worktime.MySQL.EmployeeNo != "" {
		t.Fatal("save changed active config before restart")
	}
	db.Close()
	// Reopen with different seed: persisted values must win over initial YAML.
	seed.Worktime.MySQL.EmployeeNo = "wrong"
	db, err = openSettings(path, seed)
	if err != nil {
		t.Fatal(err)
	}
	defer db.Close()
	saved, err := readSettings(db)
	if err != nil {
		t.Fatal(err)
	}
	if saved.Worktime.MySQL.EmployeeNo != "ZR16060018" || saved.Worktime.MySQL.DSN != cfg.Worktime.MySQL.DSN || saved.Server.Token != "admin-secret" {
		t.Fatal("persistent config or masked secrets were lost")
	}
	if len(saved.Server.Tokens) != 1 {
		t.Fatal("device tokens lost")
	}
	app.settings = db
	bad := saved
	bad.Worktime.MySQL.TimeoutSec = 0
	body, _ = json.Marshal(bad)
	if w = request("PUT", "admin-secret", body); w.Code != 400 {
		t.Fatal("invalid config accepted")
	}
	again, _ := readSettings(db)
	if again.Worktime.MySQL.TimeoutSec != 10 {
		t.Fatal("invalid save changed persistence")
	}
}

func TestWorktimeTokenIsScoped(t *testing.T) {
	var cfg Config
	cfg.Server.Token = "admin"
	cfg.Worktime.StaticToken = "worktime-only"
	app := &App{cfg: cfg}
	for _, path := range []string{"/worktime/summary", "/ai/usage"} {
		r := httptest.NewRequest("GET", path, nil)
		r.Header.Set("Authorization", "Bearer worktime-only")
		w := httptest.NewRecorder()
		app.authorize(func(w http.ResponseWriter, r *http.Request) { w.WriteHeader(204) })(w, r)
		want := 401
		if path == "/worktime/summary" {
			want = 204
		}
		if w.Code != want {
			t.Fatalf("%s status %d", path, w.Code)
		}
	}
}
