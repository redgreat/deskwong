package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
)

// /almanac 返回设备端可直接解析的 {data:{yi,ji}} 结构，date 缺省取当日。
func TestAlmanacHandler(t *testing.T) {
	app := &App{}

	rec := httptest.NewRecorder()
	app.almanac(rec, httptest.NewRequest(http.MethodGet, "/almanac?date=2026-10-04", nil))
	if rec.Code != http.StatusOK {
		t.Fatalf("status=%d body=%s", rec.Code, rec.Body.String())
	}
	var resp struct {
		Code int `json:"code"`
		Data struct {
			Date   string   `json:"date"`
			Yangli string   `json:"yangli"`
			Yinli  string   `json:"yinli"`
			Yi     []string `json:"yi"`
			Ji     []string `json:"ji"`
		} `json:"data"`
	}
	if err := json.Unmarshal(rec.Body.Bytes(), &resp); err != nil {
		t.Fatalf("bad json: %v body=%s", err, rec.Body.String())
	}
	if resp.Code != 0 || len(resp.Data.Yi) == 0 || len(resp.Data.Ji) == 0 {
		t.Fatalf("almanac empty: code=%d yi=%v ji=%v", resp.Code, resp.Data.Yi, resp.Data.Ji)
	}
	if resp.Data.Date != "2026-10-04" || resp.Data.Yangli != "2026-10-04" {
		t.Fatalf("date mismatch: %+v", resp.Data)
	}
	if resp.Data.Yinli == "" {
		t.Fatal("missing yinli")
	}

	// 日期缺省 = 当日
	def := httptest.NewRecorder()
	app.almanac(def, httptest.NewRequest(http.MethodGet, "/almanac", nil))
	if def.Code != http.StatusOK {
		t.Fatalf("default date failed: %d", def.Code)
	}

	// 非法日期 → 400
	bad := httptest.NewRecorder()
	app.almanac(bad, httptest.NewRequest(http.MethodGet, "/almanac?date=not-a-date", nil))
	if bad.Code != http.StatusBadRequest {
		t.Fatalf("bad date accepted: %d", bad.Code)
	}
}
