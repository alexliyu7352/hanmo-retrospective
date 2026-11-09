#!/usr/bin/env bash
# 在原Godot工程启动独立城市试玩；不改Main，不安装引擎或创建另一套工程。
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -z "${GODOT_BIN:-}" && -f "$ROOT/env.sh" ]]; then source "$ROOT/env.sh"; fi
GODOT_BIN="${GODOT_BIN:-$ROOT/bin/godot}"
if [[ ! -x "$GODOT_BIN" ]]; then
  printf '找不到固定引擎。请先 source 项目 env.sh，或 export GODOT_BIN=/path/to/Godot_4.7.2\n' >&2
  exit 2
fi
VERSION="$("$GODOT_BIN" --version)"
if [[ "$VERSION" != 4.7.2.stable.official.ed1daf0bf* ]]; then
  printf '引擎版本不匹配：%s。需要4.7.2.stable.official.ed1daf0bf。\n' "$VERSION" >&2
  exit 2
fi
# 新拉取的透明PNG先走原工程资源导入，避免直接启动时缺少.ctex缓存。
"$GODOT_BIN" --headless --path "$ROOT/game" --editor --import --quit
exec "$GODOT_BIN" --path "$ROOT/game" res://demos/city_layered_showcase.tscn "$@"
