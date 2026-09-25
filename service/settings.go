package main

import (
	"database/sql"
	_ "embed"
	"encoding/json"
	"errors"
	"io"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"text/template"
	"time"

	mysql "github.com/go-sql-driver/mysql"
	_ "modernc.org/sqlite"
)

//go:embed admin.html
var adminHTML string

func openSettings(path string, seed Config) (*sql.DB, error) {
	if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
		return nil, err
	}
	db, err := sql.Open("sqlite", path)
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(1)
	for _, q := range []string{"PRAGMA journal_mode=WAL", "PRAGMA synchronous=FULL", "PRAGMA busy_timeout=5000", "CREATE TABLE IF NOT EXISTS settings (id INTEGER PRIMARY KEY CHECK(id=1), body TEXT NOT NULL)"} {
		if _, err = db.Exec(q); err != nil {
			db.Close()
			return nil, err
		}
	}
	if seed.Worktime.ExpectedDailyHours <= 0 {
		seed.Worktime.ExpectedDailyHours = 8
	}
	if seed.Worktime.MySQL.TimeoutSec <= 0 {
		seed.Worktime.MySQL.TimeoutSec = 10
	}
	if seed.Worktime.MySQL.QueryFile == "" {
		seed.Worktime.MySQL.QueryFile = "/app/conf/worktime.sql"
	}
	body, err := json.Marshal(seed)
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
	return db, nil
}

func readSettings(db *sql.DB) (Config, error) {
	var cfg Config
	var body string
	err := db.QueryRow("SELECT body FROM settings WHERE id=1").Scan(&body)
	if err == nil {
		err = json.Unmarshal([]byte(body), &cfg)
	}
	return cfg, err
}

func validateSettings(cfg Config) error {
	if _, _, err := net.SplitHostPort(cfg.Server.Listen); err != nil {
		return errors.New("监听地址格式应为 :8001 或 IP:端口")
	}
	if strings.TrimSpace(cfg.Server.Token) == "" {
		return errors.New("管理 Token 不能为空")
	}
	if cfg.Worktime.ExpectedDailyHours <= 0 || cfg.Worktime.ExpectedDailyHours > 24 {
		return errors.New("每日工时须大于 0 且不超过 24")
	}
	if cfg.Worktime.MySQL.TimeoutSec < 1 || cfg.Worktime.MySQL.TimeoutSec > 120 {
		return errors.New("查询超时须为 1–120 秒")
	}
	if cfg.Worktime.MySQL.DSN != "" {
		if _, err := mysql.ParseDSN(cfg.Worktime.MySQL.DSN); err != nil {
			return errors.New("MySQL DSN 格式无效")
		}
	}
	if _, err := template.New("worktime").Parse(cfg.Worktime.MySQL.Query); err != nil {
		return errors.New("SQL 模板语法无效")
	}
	for _, cidr := range cfg.Server.AllowCIDR {
		if _, _, err := net.ParseCIDR(cidr); err != nil {
			return errors.New("访问网段格式无效")
		}
	}
	return nil
}

// Only the primary server token can edit settings; device tokens cannot administer.
func (a *App) adminAuth(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store")
		if !a.clientAllowed(r) || a.cfg.Server.Token == "" || r.Header.Get("Authorization") != "Bearer "+a.cfg.Server.Token {
			jsonReply(w, 401, map[string]any{"message": "需要管理 Token"})
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
		cfg.Server.Tokens = nil // additional device tokens are retained, never exposed
		if cfg.Worktime.StaticToken != "" {
			cfg.Worktime.StaticToken = "******"
		}
		if cfg.Worktime.MySQL.DSN != "" {
			cfg.Worktime.MySQL.DSN = "******"
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
		cfg.Server.Tokens = old.Server.Tokens
		if cfg.Server.Token == "******" || cfg.Server.Token == "" {
			cfg.Server.Token = old.Server.Token
		}
		if cfg.Worktime.StaticToken == "******" {
			cfg.Worktime.StaticToken = old.Worktime.StaticToken
		}
		if cfg.Worktime.MySQL.DSN == "******" {
			cfg.Worktime.MySQL.DSN = old.Worktime.MySQL.DSN
		}
		if err = validateSettings(cfg); err != nil {
			jsonReply(w, 400, map[string]any{"message": err.Error()})
			return
		}
		body, err := json.Marshal(cfg)
		if err == nil {
			_, err = a.settings.Exec("UPDATE settings SET body=? WHERE id=1", string(body))
		}
		if err != nil {
			jsonReply(w, 500, map[string]any{"message": "写入 SQLite 失败，配置未保存"})
			return
		}
		jsonReply(w, 200, map[string]any{"message": "已保存到 SQLite，重启服务后生效", "restart_required": true})
	}))
	mux.HandleFunc("POST /api/restart", a.adminAuth(func(w http.ResponseWriter, r *http.Request) {
		jsonReply(w, 200, map[string]any{"message": "服务正在重启；如更改管理 Token，请用新 Token 登录"})
		time.AfterFunc(300*time.Millisecond, a.restart)
	}))
}
