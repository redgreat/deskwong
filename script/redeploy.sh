#!/usr/bin/env bash
# =============================================================================
# deskwong 服务部署机侧全量重部署脚本
#
# 功能：停止并删除容器、删除本地旧镜像、重新拉取并部署。
# 不会删除命名卷 deskwong-settings，SQLite 配置与数据都会保留。
#
# 目录结构：
#   service/docker-compose.yml
#   script/redeploy.sh
#
# 用法：
#   ./script/redeploy.sh
#   ./script/redeploy.sh --logs      # 部署成功后继续跟随日志
#   DEPLOY_DIR=/path/to/service ./script/redeploy.sh
# =============================================================================
set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# 目录布局自适应：部署机上脚本与 docker-compose.yml 同级；仓库内脚本在 script/ 下、compose 在 service/
if [[ -z "${DEPLOY_DIR:-}" ]]; then
  if [[ -f "$SCRIPT_DIR/docker-compose.yml" ]]; then
    DEPLOY_DIR="$SCRIPT_DIR"
  else
    DEPLOY_DIR="$(cd "$SCRIPT_DIR/.." && pwd)/service"
  fi
fi
HEALTH_TIMEOUT="${HEALTH_TIMEOUT:-120}"
# 健康检查端口：auto 表示从容器 8001/tcp 的宿主机映射自动解析（部署端口可能不是 8001）
HEALTH_PORT="${HEALTH_PORT:-auto}"
FOLLOW_LOGS=0

c_reset=$'\033[0m'; c_red=$'\033[31m'; c_green=$'\033[32m'; c_yellow=$'\033[33m'; c_blue=$'\033[36m'
if [[ -n "${NO_COLOR:-}" || ! -t 1 ]]; then c_reset=""; c_red=""; c_green=""; c_yellow=""; c_blue=""; fi

log()  { printf '%s[redeploy]%s %s\n' "$c_blue" "$c_reset" "$*"; }
ok()   { printf '%s  ✓%s %s\n' "$c_green" "$c_reset" "$*"; }
warn() { printf '%s  !%s %s\n' "$c_yellow" "$c_reset" "$*" >&2; }
die()  { printf '%s  ✗%s %s\n' "$c_red" "$c_reset" "$*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --logs|-f) FOLLOW_LOGS=1; shift ;;
    -h|--help) sed -n '2,16p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) die "未知参数: $1（用 -h 查看用法）" ;;
  esac
done

[[ -f "$DEPLOY_DIR/docker-compose.yml" ]] || die "未找到 $DEPLOY_DIR/docker-compose.yml"
command -v docker >/dev/null 2>&1 || die "未找到 docker"
docker info >/dev/null 2>&1 || die "Docker daemon 不可用"

if docker compose version >/dev/null 2>&1; then
  COMPOSE=(docker compose)
elif command -v docker-compose >/dev/null 2>&1; then
  COMPOSE=(docker-compose)
else
  die "未找到 docker compose（v2 插件或 docker-compose）"
fi

cd "$DEPLOY_DIR"
mapfile -t IMAGES < <("${COMPOSE[@]}" config --images | sed '/^[[:space:]]*$/d' | sort -u)
[[ ${#IMAGES[@]} -gt 0 ]] || die "未从 docker-compose.yml 解析到镜像"

if command -v curl >/dev/null 2>&1; then
  health_check() { curl -sf "http://127.0.0.1:${HEALTH_PORT}/health" >/dev/null 2>&1; }
elif command -v wget >/dev/null 2>&1; then
  health_check() { wget -q -O /dev/null "http://127.0.0.1:${HEALTH_PORT}/health"; }
else
  warn "未找到 curl 或 wget，仅检查容器运行状态"
  health_check() { return 0; }
fi

log "停止并删除 Compose 容器（保留命名卷）"
"${COMPOSE[@]}" down --remove-orphans

for image in "${IMAGES[@]}"; do
  if docker image inspect "$image" >/dev/null 2>&1; then
    log "删除旧镜像: $image"
    docker image rm -f "$image"
  else
    log "本地镜像不存在，跳过: $image"
  fi
done

log "重新拉取镜像"
"${COMPOSE[@]}" pull

log "重新创建并启动 deskwong-service"
"${COMPOSE[@]}" up -d --remove-orphans --force-recreate

container_id="$("${COMPOSE[@]}" ps -q deskwong-service || true)"
if [[ -z "$container_id" ]]; then
  container_id="$("${COMPOSE[@]}" ps -q | head -n1)"
fi
[[ -n "$container_id" ]] || die "未找到运行中的容器"

# 解析容器 8001/tcp 映射到宿主机的实际端口（如 8030:8001 时打 8030）
if [[ "$HEALTH_PORT" == "auto" ]]; then
  hp="$(docker inspect --format '{{with index .NetworkSettings.Ports "8001/tcp"}}{{(index . 0).HostPort}}{{end}}' "$container_id" 2>/dev/null || true)"
  HEALTH_PORT="${hp:-8001}"
fi
log "健康检查: http://127.0.0.1:${HEALTH_PORT}/health"

log "等待服务就绪（最多 ${HEALTH_TIMEOUT}s）"
ready=0
for ((waited = 0; waited < HEALTH_TIMEOUT; waited += 3)); do
  state="$(docker inspect --format '{{.State.Status}}' "$container_id" 2>/dev/null || true)"
  case "$state" in
    exited*|dead*) die "容器已退出，请执行 docker compose logs --tail=200 deskwong-service" ;;
  esac
  if health_check; then ready=1; break; fi
  sleep 3
done

if [[ "$ready" -ne 1 ]]; then
  "${COMPOSE[@]}" ps
  "${COMPOSE[@]}" logs --tail=200 deskwong-service || true
  die "服务在 ${HEALTH_TIMEOUT}s 内未就绪"
fi

ok "deskwong-service 已完成全量重部署"
"${COMPOSE[@]}" ps

if [[ "$FOLLOW_LOGS" -eq 1 ]]; then
  log "跟随日志（Ctrl+C 退出，不会停止服务）"
  "${COMPOSE[@]}" logs -f --tail=200 deskwong-service
fi
