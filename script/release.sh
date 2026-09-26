#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

[[ -z "$(git status --porcelain)" ]] || { echo "工作区不干净，请先提交或暂存改动。" >&2; exit 1; }
git fetch --tags origin
version="${1:-}"
if [[ -z "$version" ]]; then
  latest=$(git tag --list 'v[0-9]*.[0-9]*.[0-9]*' --sort=-version:refname | head -n1)
  if [[ -z "$latest" ]]; then version=v0.0.1; else
    IFS=. read -r major minor patch <<< "${latest#v}"
    version="v${major}.${minor}.$((patch + 1))"
  fi
fi
[[ "$version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "版本必须为 vMAJOR.MINOR.PATCH" >&2; exit 2; }
git rev-parse -q --verify "refs/tags/$version" >/dev/null && { echo "标签 $version 已存在" >&2; exit 1; }

git tag -a "$version" -m "Release $version"
git push origin "$version"
echo "已推送 $version，GitHub Actions 将测试、推送 GHCR 镜像并创建 GitHub Release。"
echo "镜像地址：ghcr.io/redgreat/deskwong:${version#v} 与 :latest"
