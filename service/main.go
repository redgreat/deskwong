package main

import (
	"context"
	"database/sql"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"text/template"
	"time"

	_ "github.com/go-sql-driver/mysql"
	"gopkg.in/yaml.v3"
)

var version = "dev"

const (
	codexUsageURL = "https://chatgpt.com/backend-api/wham/usage"
	// ChatGPT 额度接口很敏感，缓存一份避免设备频繁刷新被打
	aiCacheTTL = 60 * time.Second
)

type Config struct {
	Server struct {
		Listen   string `yaml:"listen" json:"listen"`
		Token    string `yaml:"token" json:"token"`
		Username string `yaml:"username" json:"username"`
		Password string `yaml:"password" json:"password"`
	} `yaml:"server" json:"server"`
	Worktime struct {
		// 优先通过 PingCode 网页 API 取工时；保留 MySQL 作为兼容数据源。
		ExpectedDailyHours float64 `yaml:"expected_daily_hours" json:"expected_daily_hours"`
		StaticToken        string  `yaml:"token" json:"token"`
		PingCode           struct {
			BaseURL    string `yaml:"base_url" json:"base_url"`
			Username   string `yaml:"username" json:"username"`
			Password   string `yaml:"password" json:"password"`
			TimeoutSec int    `yaml:"timeout_sec" json:"timeout_sec"`
		} `yaml:"pingcode" json:"pingcode"`
		MySQL struct {
			DSN        string `yaml:"dsn" json:"dsn"`
			EmployeeNo string `yaml:"employee_no" json:"employee_no"`
			QueryFile  string `yaml:"query_file" json:"query_file"`
			Query      string `yaml:"query" json:"query"`
			TimeoutSec int    `yaml:"timeout_sec" json:"timeout_sec"`
		} `yaml:"mysql" json:"mysql"`
	} `yaml:"worktime" json:"worktime"`
	Log struct {
		Level  string `yaml:"level" json:"level"`
		Format string `yaml:"format" json:"format"`
	} `yaml:"log" json:"log"`
}

type App struct {
	cfg        Config
	settings   *SettingsStore
	restart    func()
	client     *http.Client
	worktimeDB *sql.DB // 工时查询（公司 PingCode MySQL）

	aiMu      sync.Mutex
	aiCached  []byte
	aiCacheAt time.Time

	// 节假日年历缓存：按年份缓存 timor.tech 整年数据，按天区分放假/调休。
	// 缓存最多保留 7 天，到期自动重拉；重拉失败保留旧缓存并告警。
	holMu    sync.Mutex
	holCache map[int]holidayYear
}

// ---- 节假日：免费接口 timor.tech（https://timor.tech/api/holiday）----
// 按年缓存 7 天；失败回落到 2025-2026 内置表，表外年份按自然周末。
// 通过 DESKWONG_HOLIDAY_API_BASE 覆盖，方便测试和内网代理。
func holidayAPIBase() string {
	if base := strings.TrimRight(os.Getenv("DESKWONG_HOLIDAY_API_BASE"), "/"); base != "" {
		return base
	}
	return "https://timor.tech/api/holiday"
}

type timorHolidayDay struct {
	Holiday bool   `json:"holiday"` // true=放假，false=调休上班
	Name    string `json:"name"`
}

type timorHolidayYear struct {
	Code    int                        `json:"code"`
	Holiday map[string]timorHolidayDay `json:"holiday"`
}

// holidayYear 是某年"放假集合 + 调休集合"，是否完整由 complete 标记。
type holidayYear struct {
	year     int
	fetched  time.Time
	off      map[string]bool // 放假（含法定假和连带休息）
	workdays map[string]bool // 调休上班
	complete bool            // 整年数据拉取成功，用于判断能否信任空调休集合
}

func parseTimorHolidayYear(year int, body []byte) (holidayYear, error) {
	var payload timorHolidayYear
	if err := json.Unmarshal(body, &payload); err != nil {
		return holidayYear{}, err
	}
	if payload.Code != 0 {
		return holidayYear{}, fmt.Errorf("节假日接口返回 code=%d", payload.Code)
	}
	out := holidayYear{year: year, off: map[string]bool{}, workdays: map[string]bool{}, complete: true}
	for md, day := range payload.Holiday {
		date := fmt.Sprintf("%04d-%s", year, md)
		if _, err := time.Parse("2006-01-02", date); err != nil {
			continue
		}
		if day.Holiday {
			out.off[date] = true
		} else {
			out.workdays[date] = true
		}
	}
	return out, nil
}

func (a *App) fetchHolidayYear(ctx context.Context, year int) (holidayYear, error) {
	timeout := 15 * time.Second
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	url := holidayAPIBase() + "/year/" + strconv.Itoa(year) + "/"
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return holidayYear{}, err
	}
	req.Header.Set("User-Agent", "deskwong-service/"+version)
	resp, err := a.client.Do(req)
	if err != nil {
		return holidayYear{}, err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return holidayYear{}, fmt.Errorf("节假日接口 HTTP %d", resp.StatusCode)
	}
	body, err := io.ReadAll(io.LimitReader(resp.Body, 1<<20))
	if err != nil {
		return holidayYear{}, err
	}
	return parseTimorHolidayYear(year, body)
}

// holidayYearFor 返回某年节假日：缓存 7 天内直接用，过期才重拉；
// 拉取失败保留旧缓存并告警，旧缓存也没有时才退回内置表。
func (a *App) holidayYearFor(ctx context.Context, year int) holidayYear {
	a.holMu.Lock()
	cached, ok := a.holCache[year]
	a.holMu.Unlock()
	if ok && time.Since(cached.fetched) < 7*24*time.Hour {
		return cached
	}
	fresh, err := a.fetchHolidayYear(ctx, year)
	if err != nil {
		slog.Warn("holiday fetch failed", "year", year, "error", err)
		return cached
	}
	fresh.fetched = time.Now()
	a.holMu.Lock()
	if a.holCache == nil {
		a.holCache = map[int]holidayYear{}
	}
	a.holCache[year] = fresh
	a.holMu.Unlock()
	return fresh
}

// isWorkdayAt 按"放假集合 + 调休集合 + 自然周末"判定是否应出勤。
func isWorkdayAt(t time.Time, hy holidayYear) bool {
	key := t.Format("2006-01-02")
	if hy.workdays[key] {
		return true
	}
	if hy.off[key] {
		return false
	}
	wd := t.Weekday()
	return wd != time.Saturday && wd != time.Sunday
}

func loadConfig(path string) (Config, error) {
	var cfg Config
	b, err := os.ReadFile(path)
	if err != nil {
		return cfg, err
	}
	if err := yaml.Unmarshal(b, &cfg); err != nil {
		return cfg, err
	}
	if cfg.Server.Listen == "" {
		cfg.Server.Listen = ":8000"
	}
	return cfg, nil
}

func setupLogger(cfg Config) {
	// 环境变量优先（docker-compose 里可直接设置），其次 conf/config.yml 的 log 段
	level := strings.ToLower(cfg.Log.Level)
	if env := strings.ToLower(os.Getenv("DESKWONG_LOG_LEVEL")); env != "" {
		level = env
	}
	var lv slog.Level
	switch level {
	case "debug":
		lv = slog.LevelDebug
	case "warn", "warning":
		lv = slog.LevelWarn
	case "error":
		lv = slog.LevelError
	default:
		lv = slog.LevelInfo
	}
	opts := &slog.HandlerOptions{Level: lv}
	format := cfg.Log.Format
	if env := os.Getenv("DESKWONG_LOG_FORMAT"); env != "" {
		format = env
	}
	var h slog.Handler
	if strings.EqualFold(format, "json") {
		h = slog.NewJSONHandler(os.Stdout, opts)
	} else {
		h = slog.NewTextHandler(os.Stdout, opts)
	}
	slog.SetDefault(slog.New(h))
}

func jsonReply(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(v)
}

func (a *App) tokenAllowed(got string) bool {
	return got != "" && a.cfg.Server.Token != "" && got == a.cfg.Server.Token
}

func (a *App) authorize(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		got := r.Header.Get("Authorization")
		got = strings.TrimPrefix(got, "Bearer ")
		if got == "" {
			got = r.Header.Get("X-Token")
		}
		if !a.tokenAllowed(got) && !(r.URL.Path == "/worktime/summary" && a.cfg.Worktime.StaticToken != "" && got == a.cfg.Worktime.StaticToken) {
			jsonReply(w, http.StatusUnauthorized, map[string]any{"code": 1001, "message": "unauthorized"})
			return
		}
		next(w, r)
	}
}

func (a *App) proxyJSON(w http.ResponseWriter, r *http.Request, baseURL string) bool {
	if baseURL == "" {
		return false
	}
	u := strings.TrimRight(baseURL, "/") + r.URL.Path
	if r.URL.RawQuery != "" {
		u += "?" + r.URL.RawQuery
	}
	req, err := http.NewRequestWithContext(r.Context(), http.MethodGet, u, nil)
	if err != nil {
		jsonReply(w, 500, map[string]any{"code": 2001, "message": err.Error()})
		return true
	}
	if tok := strings.TrimPrefix(r.Header.Get("Authorization"), "Bearer "); tok != "" {
		req.Header.Set("Authorization", "Bearer "+tok)
	}
	resp, err := a.client.Do(req)
	if err != nil {
		jsonReply(w, 502, map[string]any{"code": 2002, "message": "upstream unavailable"})
		return true
	}
	defer resp.Body.Close()
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(resp.StatusCode)
	_, _ = io.Copy(w, resp.Body)
	return true
}

func parseYearMonth(r *http.Request) (int, time.Month, error) {
	now := time.Now()
	year, month := now.Year(), now.Month()
	if s := r.URL.Query().Get("year"); s != "" {
		v, err := strconv.Atoi(s)
		if err != nil {
			return 0, 0, err
		}
		year = v
	}
	if s := r.URL.Query().Get("month"); s != "" {
		v, err := strconv.Atoi(s)
		if err != nil || v < 1 || v > 12 {
			return 0, 0, errors.New("invalid month")
		}
		month = time.Month(v)
	}
	return year, month, nil
}

/* ---------- 工时：优先代理上游，其次 PingCode 网页 API，最后 MySQL ---------- */

func (a *App) worktimeSQL() (string, error) {
	if a.cfg.Worktime.MySQL.Query != "" {
		return a.cfg.Worktime.MySQL.Query, nil
	}
	return "", fmt.Errorf("未配置工时 SQL 查询模板")
}

// 渲染 SQL 模板：可用 {{.Year}} {{.Month}} {{.EmployeeNo}} {{.Start}} {{.End}}
func (a *App) renderWorktimeSQL(year int, month time.Month) (string, error) {
	raw, err := a.worktimeSQL()
	if err != nil {
		return "", err
	}
	start := time.Date(year, month, 1, 0, 0, 0, 0, time.Local)
	end := time.Date(year, month+1, 0, 0, 0, 0, 0, time.Local)
	t, err := template.New("worktime").Parse(raw)
	if err != nil {
		return "", fmt.Errorf("解析工时 SQL 模板失败：%w", err)
	}
	var buf strings.Builder
	err = t.Execute(&buf, map[string]any{
		"Year":       year,
		"Month":      int(month),
		"EmployeeNo": a.cfg.Worktime.MySQL.EmployeeNo,
		"Start":      start.Format("2006-01-02"),
		"End":        end.Format("2006-01-02"),
	})
	if err != nil {
		return "", err
	}
	return buf.String(), nil
}

func columnIndex(cols []string, names ...string) int {
	for _, n := range names {
		for i, c := range cols {
			if strings.EqualFold(c, n) {
				return i
			}
		}
	}
	return -1
}

// 执行 SQL：期望返回日期列 + 工时列（列名见 columnIndex），多余列忽略
func (a *App) queryWorktime(ctx context.Context, year int, month time.Month) ([]map[string]any, float64, error) {
	if a.worktimeDB == nil {
		return nil, 0, errors.New("worktime.mysql.dsn 未配置")
	}
	sqlText, err := a.renderWorktimeSQL(year, month)
	if err != nil {
		return nil, 0, err
	}
	timeout := time.Duration(a.cfg.Worktime.MySQL.TimeoutSec) * time.Second
	if timeout <= 0 {
		timeout = 10 * time.Second
	}
	qctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	rows, err := a.worktimeDB.QueryContext(qctx, sqlText)
	if err != nil {
		return nil, 0, err
	}
	defer rows.Close()
	cols, err := rows.Columns()
	if err != nil {
		return nil, 0, err
	}
	di := columnIndex(cols, "date", "work_date", "day", "workday", "dt")
	hi := columnIndex(cols, "hours", "work_hours", "duration", "worktime", "hours_recorded")
	if di < 0 || hi < 0 {
		if len(cols) < 2 {
			return nil, 0, fmt.Errorf("SQL 至少要返回两列（日期、工时），实际列：%v", cols)
		}
		di, hi = 0, 1
	}
	days := make([]map[string]any, 0)
	total := 0.0
	vals := make([]any, len(cols))
	ptrs := make([]any, len(cols))
	for i := range vals {
		ptrs[i] = &vals[i]
	}
	for rows.Next() {
		if err := rows.Scan(ptrs...); err != nil {
			return nil, 0, err
		}
		dateStr := fmt.Sprintf("%v", vals[di])
		hours := toFloat(vals[hi])
		day := 0
		if len(dateStr) >= 10 {
			fmt.Sscanf(dateStr[8:10], "%d", &day)
			dateStr = dateStr[:10]
		}
		days = append(days, map[string]any{"date": dateStr, "day": day, "hours": hours, "recorded_hours": hours})
		total += hours
	}
	if err := rows.Err(); err != nil {
		return nil, 0, err
	}
	return days, total, nil
}

func toFloat(v any) float64 {
	switch t := v.(type) {
	case nil:
		return 0
	case float64:
		return t
	case float32:
		return float64(t)
	case int64:
		return float64(t)
	case int:
		return float64(t)
	case []byte:
		f, _ := strconv.ParseFloat(string(t), 64)
		return f
	case string:
		f, _ := strconv.ParseFloat(t, 64)
		return f
	case time.Time:
		return 0
	}
	f, _ := strconv.ParseFloat(fmt.Sprintf("%v", v), 64)
	return f
}

// 单日法定工作日表（接口节日不可用时的回落口径），与固件保持一致。
// 2025-2026 为国务院发布的放假安排转写；2027 起由 timor.tech 整年接口自动覆盖。
type cnHolidayRange struct{ year, startMonth, startDay, endMonth, endDay int }

var cnHolidays = []cnHolidayRange{
	{2025, 1, 1, 1, 1}, {2025, 1, 28, 2, 4}, {2025, 4, 4, 4, 6},
	{2025, 5, 1, 5, 5}, {2025, 5, 31, 6, 2}, {2025, 10, 1, 10, 8},
	{2026, 1, 1, 1, 3}, {2026, 2, 15, 2, 23}, {2026, 4, 4, 4, 6},
	{2026, 5, 1, 5, 5}, {2026, 6, 19, 6, 21}, {2026, 9, 25, 9, 27},
	{2026, 10, 1, 10, 7},
}

type cnWorkday struct{ year, month, day int }

var cnMakeupWorkdays = []cnWorkday{
	{2025, 1, 26}, {2025, 2, 8}, {2025, 4, 27}, {2025, 9, 28}, {2025, 10, 11},
	{2026, 1, 4}, {2026, 2, 14}, {2026, 2, 28}, {2026, 5, 9}, {2026, 9, 20},
	{2026, 10, 10},
}

func inDayRange(month, day, startMonth, startDay, endMonth, endDay int) bool {
	if startMonth == endMonth {
		return month == startMonth && day >= startDay && day <= endDay
	}
	if month == startMonth {
		return day >= startDay
	}
	if month == endMonth {
		return day <= endDay
	}
	return month > startMonth && month < endMonth
}

// cnWorkdayAt 判断某天是否法定工作日：调休上班日算上班，法定节假日与自然周末不算。
func cnWorkdayAt(t time.Time) bool {
	year, month, day := t.Year(), int(t.Month()), t.Day()
	for _, w := range cnMakeupWorkdays {
		if w.year == year && w.month == month && w.day == day {
			return true
		}
	}
	for _, h := range cnHolidays {
		if h.year == year && inDayRange(month, day, h.startMonth, h.startDay, h.endMonth, h.endDay) {
			return false
		}
	}
	wd := t.Weekday()
	return wd != time.Saturday && wd != time.Sunday
}

// expectedHours 为当月应收工时 = 法定工作日 × 每日标准工时。
// 节假日来源优先级：timor.tech 整年接口（按年缓存 7 天）→ 内置表 → 自然周末。
func (a *App) expectedHours(ctx context.Context, year int, month time.Month, daily float64) float64 {
	if daily <= 0 {
		daily = 8
	}
	hy := a.holidayYearFor(ctx, year)
	days := time.Date(year, month+1, 0, 0, 0, 0, 0, time.Local).Day()
	total := 0.0
	for d := 1; d <= days; d++ {
		t := time.Date(year, month, d, 0, 0, 0, 0, time.Local)
		workday := false
		if hy.complete {
			workday = isWorkdayAt(t, hy)
		} else {
			workday = cnWorkdayAt(t)
		}
		if workday {
			total += daily
		}
	}
	return total
}

// GET /worktime/summary?year=&month=
// 数据来源优先级：DESKWONG_WORKTIME_UPSTREAM 代理 → PingCode 网页 API → 公司 MySQL
func (a *App) worktime(w http.ResponseWriter, r *http.Request) {
	if upstream := os.Getenv("DESKWONG_WORKTIME_UPSTREAM"); upstream != "" {
		if a.proxyJSON(w, r, upstream) {
			return
		}
	}
	year, month, err := parseYearMonth(r)
	if err != nil {
		jsonReply(w, 400, map[string]any{"code": 2003, "message": err.Error()})
		return
	}
	if a.cfg.Worktime.PingCode.BaseURL != "" && a.cfg.Worktime.PingCode.Username != "" && a.cfg.Worktime.PingCode.Password != "" {
		days, total, err := a.queryPingCodeWorktime(r.Context(), year, month)
		if err != nil {
			slog.Warn("PingCode worktime query failed", "error", err)
			jsonReply(w, http.StatusServiceUnavailable, map[string]any{"code": 2006, "message": "PingCode 工时读取失败"})
			return
		}
		jsonReply(w, 200, map[string]any{"code": 0, "message": "ok", "data": map[string]any{
			"year": year, "month": int(month), "days": days,
			"total_recorded_hours": total,
			"total_expected_hours": a.expectedHours(r.Context(), year, month, a.cfg.Worktime.ExpectedDailyHours),
		}})
		return
	}
	if a.worktimeDB == nil {
		jsonReply(w, http.StatusNotImplemented, map[string]any{
			"code":    2004,
			"message": "工时数据源未配置：请在服务后台 /admin 填写 PingCode 账号密码、MySQL DSN/工号，或设置 DESKWONG_WORKTIME_UPSTREAM",
		})
		return
	}
	days, total, err := a.queryWorktime(r.Context(), year, month)
	if err != nil {
		slog.Warn("worktime query failed", "error", err)
		jsonReply(w, http.StatusServiceUnavailable, map[string]any{"code": 2006, "message": err.Error()})
		return
	}
	jsonReply(w, 200, map[string]any{"code": 0, "message": "ok", "data": map[string]any{
		"year": year, "month": int(month), "days": days,
		"total_recorded_hours": total,
		"total_expected_hours": a.expectedHours(r.Context(), year, month, a.cfg.Worktime.ExpectedDailyHours),
	}})
}

func unixTime(v any) int64 {
	switch t := v.(type) {
	case float64:
		return int64(t)
	case string:
		if n, err := strconv.ParseInt(t, 10, 64); err == nil {
			return n
		}
		if ts, err := time.Parse(time.RFC3339, t); err == nil {
			return ts.Unix()
		}
	}
	return 0
}

type codexAuthFile struct {
	Tokens struct {
		AccessToken string `json:"access_token"`
		AccountID   string `json:"account_id"`
	} `json:"tokens"`
	// 兼容少数直接把 token 放根目录的写法
	AccessToken string `json:"access_token"`
}

func codexCredentials() (string, string, error) {
	if tok := os.Getenv("DESKWONG_CODEX_ACCESS_TOKEN"); tok != "" {
		return tok, os.Getenv("DESKWONG_CODEX_ACCOUNT_ID"), nil
	}
	path := os.Getenv("DESKWONG_CODEX_AUTH_FILE")
	if path == "" {
		home, err := os.UserHomeDir()
		if err != nil {
			return "", "", err
		}
		path = filepath.Join(home, ".codex", "auth.json")
	}
	b, err := os.ReadFile(path)
	if err != nil {
		return "", "", fmt.Errorf("读取 Codex 凭据失败（%s）：%w；可在设备上重新 codex login，或设置 DESKWONG_CODEX_ACCESS_TOKEN", path, err)
	}
	var f codexAuthFile
	if err := json.Unmarshal(b, &f); err != nil {
		return "", "", fmt.Errorf("解析 %s 失败：%w", path, err)
	}
	if f.Tokens.AccessToken != "" {
		return f.Tokens.AccessToken, f.Tokens.AccountID, nil
	}
	if f.AccessToken != "" {
		return f.AccessToken, f.Tokens.AccountID, nil
	}
	return "", "", errors.New("凭据里没有 access_token")
}

type codexWindow struct {
	UsedPercent        float64 `json:"used_percent"`
	LimitWindowSeconds int     `json:"limit_window_seconds"`
	ResetAt            any     `json:"reset_at"`
}

type codexUsage struct {
	PlanType  string `json:"plan_type"`
	RateLimit struct {
		PrimaryWindow   codexWindow `json:"primary_window"`
		SecondaryWindow codexWindow `json:"secondary_window"`
		// 部分账号的模型专用额度（如 codex-spark）
		Additional []struct {
			Name               string  `json:"name"`
			LimitWindowSeconds int     `json:"limit_window_seconds"`
			UsedPercent        float64 `json:"used_percent"`
			ResetAt            any     `json:"reset_at"`
		} `json:"additional_rate_limits"`
	} `json:"rate_limit"`
}

// 把 ChatGPT /backend-api/wham/usage 的响应归一化成设备期望的 providers 结构
func codexProviders(u codexUsage) []map[string]any {
	providers := []map[string]any{}
	add := func(id string, seconds int, used float64, reset any) {
		if seconds <= 0 {
			return
		}
		providers = append(providers, map[string]any{
			"id":                id,
			"name":              "ChatGPT/Codex",
			"window_minutes":    seconds / 60,
			"remaining_percent": 100 - used,
			"resets_at":         unixTime(reset),
			"status":            "normal",
		})
	}
	add("chatgpt_5h", u.RateLimit.PrimaryWindow.LimitWindowSeconds, u.RateLimit.PrimaryWindow.UsedPercent, u.RateLimit.PrimaryWindow.ResetAt)
	add("chatgpt_weekly", u.RateLimit.SecondaryWindow.LimitWindowSeconds, u.RateLimit.SecondaryWindow.UsedPercent, u.RateLimit.SecondaryWindow.ResetAt)
	for _, extra := range u.RateLimit.Additional {
		if extra.Name == "" {
			continue
		}
		add("codex_"+strings.ToLower(strings.ReplaceAll(extra.Name, " ", "_")),
			extra.LimitWindowSeconds, extra.UsedPercent, extra.ResetAt)
	}
	return providers
}

func fetchCodexUsage(ctx context.Context, client *http.Client) ([]byte, error) {
	tok, accountID, err := codexCredentials()
	if err != nil {
		return nil, err
	}
	usageURL := os.Getenv("DESKWONG_CODEX_USAGE_URL")
	if usageURL == "" {
		usageURL = codexUsageURL
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, usageURL, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Authorization", "Bearer "+tok)
	if accountID != "" {
		req.Header.Set("ChatGPT-Account-Id", accountID)
	}
	req.Header.Set("User-Agent", "codex-cli deskwong-service/"+version)
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode == http.StatusUnauthorized || resp.StatusCode == http.StatusForbidden {
		return nil, fmt.Errorf("Codex 凭据已失效（HTTP %d），请重新 codex login 或更新 DESKWONG_CODEX_ACCESS_TOKEN", resp.StatusCode)
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("ChatGPT 额度接口返回 HTTP %d", resp.StatusCode)
	}
	var u codexUsage
	if err := json.Unmarshal(body, &u); err != nil {
		return nil, fmt.Errorf("解析 ChatGPT 额度接口失败：%w", err)
	}
	out, err := json.Marshal(map[string]any{
		"code": 0, "message": "ok",
		"data": map[string]any{
			"updated_at": time.Now().Unix(),
			"plan_type":  u.PlanType,
			"providers":  codexProviders(u),
		},
	})
	if err != nil {
		return nil, err
	}
	return out, nil
}

// GET /ai/usage
// 优先代理 DESKWONG_AI_UPSTREAM（自建适配器）；否则直接抓 ChatGPT/Codex 额度。
func (a *App) aiUsage(w http.ResponseWriter, r *http.Request) {
	if upstream := os.Getenv("DESKWONG_AI_UPSTREAM"); upstream != "" {
		if a.proxyJSON(w, r, upstream) {
			return
		}
	}
	a.aiMu.Lock()
	defer a.aiMu.Unlock()
	if a.client == nil {
		jsonReply(w, http.StatusServiceUnavailable, map[string]any{"code": 2005, "message": "http client not initialized"})
		return
	}
	if a.aiCached != nil && time.Since(a.aiCacheAt) < aiCacheTTL {
		w.Header().Set("Content-Type", "application/json; charset=utf-8")
		_, _ = w.Write(a.aiCached)
		return
	}
	body, err := fetchCodexUsage(r.Context(), a.client)
	if err != nil {
		slog.Warn("codex usage fetch failed", "error", err)
		jsonReply(w, http.StatusServiceUnavailable, map[string]any{"code": 2005, "message": err.Error()})
		return
	}
	a.aiCached = body
	a.aiCacheAt = time.Now()
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	_, _ = w.Write(body)
}

func main() {
	path := os.Getenv("DESKWONG_CONFIG")
	if path == "" {
		path = "/app/conf/config.yml"
	}
	seed, err := loadConfig(path)
	if err != nil {
		slog.Error("load config", "error", err)
		os.Exit(1)
	}
	dbPath := os.Getenv("DESKWONG_SETTINGS_DB")
	if dbPath == "" {
		dbPath = "/app/data/settings.sqlite"
	}
	store, err := openSettings(dbPath, seed)
	if err != nil {
		slog.Error("open settings", "error", err)
		os.Exit(1)
	}
	defer store.Close()
	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer cancel()
	for ctx.Err() == nil {
		cfg, err := readSettings(store)
		if err != nil {
			slog.Error("read settings", "error", err)
			return
		}
		// 管理账号密码：SQLite 里没有时回落到 config.yml，保证旧库升级后仍能登录
		if cfg.Server.Username == "" {
			cfg.Server.Username = seed.Server.Username
		}
		if cfg.Server.Password == "" {
			cfg.Server.Password = seed.Server.Password
		}
		if cfg.Server.Username == "" {
			cfg.Server.Username = "admin"
		}
		if cfg.Server.Password == "" {
			cfg.Server.Password = "admin"
		}
		// PingCode / MySQL 凭据只以 config.yml 为权威源（不在 /admin 编辑）：
		// 每次启动或保存重载时用 config.yml 覆盖 SQLite 中的历史 seed 值。
		pc := seed.Worktime.PingCode
		if pc.TimeoutSec <= 0 {
			pc.TimeoutSec = 20
		}
		cfg.Worktime.PingCode = pc
		my := seed.Worktime.MySQL
		if my.TimeoutSec <= 0 {
			my.TimeoutSec = 10
		}
		cfg.Worktime.MySQL = my
		setupLogger(cfg)
		if err := runService(ctx, cfg, store); err != nil {
			slog.Error("service", "error", err)
			return
		}
	}
}

func runService(parent context.Context, cfg Config, store *SettingsStore) error {
	ctx, cancel := context.WithCancel(parent)
	defer cancel()
	app := &App{cfg: cfg, settings: store, restart: cancel, client: &http.Client{Timeout: 12 * time.Second}}
	if cfg.Worktime.MySQL.DSN != "" {
		wdb, err := sql.Open("mysql", cfg.Worktime.MySQL.DSN)
		if err != nil {
			slog.Error("worktime mysql", "error", err)
			return err
		}
		wdb.SetMaxOpenConns(4)
		wdb.SetMaxIdleConns(1)
		wdb.SetConnMaxLifetime(30 * time.Minute)
		app.worktimeDB = wdb
		defer wdb.Close()
		slog.Info("worktime mysql configured", "employee", cfg.Worktime.MySQL.EmployeeNo)
	}

	mux := http.NewServeMux()
	app.registerAdmin(mux)
	mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
		status := map[string]any{"status": "ok", "version": version}
		jsonReply(w, 200, status)
	})
	mux.HandleFunc("GET /worktime/summary", app.authorize(app.worktime))
	mux.HandleFunc("GET /ai/usage", app.authorize(app.aiUsage))

	server := &http.Server{Addr: cfg.Server.Listen, Handler: mux, ReadHeaderTimeout: 5 * time.Second}
	go func() {
		<-ctx.Done()
		shutdown, done := context.WithTimeout(context.Background(), 5*time.Second)
		defer done()
		_ = server.Shutdown(shutdown)
	}()
	slog.Info("deskwong service started", "listen", cfg.Server.Listen, "version", version)
	if err := server.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
		slog.Error("http server", "error", err)
		return err
	}
	return nil
}
