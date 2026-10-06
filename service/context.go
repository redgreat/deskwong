package main

import (
	"crypto/subtle"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

const maxContextBody = 16 << 10

type contextAIUsage struct {
	Name             string  `json:"name"`
	RemainingPercent float64 `json:"remaining_percent"`
	ResetsAt         int64   `json:"resets_at"`
}

type contextSnapshot struct {
	DeviceID   string `json:"device_id"`
	ObservedAt int64  `json:"observed_at"`
	Worktime   struct {
		TodayHours         float64 `json:"today_hours"`
		MonthRecordedHours float64 `json:"month_recorded_hours"`
		MonthExpectedHours float64 `json:"month_expected_hours"`
	} `json:"worktime"`
	AIUsage []contextAIUsage `json:"ai_usage"`
	Weather struct {
		Text            string `json:"text"`
		TemperatureC    int    `json:"temperature_c"`
		HumidityPercent int    `json:"humidity_percent"`
		AlmanacYi       string `json:"almanac_yi"`
		AlmanacJi       string `json:"almanac_ji"`
	} `json:"weather"`
	RaceBox struct {
		Date         string `json:"date"`
		SyncedPoints int    `json:"synced_points"`
		SyncComplete bool   `json:"sync_complete"`
	} `json:"racebox"`
}

func bearerToken(r *http.Request) string {
	v := strings.TrimSpace(r.Header.Get("Authorization"))
	if len(v) > 7 && strings.EqualFold(v[:7], "Bearer ") {
		return strings.TrimSpace(v[7:])
	}
	return ""
}

func tokenEqual(got, want string) bool {
	return got != "" && want != "" && len(got) == len(want) && subtle.ConstantTimeCompare([]byte(got), []byte(want)) == 1
}

func validDeviceID(v string) bool {
	if len(v) < 3 || len(v) > 64 {
		return false
	}
	for _, r := range v {
		if !(r == ':' || r == '-' || r == '_' || r == '.' || r >= '0' && r <= '9' || r >= 'a' && r <= 'z' || r >= 'A' && r <= 'Z') {
			return false
		}
	}
	return true
}

func (a *App) contextReport(w http.ResponseWriter, r *http.Request) {
	if !tokenEqual(bearerToken(r), a.cfg.Context.ReportToken) {
		jsonReply(w, http.StatusUnauthorized, map[string]any{"code": 1001, "msg": "unauthorized"})
		return
	}
	body, err := io.ReadAll(io.LimitReader(r.Body, maxContextBody+1))
	if err != nil || len(body) > maxContextBody {
		jsonReply(w, http.StatusRequestEntityTooLarge, map[string]any{"code": 1002, "msg": "snapshot too large"})
		return
	}
	var snap contextSnapshot
	if err := json.Unmarshal(body, &snap); err != nil || !validDeviceID(snap.DeviceID) || snap.ObservedAt <= 0 {
		jsonReply(w, http.StatusBadRequest, map[string]any{"code": 1003, "msg": "invalid snapshot"})
		return
	}
	if headerID := strings.TrimSpace(r.Header.Get("device-id")); headerID == "" || headerID != snap.DeviceID {
		jsonReply(w, http.StatusBadRequest, map[string]any{"code": 1005, "msg": "device-id mismatch"})
		return
	}
	now := time.Now().Unix()
	if snap.ObservedAt > now+300 {
		jsonReply(w, http.StatusBadRequest, map[string]any{"code": 1004, "msg": "observed_at is in the future"})
		return
	}
	canonical, _ := json.Marshal(snap)
	_, err = a.settings.Exec(`INSERT INTO voice_context_snapshots(device_id,observed_at,received_at,body) VALUES(?,?,?,?)
		ON CONFLICT(device_id) DO UPDATE SET observed_at=excluded.observed_at,received_at=excluded.received_at,body=excluded.body
		WHERE excluded.observed_at >= voice_context_snapshots.observed_at`, snap.DeviceID, snap.ObservedAt, now, canonical)
	if err != nil {
		jsonReply(w, http.StatusInternalServerError, map[string]any{"code": 1500, "msg": "snapshot store failed"})
		return
	}
	jsonReply(w, http.StatusOK, map[string]any{"code": 0, "msg": "success"})
}

func fmtHours(v float64) string {
	return strings.TrimRight(strings.TrimRight(fmt.Sprintf("%.1f", v), "0"), ".")
}

func (a *App) contextRead(w http.ResponseWriter, r *http.Request) {
	if !tokenEqual(bearerToken(r), a.cfg.Context.ReadToken) {
		jsonReply(w, http.StatusUnauthorized, map[string]any{"code": 1001, "msg": "unauthorized"})
		return
	}
	deviceID := strings.TrimSpace(r.Header.Get("device-id"))
	if !validDeviceID(deviceID) {
		jsonReply(w, http.StatusBadRequest, map[string]any{"code": 1003, "msg": "invalid device-id"})
		return
	}
	var body []byte
	var observed int64
	if err := a.settings.QueryRow("SELECT observed_at,body FROM voice_context_snapshots WHERE device_id=?", deviceID).Scan(&observed, &body); err != nil {
		jsonReply(w, http.StatusNotFound, map[string]any{"code": 1404, "msg": "context not found"})
		return
	}
	var snap contextSnapshot
	if json.Unmarshal(body, &snap) != nil {
		jsonReply(w, http.StatusInternalServerError, map[string]any{"code": 1501, "msg": "invalid stored context"})
		return
	}
	now := time.Now()
	age := now.Sub(time.Unix(observed, 0))
	data := map[string]string{}
	if snap.Worktime.MonthExpectedHours > 0 {
		data["工时"] = fmt.Sprintf("今天%s小时，本月%s/%s小时", fmtHours(snap.Worktime.TodayHours), fmtHours(snap.Worktime.MonthRecordedHours), fmtHours(snap.Worktime.MonthExpectedHours))
	}
	if age <= 24*time.Hour {
		if len(snap.AIUsage) > 0 {
			parts := make([]string, 0, len(snap.AIUsage))
			for _, item := range snap.AIUsage {
				part := fmt.Sprintf("%s剩余%.0f%%", item.Name, item.RemainingPercent)
				if item.ResetsAt > 0 {
					part += "，" + time.Unix(item.ResetsAt, 0).Format("01-02 15:04") + "重置"
				}
				parts = append(parts, part)
			}
			data["AI用量"] = strings.Join(parts, "；")
		}
		if snap.Weather.Text != "" {
			data["天气"] = fmt.Sprintf("%s，%d℃，湿度%d%%", snap.Weather.Text, snap.Weather.TemperatureC, snap.Weather.HumidityPercent)
		}
		if snap.Weather.AlmanacYi != "" || snap.Weather.AlmanacJi != "" {
			data["今日黄历"] = "宜：" + snap.Weather.AlmanacYi + "；忌：" + snap.Weather.AlmanacJi
		}
		if snap.RaceBox.Date == now.Format("2006-01-02") {
			state := "同步中"
			if snap.RaceBox.SyncComplete {
				state = "已完成"
			}
			data["RaceBox"] = fmt.Sprintf("今天已同步%d条，%s", snap.RaceBox.SyncedPoints, state)
		}
	}
	data["数据时间"] = time.Unix(observed, 0).Format("2006-01-02 15:04:05")
	if age > 15*time.Minute {
		data["数据状态"] = "缓存数据，请留意更新时间"
	}
	jsonReply(w, http.StatusOK, map[string]any{"code": 0, "msg": "success", "data": data})
}
