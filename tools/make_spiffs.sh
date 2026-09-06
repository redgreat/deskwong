#!/usr/bin/env bash
# 生成 web 静态资源的 SPIFFS 镜像（需 ESP-IDF 环境）
# 用法：bash tools/make_spiffs.sh
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WEB_DIST="$ROOT/web/dist"
SPIFFS_SIZE=0x7E0000
OUT="$ROOT/firmware/build/spiffs.bin"
TMP="$(mktemp -d)"

echo "==> 构建前端 web/dist ..."
( cd "$ROOT/web" && npm install && npm run build )

echo "==> 打包 SPIFFS（www/） ..."
mkdir -p "$TMP/www"
cp -r "$WEB_DIST"/. "$TMP/www/"
mkdir -p "$(dirname "$OUT")"
python3 "$IDF_PATH/components/spiffs/spiffsgen.py" "$SPIFFS_SIZE" "$TMP" "$OUT"
rm -rf "$TMP"
echo "==> 生成 $OUT"
