package main

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"strings"
)

const encryptedPrefix = "enc:v1:"

type settingsCipher struct{ aead cipher.AEAD }

func loadSettingsCipher(dbPath string) (*settingsCipher, error) {
	var key []byte
	if raw := strings.TrimSpace(os.Getenv("DESKWONG_SETTINGS_KEY")); raw != "" {
		var err error
		key, err = base64.StdEncoding.DecodeString(raw)
		if err != nil || len(key) != 32 {
			key, err = hex.DecodeString(raw)
		}
		if err != nil || len(key) != 32 {
			return nil, errors.New("DESKWONG_SETTINGS_KEY 必须是 32 字节密钥的 Base64 或 64 位十六进制")
		}
	} else {
		keyPath := dbPath + ".key"
		var err error
		key, err = os.ReadFile(keyPath)
		if errors.Is(err, os.ErrNotExist) {
			key = make([]byte, 32)
			if _, err = rand.Read(key); err == nil {
				err = os.WriteFile(keyPath, key, 0600)
			}
		}
		if err != nil {
			return nil, fmt.Errorf("读取或创建配置加密密钥失败: %w", err)
		}
		if len(key) != 32 {
			return nil, errors.New("配置加密密钥文件长度无效")
		}
		if err := os.Chmod(keyPath, 0600); err != nil {
			return nil, err
		}
	}
	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}
	aead, err := cipher.NewGCM(block)
	if err != nil {
		return nil, err
	}
	return &settingsCipher{aead: aead}, nil
}

func (c *settingsCipher) encrypt(value string) (string, error) {
	if value == "" || strings.HasPrefix(value, encryptedPrefix) {
		return value, nil
	}
	nonce := make([]byte, c.aead.NonceSize())
	if _, err := rand.Read(nonce); err != nil {
		return "", err
	}
	sealed := c.aead.Seal(nil, nonce, []byte(value), []byte("deskwong-settings-v1"))
	return encryptedPrefix + base64.RawStdEncoding.EncodeToString(append(nonce, sealed...)), nil
}

func (c *settingsCipher) decrypt(value string) (string, error) {
	if value == "" || !strings.HasPrefix(value, encryptedPrefix) {
		return value, nil
	}
	raw, err := base64.RawStdEncoding.DecodeString(strings.TrimPrefix(value, encryptedPrefix))
	if err != nil || len(raw) < c.aead.NonceSize() {
		return "", errors.New("SQLite 中的加密配置格式无效")
	}
	nonce, ciphertext := raw[:c.aead.NonceSize()], raw[c.aead.NonceSize():]
	plain, err := c.aead.Open(nil, nonce, ciphertext, []byte("deskwong-settings-v1"))
	if err != nil {
		return "", errors.New("无法解密 SQLite 配置，请检查 DESKWONG_SETTINGS_KEY 或密钥文件")
	}
	return string(plain), nil
}

func cryptConfig(cfg Config, c *settingsCipher, encrypt bool) (Config, error) {
	fields := []*string{
		&cfg.Server.Token, &cfg.Server.Password,
		&cfg.Context.ReportToken, &cfg.Context.ReadToken,
		&cfg.Worktime.PingCode.Password,
	}
	for _, field := range fields {
		var err error
		if encrypt {
			*field, err = c.encrypt(*field)
		} else {
			*field, err = c.decrypt(*field)
		}
		if err != nil {
			return Config{}, err
		}
	}
	return cfg, nil
}
