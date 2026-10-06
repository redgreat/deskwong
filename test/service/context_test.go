package main

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func contextTestApp(t *testing.T) (*App, func()) {
	t.Helper()
	var cfg Config
	cfg.Server.Listen = ":8001"
	cfg.Server.Token = "general"
	cfg.Server.Username = "admin"
	cfg.Server.Password = "password"
	cfg.Context.ReportToken = "report-secret"
	cfg.Context.ReadToken = "read-secret"
	cfg.Worktime.PingCode.TimeoutSec = 20
	store, err := openSettings(filepath.Join(t.TempDir(), "settings.sqlite"), cfg)
	if err != nil { t.Fatal(err) }
	return &App{cfg: cfg, settings: store}, func() { store.Close() }
}

func TestVoiceContextReportAndRead(t *testing.T) {
	app, closeFn := contextTestApp(t)
	defer closeFn()
	now := time.Now()
	snapshot := contextSnapshot{DeviceID: "94:a9:90:dd:12:38", ObservedAt: now.Unix()}
	snapshot.Worktime.TodayHours = 2
	snapshot.Worktime.MonthRecordedHours = 42
	snapshot.Worktime.MonthExpectedHours = 144
	snapshot.AIUsage = []contextAIUsage{{Name: "ChatGPT 5小时", RemainingPercent: 72, ResetsAt: now.Add(time.Hour).Unix()}}
	snapshot.Weather.Text = "晴"
	snapshot.Weather.TemperatureC = 23
	snapshot.Weather.HumidityPercent = 40
	snapshot.Weather.AlmanacYi = "出行、学习"
	snapshot.Weather.AlmanacJi = "动土"
	snapshot.RaceBox.Date = now.Format("2006-01-02")
	snapshot.RaceBox.SyncedPoints = 750
	snapshot.RaceBox.SyncComplete = true
	body, _ := json.Marshal(snapshot)
	req := httptest.NewRequest(http.MethodPost, "/voice/context/report", bytes.NewReader(body))
	req.Header.Set("Authorization", "Bearer report-secret")
	req.Header.Set("device-id", snapshot.DeviceID)
	w := httptest.NewRecorder()
	app.contextReport(w, req)
	if w.Code != http.StatusOK { t.Fatalf("report: %d %s", w.Code, w.Body.String()) }

	req = httptest.NewRequest(http.MethodGet, "/voice/context", nil)
	req.Header.Set("Authorization", "Bearer read-secret")
	req.Header.Set("device-id", snapshot.DeviceID)
	w = httptest.NewRecorder()
	app.contextRead(w, req)
	if w.Code != http.StatusOK { t.Fatalf("read: %d %s", w.Code, w.Body.String()) }
	for _, want := range []string{"今天2小时", "本月42/144小时", "ChatGPT 5小时剩余72%", "晴，23℃", "今天已同步750条"} {
		if !strings.Contains(w.Body.String(), want) { t.Errorf("missing %q in %s", want, w.Body.String()) }
	}
}

func TestVoiceContextAuthMissingAndStale(t *testing.T) {
	app, closeFn := contextTestApp(t)
	defer closeFn()
	unauthorized := httptest.NewRecorder()
	app.contextRead(unauthorized, httptest.NewRequest(http.MethodGet, "/voice/context", nil))
	if unauthorized.Code != http.StatusUnauthorized { t.Fatalf("unauthorized=%d", unauthorized.Code) }

	missing := httptest.NewRequest(http.MethodGet, "/voice/context", nil)
	missing.Header.Set("Authorization", "Bearer read-secret")
	missing.Header.Set("device-id", "unknown-device")
	w := httptest.NewRecorder()
	app.contextRead(w, missing)
	if w.Code != http.StatusNotFound { t.Fatalf("missing=%d", w.Code) }

	now := time.Now()
	snapshot := contextSnapshot{DeviceID: "stale-device", ObservedAt: now.Add(-25 * time.Hour).Unix()}
	snapshot.Worktime.MonthRecordedHours = 42
	snapshot.Worktime.MonthExpectedHours = 144
	snapshot.Weather.Text = "晴"
	snapshot.RaceBox.Date = now.Format("2006-01-02")
	body, _ := json.Marshal(snapshot)
	report := httptest.NewRequest(http.MethodPost, "/voice/context/report", bytes.NewReader(body))
	report.Header.Set("Authorization", "Bearer report-secret")
	report.Header.Set("device-id", snapshot.DeviceID)
	w = httptest.NewRecorder()
	app.contextReport(w, report)
	if w.Code != http.StatusOK { t.Fatal(w.Body.String()) }
	read := httptest.NewRequest(http.MethodGet, "/voice/context", nil)
	read.Header.Set("Authorization", "Bearer read-secret")
	read.Header.Set("device-id", "stale-device")
	w = httptest.NewRecorder()
	app.contextRead(w, read)
	if strings.Contains(w.Body.String(), "天气") || strings.Contains(w.Body.String(), "RaceBox") || !strings.Contains(w.Body.String(), "缓存数据") {
		t.Fatalf("stale filtering failed: %s", w.Body.String())
	}
}

func TestVoiceContextRejectsOlderOverwrite(t *testing.T) {
	app, closeFn := contextTestApp(t)
	defer closeFn()
	post := func(ts int64, points int) {
		s := contextSnapshot{DeviceID: "order-device", ObservedAt: ts}
		s.RaceBox.Date = time.Now().Format("2006-01-02")
		s.RaceBox.SyncedPoints = points
		body, _ := json.Marshal(s)
		r := httptest.NewRequest(http.MethodPost, "/voice/context/report", bytes.NewReader(body))
		r.Header.Set("Authorization", "Bearer report-secret")
		r.Header.Set("device-id", s.DeviceID)
		w := httptest.NewRecorder(); app.contextReport(w, r)
		if w.Code != 200 { t.Fatal(w.Body.String()) }
	}
	now := time.Now().Unix()
	post(now, 100)
	post(now-60, 1)
	var body string
	if err := app.settings.QueryRow("SELECT body FROM voice_context_snapshots WHERE device_id='order-device'").Scan(&body); err != nil { t.Fatal(err) }
	if !strings.Contains(body, `"synced_points":100`) { t.Fatalf("older report overwrote snapshot: %s", body) }
}
