package main

import (
	"bytes"
	"context"
	"crypto/md5"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"net/url"
	"sort"
	"strings"
	"time"
)

type pingCodeEnvelope struct {
	Code    int             `json:"code"`
	Data    json.RawMessage `json:"data"`
	Value   json.RawMessage `json:"value"`
	Message string          `json:"message"`
}

type pingCodeLoginValue struct {
	AccessToken string `json:"access_token"`
}

type pingCodeWorkloadPage struct {
	Value []struct {
		RegisterDate int64   `json:"register_date"`
		ManHour      float64 `json:"man_hour"`
	} `json:"value"`
	Count     int `json:"count"`
	PageIndex int `json:"page_index"`
	PageSize  int `json:"page_size"`
	PageCount int `json:"page_count"`
}

func (a *App) pingCodeRequest(ctx context.Context, method, path, token string, payload any, out any) error {
	base, err := url.Parse(strings.TrimRight(a.cfg.Worktime.PingCode.BaseURL, "/"))
	if err != nil || base.Scheme != "https" || base.Host == "" {
		return errors.New("PingCode 地址必须是有效 HTTPS 地址")
	}
	u, err := base.Parse(path)
	if err != nil || u.Host != base.Host {
		return errors.New("PingCode 请求地址无效")
	}
	var body bytes.Buffer
	if payload != nil {
		if err := json.NewEncoder(&body).Encode(payload); err != nil {
			return err
		}
	}
	req, err := http.NewRequestWithContext(ctx, method, u.String(), &body)
	if err != nil {
		return err
	}
	req.Header.Set("Accept", "application/json")
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("User-Agent", "deskwong-service/"+version)
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}
	client := *a.client
	client.Timeout = 0 // queryPingCodeWorktime 的 context 统一控制后台可配置超时。
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("PingCode HTTP %d", resp.StatusCode)
	}
	if err := json.NewDecoder(resp.Body).Decode(out); err != nil {
		return fmt.Errorf("解析 PingCode 响应失败: %w", err)
	}
	return nil
}

func (a *App) pingCodeLogin(ctx context.Context) (string, error) {
	sum := md5.Sum([]byte(a.cfg.Worktime.PingCode.Password)) // PingCode 网页当前协议要求 MD5；HTTPS 仍是传输保护边界。
	payload := map[string]string{
		"signin_name": a.cfg.Worktime.PingCode.Username,
		"password":    hex.EncodeToString(sum[:]),
	}
	var envelope pingCodeEnvelope
	if err := a.pingCodeRequest(ctx, http.MethodPost, "/api/typhon/team/signin", "", payload, &envelope); err != nil {
		return "", err
	}
	raw := envelope.Value
	if len(raw) == 0 || string(raw) == "null" {
		raw = envelope.Data
	}
	var value pingCodeLoginValue
	if err := json.Unmarshal(raw, &value); err != nil {
		return "", errors.New("无法解析 PingCode 登录响应")
	}
	if value.AccessToken == "" {
		var wrapped struct {
			Value pingCodeLoginValue `json:"value"`
		}
		_ = json.Unmarshal(raw, &wrapped)
		value = wrapped.Value
	}
	if value.AccessToken == "" {
		return "", errors.New("PingCode 登录未返回 access_token")
	}
	return value.AccessToken, nil
}

func (a *App) queryPingCodeWorktime(ctx context.Context, year int, month time.Month) ([]map[string]any, float64, error) {
	timeout := time.Duration(a.cfg.Worktime.PingCode.TimeoutSec) * time.Second
	if timeout <= 0 {
		timeout = 20 * time.Second
	}
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	token, err := a.pingCodeLogin(ctx)
	if err != nil {
		return nil, 0, err
	}
	loc, err := time.LoadLocation("Asia/Shanghai")
	if err != nil {
		loc = time.FixedZone("CST", 8*60*60)
	}
	start := time.Date(year, month, 1, 0, 0, 0, 0, loc)
	end := time.Date(year, month+1, 1, 0, 0, 0, 0, loc).Add(-time.Second)
	hoursByDate := map[string]float64{}
	for page := 0; ; page++ {
		payload := map[string]any{"query": map[string]any{
			"conditions":          []any{},
			"search":              map[string]any{"keywords": "", "scopes": []string{"identifier", "title"}},
			"register_date_range": map[string]int64{"from": start.Unix(), "to": end.Unix()},
			"sort_by":             "recorded_at", "sort_direction": -1,
			"pagination": map[string]any{"to": map[string]int{"pi": page, "ps": 100}},
		}}
		var envelope pingCodeEnvelope
		if err := a.pingCodeRequest(ctx, http.MethodPost, "/api/ladon/workload-logs/mine", token, payload, &envelope); err != nil {
			return nil, 0, err
		}
		raw := envelope.Data
		if len(raw) == 0 || string(raw) == "null" {
			raw = envelope.Value
		}
		var result pingCodeWorkloadPage
		if err := json.Unmarshal(raw, &result); err != nil {
			return nil, 0, fmt.Errorf("解析 PingCode 工时页失败: %w", err)
		}
		for _, item := range result.Value {
			t := time.Unix(item.RegisterDate, 0).In(loc)
			if !t.Before(start) && !t.After(end) {
				hoursByDate[t.Format("2006-01-02")] += item.ManHour
			}
		}
		if result.PageCount <= page+1 || len(result.Value) == 0 {
			break
		}
	}
	keys := make([]string, 0, len(hoursByDate))
	for date := range hoursByDate {
		keys = append(keys, date)
	}
	sort.Strings(keys)
	days := make([]map[string]any, 0, len(keys))
	total := 0.0
	for _, date := range keys {
		hours := hoursByDate[date]
		day, _ := time.ParseInLocation("2006-01-02", date, loc)
		days = append(days, map[string]any{"date": date, "day": day.Day(), "hours": hours, "recorded_hours": hours})
		total += hours
	}
	return days, total, nil
}
