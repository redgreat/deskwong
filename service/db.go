package main

import (
	"database/sql"
	"os"
	"path/filepath"

	_ "modernc.org/sqlite"
)

/* 数据卷上的 SQLite 现在只承载「设备上报的小智上下文快照」这一张表。
 *
 * 过去这里还存了一份可在线编辑的配置（settings 表 + /admin 后台 + 设置读写接口），
 * 现已整体移除：配置以 conf/config.yml 为唯一权威源，不支持运行时在线编辑，
 * 也不需要 DESKWONG_SETTINGS_KEY 之类的加密密钥。 */
func openDB(path string) (*sql.DB, error) {
	if err := os.MkdirAll(filepath.Dir(path), 0700); err != nil {
		return nil, err
	}
	db, err := sql.Open("sqlite", path)
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(1)
	for _, q := range []string{
		"PRAGMA journal_mode=WAL",
		"PRAGMA synchronous=FULL",
		"PRAGMA busy_timeout=5000",
		"CREATE TABLE IF NOT EXISTS voice_context_snapshots (device_id TEXT PRIMARY KEY, observed_at INTEGER NOT NULL, received_at INTEGER NOT NULL, body TEXT NOT NULL)",
	} {
		if _, err = db.Exec(q); err != nil {
			db.Close()
			return nil, err
		}
	}
	if err = os.Chmod(path, 0600); err != nil {
		db.Close()
		return nil, err
	}
	return db, nil
}
