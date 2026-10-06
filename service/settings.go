package main

import (
	"database/sql"
	_ "embed"
	"encoding/json"
	"errors"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
	"strings"
	"time"

	_ "modernc.org/sqlite"
)

//go:embed admin.html
var adminHTML string

type SettingsStore struct {
	*sql.DB
	cipher *settingsCipher
}

func openSettings(path string, seed Config) (*SettingsStore, error) {
	if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
		return nil, err
	}
	cipher, err := loadSettingsCipher(path)
	if err != nil {
		return nil, err
	}
	db, err := sql.Open("sqlite", path)
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(1)
	for _, q := range []string{"PRAGMA journal_mode=WAL", "PRAGMA synchronous=FULL", "PRAGMA busy_timeout=5000", "CREATE TABLE IF NOT EXISTS settings (id INTEGER PRIMARY KEY CHECK(id=1), body TEXT NOT NULL)", "CREATE TABLE IF NOT EXISTS voice_context_snapshots (device_id TEXT PRIMARY KEY, observed_at INTEGER NOT NULL, received_at INTEGER NOT NULL, body TEXT NOT NULL)"} {
		if _, err = db.Exec(q); err != nil {
			db.Close()
			return nil, err
		}
	}
	if seed.Worktime.PingCode.TimeoutSec <= 0 {
		seed.Worktime.PingCode.TimeoutSec = 20
	}
	store := &SettingsStore{DB: db, cipher: cipher}
	storedSeed, err := cryptConfig(seed, cipher, true)
	if err != nil {
		db.Close()
		return nil, err
	}
	body, err := json.Marshal(storedSeed)
	if err == nil {
		_, err = db.Exec("INSERT OR IGNORE INTO settings(id,body) VALUES(1,?)", string(body))
	}
	if err != nil {
		db.Close()
		return nil, err
	}
	if err = os.Chmod(path, 0600); err != nil {
		db.Close()
		return nil, err
	}
	var current string
	if err := db.QueryRow("SELECT body FROM settings WHERE id=1").Scan(&current); err != nil {
		db.Close()
		return nil, err
	}
	var currentCfg Config
	if err := json.Unmarshal([]byte(current), &currentCfg); err != nil {
		db.Close()
		return nil, err
	}
	plainCfg, err := cryptConfig(currentCfg, cipher, false)
	if err != nil {
		db.Close()
		return nil, err
	}
	if err := writeSettings(store, plainCfg); err != nil {
		db.Close()
		return nil, err
	}
	return store, nil
}

func readSettings(store *SettingsStore) (Config, error) {
	var cfg Config
	var body string
	err := store.QueryRow("SELECT body FROM settings WHERE id=1").Scan(&body)
	if err == nil {
		err = json.Unmarshal([]byte(body), &cfg)
	}
	if err != nil {
		return cfg, err
	}
	return cryptConfig(cfg, store.cipher, false)
}

func writeSettings(store *SettingsStore, cfg Config) error {
	stored, err := cryptConfig(cfg, store.cipher, true)
	if err != nil {
		return err
	}
	body, err := json.Marshal(stored)
	if err == nil {
		_, err = store.Exec("UPDATE settings SET body=? WHERE id=1", string(body))
	}
	return err
}

func validateSettings(cfg Config) error {
	if _, _, err := net.SplitHostPort(cfg.Server.Listen); err != nil {
		return errors.New("监听地址格式应为 :8001 或 IP:端口")
	}
	if strings.TrimSpace(cfg.Server.Token) == "" {
		return errors.New("设备访问 Token 不能为空")
	}
	if strings.TrimSpace(cfg.Server.Username) == "" {
		return errors.New("管理账号不能为空")
	}
	if strings.TrimSpace(cfg.Server.Password) == "" {
		return errors.New("管理密码不能为空")
	}
	if cfg.Worktime.PingCode.TimeoutSec < 1 || cfg.Worktime.PingCode.TimeoutSec > 120 {
		return errors.New("PingCode 超时须为 1–120 秒")
	}
	if cfg.Worktime.PingCode.BaseURL != "" {
		u, err := url.Parse(cfg.Worktime.PingCode.BaseURL)
		if err != nil || u.Scheme != "https" || u.Host == "" || u.User != nil {
			return errors.New("PingCode 地址必须是有效 HTTPS 地址且不能包含账号密码")
		}
		if cfg.Worktime.PingCode.Username == "" || cfg.Worktime.PingCode.Password == "" {
			return errors.New("启用 PingCode 时账号和密码不能为空")
		}
	}
	return nil
}

// 管理后台用账号密码（HTTP Basic）登录；设备 Token 只能调数据接口，不能管理配置。
func (a *App) adminAuth(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		user, pass, ok := r.BasicAuth()
		if !ok || user != a.cfg.Server.Username || pass != a.cfg.Server.Password || a.cfg.Server.Password == "" {
			w.Header().Set("WWW-Authenticate", `Basic realm="deskwong-admin"`)
			jsonReply(w, 401, map[string]any{"message": "账号或密码错误"})
			return
		}
		next(w, r)
	}
}

func (a *App) registerAdmin(mux *http.ServeMux) {
	mux.HandleFunc("GET /admin", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "text/html; charset=utf-8")
		w.Header().Set("Cache-Control", "no-store")
		io.WriteString(w, adminHTML)
	})
	mux.HandleFunc("GET /api/settings", a.adminAuth(func(w http.ResponseWriter, r *http.Request) {
		cfg, err := readSettings(a.settings)
		if err != nil {
			jsonReply(w, 500, map[string]any{"message": "读取配置失败"})
			return
		}
		if cfg.Server.Token != "" {
			cfg.Server.Token = "******"
		}
		if cfg.Server.Password != "" {
			cfg.Server.Password = "******"
		}
		if cfg.Context.ReportToken != "" {
			cfg.Context.ReportToken = "******"
		}
		if cfg.Context.ReadToken != "" {
			cfg.Context.ReadToken = "******"
		}
		if cfg.Worktime.PingCode.Password != "" {
			cfg.Worktime.PingCode.Password = "******"
		}
		jsonReply(w, 200, map[string]any{"data": cfg})
	}))
	mux.HandleFunc("PUT /api/settings", a.adminAuth(func(w http.ResponseWriter, r *http.Request) {
		r.Body = http.MaxBytesReader(w, r.Body, 128*1024)
		var cfg Config
		dec := json.NewDecoder(r.Body)
		dec.DisallowUnknownFields()
		if err := dec.Decode(&cfg); err != nil {
			jsonReply(w, 400, map[string]any{"message": "配置格式无效或内容过大"})
			return
		}
		var extra any
		if dec.Decode(&extra) != io.EOF {
			jsonReply(w, 400, map[string]any{"message": "只允许一个配置对象"})
			return
		}
		old, err := readSettings(a.settings)
		if err != nil {
			jsonReply(w, 500, map[string]any{"message": "读取配置失败"})
			return
		}
		if cfg.Server.Token == "******" || cfg.Server.Token == "" {
			cfg.Server.Token = old.Server.Token
		}
		if cfg.Server.Password == "******" || cfg.Server.Password == "" {
			cfg.Server.Password = old.Server.Password
		}
		if cfg.Context.ReportToken == "******" || cfg.Context.ReportToken == "" {
			cfg.Context.ReportToken = old.Context.ReportToken
		}
		if cfg.Context.ReadToken == "******" || cfg.Context.ReadToken == "" {
			cfg.Context.ReadToken = old.Context.ReadToken
		}
		if cfg.Worktime.PingCode.Password == "******" {
			cfg.Worktime.PingCode.Password = old.Worktime.PingCode.Password
		}
		if err = validateSettings(cfg); err != nil {
			jsonReply(w, 400, map[string]any{"message": err.Error()})
			return
		}
		err = writeSettings(a.settings, cfg)
		if err != nil {
			jsonReply(w, 500, map[string]any{"message": "写入 SQLite 失败，配置未保存"})
			return
		}
		jsonReply(w, 200, map[string]any{"message": "已保存并自动生效（如账号密码变更，请用新凭据重新登录）", "restart_required": false})
		// 保存成功后自动重载配置，无需手动点“重启服务”
		time.AfterFunc(300*time.Millisecond, a.restart)
	}))
	mux.HandleFunc("POST /api/restart", a.adminAuth(func(w http.ResponseWriter, r *http.Request) {
		jsonReply(w, 200, map[string]any{"message": "服务正在重启；如更改了账号或密码，请用新账号密码重新登录"})
		time.AfterFunc(300*time.Millisecond, a.restart)
	}))
}
