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
	"net/url"
	"os"
	"sort"
	"strconv"
	"strings"
	"time"
)

const (
	aiStatusLive        = "live"
	aiStatusCached      = "cached"
	aiStatusStale       = "stale"
	aiStatusUnavailable = "unavailable"
)

type AIProviderConfig struct {
	ID             string `yaml:"id" json:"id"`
	Enabled        bool   `yaml:"enabled" json:"enabled"`
	Order          int    `yaml:"order" json:"order"`
	RefreshMinutes int    `yaml:"refresh_minutes" json:"refresh_minutes"`
	AuthMode       string `yaml:"auth_mode" json:"auth_mode"`
	Username       string `yaml:"username" json:"username"`
	Password       string `yaml:"password" json:"password"`
	APIBase        string `yaml:"api_base" json:"api_base"`
	APIToken       string `yaml:"api_token" json:"api_token"`
	RefreshToken   string `yaml:"refresh_token" json:"refresh_token"`
}

type AIWindow struct {
	ID               string  `json:"id"`
	Label            string  `json:"label"`
	WindowMinutes    int     `json:"window_minutes,omitempty"`
	RemainingPercent float64 `json:"remaining_percent"`
	ResetsAt         int64   `json:"resets_at,omitempty"`
}

type AIApp struct {
	ID         string     `json:"id"`
	Name       string     `json:"name"`
	Status     string     `json:"status"`
	ObservedAt int64      `json:"observed_at,omitempty"`
	Windows    []AIWindow `json:"windows"`
	Error      string     `json:"-"`
}

var aiCatalog = map[string]string{
	"chatgpt": "GPT",
	"devin":   "Devin",
	"claude":  "Claude",
	"mimo":    "MiMo",
}

var (
	devinWebBase = "https://app.devin.ai"
	mimoOAuthURL = "https://account.xiaomi.com/oauth2/token"
	mimoUsageURL = "https://mimo-server-cn.xiaomimimo.com/api/user/usage"
)

func defaultAIProviders() []AIProviderConfig {
	return []AIProviderConfig{
		{ID: "chatgpt", Enabled: true, Order: 0, RefreshMinutes: 10, AuthMode: "client_login"},
		{ID: "devin", Order: 1, RefreshMinutes: 10, AuthMode: "client_login"},
		{ID: "claude", Order: 2, RefreshMinutes: 10, AuthMode: "external_api"},
		{ID: "mimo", Order: 3, RefreshMinutes: 10, AuthMode: "oauth_refresh"},
	}
}

func normalizeAIConfig(cfg *Config) {
	byID := make(map[string]AIProviderConfig, len(cfg.AIUsage.Providers))
	for _, p := range cfg.AIUsage.Providers {
		byID[p.ID] = p
	}
	defaults := defaultAIProviders()
	for i := range defaults {
		if p, ok := byID[defaults[i].ID]; ok {
			defaults[i] = p
		}
		if defaults[i].RefreshMinutes == 0 {
			defaults[i].RefreshMinutes = 10
		}
	}
	cfg.AIUsage.Providers = defaults
}

func validateAIConfig(cfg Config) error {
	seen := map[string]bool{}
	orders := map[int]bool{}
	enabled := 0
	for _, p := range cfg.AIUsage.Providers {
		if _, ok := aiCatalog[p.ID]; !ok || seen[p.ID] {
			return fmt.Errorf("AI 应用 id 无效或重复: %s", p.ID)
		}
		seen[p.ID] = true
		if p.RefreshMinutes < 1 || p.RefreshMinutes > 1440 {
			return fmt.Errorf("%s 刷新频率必须为 1～1440 分钟", aiCatalog[p.ID])
		}
		if !p.Enabled {
			continue
		}
		enabled++
		if orders[p.Order] {
			return errors.New("已启用 AI 应用的显示顺序不能重复")
		}
		orders[p.Order] = true
		switch p.AuthMode {
		case "client_login":
			if p.ID != "chatgpt" && p.ID != "devin" && p.ID != "mimo" {
				return fmt.Errorf("%s 不支持服务容器本机登录态", aiCatalog[p.ID])
			}
		case "manual_token":
			if p.ID != "devin" || p.Username == "" || p.APIToken == "" {
				return fmt.Errorf("%s 手动令牌模式需要组织 ID 和 Bearer Token", aiCatalog[p.ID])
			}
		case "oauth_refresh":
			if p.ID != "mimo" || p.RefreshToken == "" {
				return errors.New("MiMo OAuth Refresh Token 模式需要 Refresh Token")
			}
		case "password":
			if p.Username == "" || p.Password == "" || p.APIBase == "" {
				return fmt.Errorf("%s 账号密码模式需要账号、密码和 API 地址", aiCatalog[p.ID])
			}
		case "external_api":
			if p.APIBase == "" {
				return fmt.Errorf("%s 外部 API 模式需要 API 地址", aiCatalog[p.ID])
			}
		default:
			return fmt.Errorf("%s 认证方式无效", aiCatalog[p.ID])
		}
	}
	if len(seen) != len(aiCatalog) {
		return errors.New("AI 应用配置必须包含 ChatGPT、Devin、Claude Code 和 MiMo Desktop")
	}
	if enabled < 1 || enabled > 2 {
		return errors.New("必须启用 1～2 个 AI 应用")
	}
	return nil
}

func enabledAIProviders(cfg Config) []AIProviderConfig {
	out := make([]AIProviderConfig, 0, 2)
	for _, p := range cfg.AIUsage.Providers {
		if p.Enabled {
			out = append(out, p)
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].Order < out[j].Order })
	return out
}

func labelForWindow(id string, minutes int) string {
	switch {
	case strings.Contains(id, "daily") || minutes == 1440:
		return "日"
	case strings.Contains(id, "weekly") || minutes == 10080:
		return "周"
	case minutes == 300:
		return "5时"
	default:
		return "窗"
	}
}

func clampPercent(v float64) float64 {
	if v < 0 {
		return 0
	}
	if v > 100 {
		return 100
	}
	return v
}

func normalizeLegacyApp(id string, body []byte) (AIApp, error) {
	var payload struct {
		Code int `json:"code"`
		Data struct {
			Apps      []AIApp `json:"apps"`
			Providers []struct {
				ID               string   `json:"id"`
				Label            string   `json:"label"`
				WindowMinutes    int      `json:"window_minutes"`
				RemainingPercent *float64 `json:"remaining_percent"`
				UsedPercent      *float64 `json:"used_percent"`
				ResetsAt         int64    `json:"resets_at"`
			} `json:"providers"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &payload); err != nil {
		return AIApp{}, err
	}
	if payload.Code != 0 {
		return AIApp{}, fmt.Errorf("上游返回 code=%d", payload.Code)
	}
	if len(payload.Data.Apps) > 0 {
		for _, app := range payload.Data.Apps {
			if app.ID == id {
				app.Name = aiCatalog[id]
				if app.Status == "" {
					app.Status = aiStatusLive
				}
				if app.ObservedAt == 0 {
					app.ObservedAt = time.Now().Unix()
				}
				if len(app.Windows) > 2 {
					app.Windows = app.Windows[:2]
				}
				return app, nil
			}
		}
		return AIApp{}, fmt.Errorf("外部 API 未返回应用 %s", id)
	}
	app := AIApp{ID: id, Name: aiCatalog[id], Status: aiStatusLive, ObservedAt: time.Now().Unix()}
	for _, p := range payload.Data.Providers {
		if len(app.Windows) == 2 {
			break
		}
		remaining := float64(-1)
		if p.RemainingPercent != nil {
			remaining = clampPercent(*p.RemainingPercent)
		} else if p.UsedPercent != nil {
			remaining = clampPercent(100 - *p.UsedPercent)
		}
		label := p.Label
		if label == "" {
			label = labelForWindow(p.ID, p.WindowMinutes)
		}
		app.Windows = append(app.Windows, AIWindow{ID: p.ID, Label: label, WindowMinutes: p.WindowMinutes, RemainingPercent: remaining, ResetsAt: p.ResetsAt})
	}
	if len(app.Windows) == 0 {
		return AIApp{}, errors.New("上游没有返回额度窗口")
	}
	return app, nil
}

func (a *App) fetchExternalAI(ctx context.Context, p AIProviderConfig) (AIApp, error) {
	base := strings.TrimRight(p.APIBase, "/")
	if _, err := url.ParseRequestURI(base); err != nil {
		return AIApp{}, errors.New("外部 API 地址无效")
	}
	endpoint := base
	if !strings.HasSuffix(endpoint, "/ai/usage") {
		endpoint += "/ai/usage"
	}
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return AIApp{}, err
	}
	if p.APIToken != "" {
		req.Header.Set("Authorization", "Bearer "+p.APIToken)
	}
	if p.AuthMode == "password" {
		req.SetBasicAuth(p.Username, p.Password)
	}
	req.Header.Set("User-Agent", "deskwong-service/"+version)
	resp, err := a.client.Do(req)
	if err != nil {
		return AIApp{}, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(io.LimitReader(resp.Body, 1<<20))
	if err != nil {
		return AIApp{}, err
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return AIApp{}, fmt.Errorf("外部 API HTTP %d", resp.StatusCode)
	}
	return normalizeLegacyApp(p.ID, body)
}

func (a *App) fetchAIProvider(ctx context.Context, p AIProviderConfig) (AIApp, error) {
	if p.AuthMode == "client_login" && p.ID == "chatgpt" {
		body, err := fetchCodexUsage(ctx, a.client)
		if err != nil {
			return AIApp{}, err
		}
		return normalizeLegacyApp(p.ID, body)
	}
	if p.ID == "devin" {
		if p.AuthMode == "manual_token" {
			return a.fetchDevinWeb(ctx, p.Username, p.APIToken)
		}
		if p.AuthMode == "client_login" {
			return a.fetchDevinClient(ctx)
		}
	}
	if p.ID == "mimo" {
		if p.AuthMode == "client_login" {
			return a.fetchMiMoClient(ctx)
		}
		if p.AuthMode == "oauth_refresh" {
			return a.fetchMiMoOAuth(ctx, p.RefreshToken)
		}
	}
	return a.fetchExternalAI(ctx, p)
}

func percentValue(v any) (float64, bool) {
	n, ok := v.(float64)
	if !ok || n < 0 {
		return 0, false
	}
	if n > 0 && n < 1 {
		n *= 100
	}
	return clampPercent(n), true
}

func unixOrDate(v any) int64 {
	switch value := v.(type) {
	case float64:
		return int64(value)
	case string:
		if n, err := strconv.ParseInt(value, 10, 64); err == nil {
			return n
		}
		if t, err := time.Parse(time.RFC3339, value); err == nil {
			return t.Unix()
		}
		if t, err := time.ParseInLocation("2006-01-02T15:04:05", value, time.Local); err == nil {
			return t.Unix()
		}
	}
	return 0
}

func (a *App) fetchDevinWeb(ctx context.Context, organization, token string) (AIApp, error) {
	organization = strings.Trim(strings.TrimSpace(organization), "/")
	internalOrg := strings.HasPrefix(organization, "org-") || strings.HasPrefix(organization, "org_") || strings.HasPrefix(organization, "organizations/org-") || strings.HasPrefix(organization, "organizations/org_")
	if strings.HasPrefix(organization, "organizations/") {
		organization = strings.TrimPrefix(organization, "organizations/")
	}
	if strings.HasPrefix(organization, "org/") {
		organization = strings.TrimPrefix(organization, "org/")
	}
	endpoint := strings.TrimRight(devinWebBase, "/") + "/api/" + url.PathEscape(organization) + "/billing/quota/usage"
	req, _ := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	req.Header.Set("Authorization", "Bearer "+strings.TrimPrefix(strings.TrimSpace(token), "Bearer "))
	if internalOrg {
		req.Header.Set("x-cog-org-id", organization)
	}
	req.Header.Set("Accept", "application/json")
	return a.fetchDevinResponse(req)
}

func (a *App) fetchDevinResponse(req *http.Request) (AIApp, error) {
	resp, err := a.client.Do(req)
	if err != nil {
		return AIApp{}, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(io.LimitReader(resp.Body, 1<<20))
	if err != nil {
		return AIApp{}, err
	}
	if resp.StatusCode != http.StatusOK {
		return AIApp{}, fmt.Errorf("Devin 用量接口 HTTP %d", resp.StatusCode)
	}
	var raw map[string]any
	if err := json.Unmarshal(body, &raw); err != nil {
		return AIApp{}, err
	}
	app := AIApp{ID: "devin", Name: aiCatalog["devin"], Status: aiStatusLive, ObservedAt: time.Now().Unix()}
	hide, _ := raw["hide_daily_quota"].(bool)
	add := func(id, label string, used any, reset any) {
		if p, ok := percentValue(used); ok {
			app.Windows = append(app.Windows, AIWindow{ID: id, Label: label, WindowMinutes: map[string]int{"日": 1440, "周": 10080}[label], RemainingPercent: 100 - p, ResetsAt: unixOrDate(reset)})
		}
	}
	if !hide {
		add("devin_daily", "日", raw["daily_percentage"], raw["daily_reset_at"])
	}
	add("devin_weekly", "周", raw["weekly_percentage"], raw["weekly_reset_at"])
	if len(app.Windows) == 0 {
		return AIApp{}, errors.New("Devin 响应没有日/周额度")
	}
	return app, nil
}

func parseSimpleTOML(raw string) map[string]string {
	out := map[string]string{}
	for _, line := range strings.Split(raw, "\n") {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		parts := strings.SplitN(line, "=", 2)
		if len(parts) == 2 {
			out[strings.TrimSpace(parts[0])] = strings.Trim(strings.TrimSpace(parts[1]), "\"'")
		}
	}
	return out
}

func (a *App) fetchDevinClient(ctx context.Context) (AIApp, error) {
	path := os.Getenv("DESKWONG_DEVIN_CREDENTIALS_FILE")
	if path == "" {
		path = "/home/nonroot/.local/share/devin/credentials.toml"
	}
	var key, base string
	if raw, err := os.ReadFile(path); err == nil {
		cfg := parseSimpleTOML(string(raw))
		key = cfg["windsurf_api_key"]
		base = cfg["api_server_url"]
	}
	if key == "" {
		dbPath := os.Getenv("DESKWONG_DEVIN_STATE_DB")
		if dbPath == "" {
			dbPath = "/home/nonroot/.config/Devin/User/globalStorage/state.vscdb"
		}
		db, err := sql.Open("sqlite", "file:"+dbPath+"?mode=ro")
		if err == nil {
			defer db.Close()
			var value string
			if err = db.QueryRowContext(ctx, "SELECT value FROM ItemTable WHERE key='windsurfAuthStatus' LIMIT 1").Scan(&value); err == nil {
				var auth struct {
					APIKey string `json:"apiKey"`
				}
				if json.Unmarshal([]byte(value), &auth) == nil {
					key = strings.TrimSpace(auth.APIKey)
				}
			}
		}
	}
	if base == "" {
		base = os.Getenv("DESKWONG_DEVIN_API_SERVER")
	}
	if key == "" {
		return AIApp{}, errors.New("Devin 客户端登录态不可用：credentials.toml 和 state.vscdb 均未找到 apiKey")
	}
	base = strings.TrimRight(base, "/")
	if base == "" {
		base = "https://server.codeium.com"
	}
	payload, _ := json.Marshal(map[string]any{"metadata": map[string]any{"apiKey": key, "ideName": "devin", "ideVersion": "1.108.2", "extensionName": "devin", "extensionVersion": "1.108.2", "locale": "en"}})
	req, _ := http.NewRequestWithContext(ctx, http.MethodPost, base+"/exa.seat_management_pb.SeatManagementService/GetUserStatus", strings.NewReader(string(payload)))
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("Connect-Protocol-Version", "1")
	resp, err := a.client.Do(req)
	if err != nil {
		return AIApp{}, err
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(io.LimitReader(resp.Body, 1<<20))
	if resp.StatusCode != 200 {
		return AIApp{}, fmt.Errorf("Devin GetUserStatus HTTP %d", resp.StatusCode)
	}
	var root struct {
		UserStatus struct {
			PlanStatus struct {
				Daily       float64 `json:"dailyQuotaRemainingPercent"`
				Weekly      float64 `json:"weeklyQuotaRemainingPercent"`
				DailyReset  int64   `json:"dailyQuotaResetAtUnix"`
				WeeklyReset int64   `json:"weeklyQuotaResetAtUnix"`
				PlanInfo    struct {
					Hide bool `json:"hideDailyQuota"`
				} `json:"planInfo"`
			} `json:"planStatus"`
		} `json:"userStatus"`
	}
	if err := json.Unmarshal(body, &root); err != nil {
		return AIApp{}, err
	}
	ps := root.UserStatus.PlanStatus
	app := AIApp{ID: "devin", Name: aiCatalog["devin"], Status: aiStatusLive, ObservedAt: time.Now().Unix()}
	if !ps.PlanInfo.Hide {
		app.Windows = append(app.Windows, AIWindow{ID: "devin_daily", Label: "日", WindowMinutes: 1440, RemainingPercent: clampPercent(ps.Daily), ResetsAt: ps.DailyReset})
	}
	app.Windows = append(app.Windows, AIWindow{ID: "devin_weekly", Label: "周", WindowMinutes: 10080, RemainingPercent: clampPercent(ps.Weekly), ResetsAt: ps.WeeklyReset})
	return app, nil
}

type mimoCredentials struct {
	Type         string `json:"type"`
	AccessToken  string `json:"access_token"`
	RefreshToken string `json:"refresh_token"`
	ExpiresAt    int64  `json:"expires_at"`
	Key          string `json:"key"`
}

type mimoOAuthResponse struct {
	AccessToken  string `json:"access_token"`
	RefreshToken string `json:"refresh_token"`
	ExpiresIn    int64  `json:"expires_in"`
}

func (a *App) refreshMiMoOAuth(ctx context.Context, refreshToken string) (mimoOAuthResponse, error) {
	form := url.Values{"grant_type": {"refresh_token"}, "refresh_token": {refreshToken}, "client_id": {"mimocode-desktop"}}
	req, _ := http.NewRequestWithContext(ctx, http.MethodPost, mimoOAuthURL, strings.NewReader(form.Encode()))
	req.Header.Set("Content-Type", "application/x-www-form-urlencoded")
	resp, err := a.client.Do(req)
	if err != nil {
		return mimoOAuthResponse{}, err
	}
	defer resp.Body.Close()
	var refreshed mimoOAuthResponse
	err = json.NewDecoder(io.LimitReader(resp.Body, 1<<20)).Decode(&refreshed)
	if err != nil || resp.StatusCode != 200 || refreshed.AccessToken == "" {
		return mimoOAuthResponse{}, fmt.Errorf("刷新 MiMo 登录态失败 HTTP %d", resp.StatusCode)
	}
	if refreshed.RefreshToken == "" {
		refreshed.RefreshToken = refreshToken
	}
	return refreshed, nil
}

func (a *App) fetchMiMoOAuth(ctx context.Context, configuredRefreshToken string) (AIApp, error) {
	a.aiAuthMu.Lock()
	defer a.aiAuthMu.Unlock()
	refreshToken := configuredRefreshToken
	if a.mimoSession.RefreshToken != "" {
		refreshToken = a.mimoSession.RefreshToken
	}
	if a.mimoSession.AccessToken == "" || time.Now().Add(5*time.Minute).After(a.mimoSession.ExpiresAt) {
		refreshed, err := a.refreshMiMoOAuth(ctx, refreshToken)
		if err != nil {
			return AIApp{}, err
		}
		a.mimoSession.AccessToken = refreshed.AccessToken
		a.mimoSession.RefreshToken = refreshed.RefreshToken
		a.mimoSession.ExpiresAt = time.Now().Add(time.Duration(refreshed.ExpiresIn) * time.Second)
		if refreshed.ExpiresIn <= 0 {
			a.mimoSession.ExpiresAt = time.Now().Add(30 * time.Minute)
		}
		if refreshed.RefreshToken != configuredRefreshToken {
			if err = a.persistAIRefreshToken("mimo", refreshed.RefreshToken); err != nil {
				return AIApp{}, fmt.Errorf("保存轮换后的 MiMo Refresh Token 失败: %w", err)
			}
		}
	}
	return a.fetchMiMoUsage(ctx, a.mimoSession.AccessToken)
}

func (a *App) fetchMiMoClient(ctx context.Context) (AIApp, error) {
	path := os.Getenv("DESKWONG_MIMO_AUTH_FILE")
	if path == "" {
		path = "/home/nonroot/.local/share/mimocode/auth.json"
	}
	raw, err := os.ReadFile(path)
	if err != nil {
		return AIApp{}, fmt.Errorf("读取 MiMo 登录态失败: %w", err)
	}
	var auth struct {
		Xiaomi mimoCredentials `json:"xiaomi"`
	}
	if err = json.Unmarshal(raw, &auth); err != nil {
		return AIApp{}, err
	}
	token := auth.Xiaomi.AccessToken
	if auth.Xiaomi.Type == "api" {
		token = auth.Xiaomi.Key
	}
	if token == "" {
		return AIApp{}, errors.New("MiMo 尚未登录或 auth.json 没有 Xiaomi 凭据")
	}
	if auth.Xiaomi.Type == "oauth" && auth.Xiaomi.RefreshToken != "" && (auth.Xiaomi.ExpiresAt == 0 || auth.Xiaomi.ExpiresAt < time.Now().Add(5*time.Minute).UnixMilli()) {
		refreshed, e := a.refreshMiMoOAuth(ctx, auth.Xiaomi.RefreshToken)
		if e != nil {
			return AIApp{}, e
		}
		token = refreshed.AccessToken
	}
	return a.fetchMiMoUsage(ctx, token)
}

func (a *App) fetchMiMoUsage(ctx context.Context, token string) (AIApp, error) {
	req, _ := http.NewRequestWithContext(ctx, http.MethodGet, mimoUsageURL, nil)
	req.Header.Set("Authorization", "Bearer "+token)
	resp, err := a.client.Do(req)
	if err != nil {
		return AIApp{}, err
	}
	defer resp.Body.Close()
	var payload struct {
		Code int `json:"code"`
		Data struct {
			Percent   float64 `json:"percent"`
			ResetDate string  `json:"resetDate"`
		} `json:"data"`
	}
	if err = json.NewDecoder(io.LimitReader(resp.Body, 1<<20)).Decode(&payload); err != nil {
		return AIApp{}, err
	}
	if resp.StatusCode != 200 || payload.Code != 0 {
		return AIApp{}, fmt.Errorf("MiMo 用量接口 HTTP %d code=%d", resp.StatusCode, payload.Code)
	}
	return AIApp{ID: "mimo", Name: aiCatalog["mimo"], Status: aiStatusLive, ObservedAt: time.Now().Unix(), Windows: []AIWindow{{ID: "mimo_current", Label: "额度", RemainingPercent: clampPercent(100 - payload.Data.Percent), ResetsAt: unixOrDate(payload.Data.ResetDate)}}}, nil
}

func (a *App) refreshAIProvider(ctx context.Context, p AIProviderConfig) {
	app, err := a.fetchAIProvider(ctx, p)
	a.aiMu.Lock()
	defer a.aiMu.Unlock()
	if a.aiApps == nil {
		a.aiApps = map[string]AIApp{}
	}
	if err == nil {
		a.aiApps[p.ID] = app
		return
	}
	old, ok := a.aiApps[p.ID]
	if ok && len(old.Windows) > 0 {
		old.Status = aiStatusStale
		old.Error = err.Error()
		a.aiApps[p.ID] = old
	} else {
		a.aiApps[p.ID] = AIApp{ID: p.ID, Name: aiCatalog[p.ID], Status: aiStatusUnavailable, Windows: []AIWindow{}, Error: err.Error()}
	}
	slog.Warn("AI usage refresh failed", "provider", p.ID, "error", err)
}

func (a *App) startAIScheduler(parent context.Context) {
	ctx, cancel := context.WithCancel(parent)
	a.aiCancel = cancel
	for _, provider := range enabledAIProviders(a.cfg) {
		p := provider
		go func() {
			a.refreshAIProvider(ctx, p)
			ticker := time.NewTicker(time.Duration(p.RefreshMinutes) * time.Minute)
			defer ticker.Stop()
			for {
				select {
				case <-ctx.Done():
					return
				case <-ticker.C:
					a.refreshAIProvider(ctx, p)
				}
			}
		}()
	}
}

func (a *App) stopAIScheduler() {
	if a.aiCancel != nil {
		a.aiCancel()
	}
}

func (a *App) aiUsage(w http.ResponseWriter, _ *http.Request) {
	providers := enabledAIProviders(a.cfg)
	a.aiMu.RLock()
	apps := make([]AIApp, 0, len(providers))
	for _, p := range providers {
		if app, ok := a.aiApps[p.ID]; ok {
			apps = append(apps, app)
		} else {
			apps = append(apps, AIApp{ID: p.ID, Name: aiCatalog[p.ID], Status: aiStatusUnavailable, Windows: []AIWindow{}})
		}
	}
	a.aiMu.RUnlock()
	jsonReply(w, http.StatusOK, map[string]any{"code": 0, "message": "ok", "data": map[string]any{"apps": apps}})
}
