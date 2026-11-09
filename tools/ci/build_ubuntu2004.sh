#!/usr/bin/env bash
# 仅由 Ubuntu 20.04 容器调用；开发编译不触发完整玩法/存读验收。
set -euo pipefail
src=${HANMO_SOURCE_ROOT:-/src}
cache=${HANMO_CACHE_ROOT:-/cache}
out=${HANMO_OUTPUT_ROOT:-/out}
cd "$src"
source /etc/os-release
[[ "$ID" == ubuntu && "$VERSION_ID" == 20.04 ]]
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends ca-certificates git curl python3 build-essential ccache binutils unzip libx11-6 libxcursor1 libxinerama1 libxrandr2 libxi6 libgl1 libasound2
git config --global --add safe.directory "$src"
export XDG_CACHE_HOME="$cache/runtime/cache" XDG_DATA_HOME="$cache/runtime/data" XDG_CONFIG_HOME="$cache/runtime/config"
export CCACHE_DIR="$cache/ccache" CCACHE_BASEDIR="$src"
mkdir -p "$XDG_CACHE_HOME" "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$out"
python3 -m unittest discover -s tools/ci -p 'test_*.py'
python3 tools/ci/bootstrap_locked_native.py --root "$src" --cache "$cache"
python3 tools/ci/record_native_build.py --root "$src" --cache "$cache" --output "$out" --expected-source "$SOURCE_SHA"
mapfile -t libraries < <(find "$cache/build" -type f -name libhanmo_world_bridge.so)
[[ ${#libraries[@]} == 1 ]]
test -f "$out/hanmo_resident_journey" && test -f "$out/libhanmo_world_bridge.so"
python3 tools/ci/check_elf_compatibility.py "$out" > "$out/glibc-abi.json"
# 必须在目标容器解析真实运行依赖；只有 ABI 检查还不能算兼容运行通过。
for executable in "$out/hanmo_resident_journey" "$out/libhanmo_world_bridge.so"; do
  ldd -r "$executable" | tee -a "$out/target-linker.log"
 done
if grep -E 'not found|undefined symbol|version .* not found' "$out/target-linker.log"; then exit 1; fi
HANMO_BRIDGE="$out/libhanmo_world_bridge.so" python3 - <<'PY'
import ctypes, os
ctypes.CDLL(os.environ['HANMO_BRIDGE'], mode=ctypes.RTLD_LOCAL)
print('Native bridge loads in Ubuntu 20.04; game acceptance NOT_RUN.')
PY
printf '%s\n' "$SOURCE_SHA" > "$out/source-commit.txt"
python3 tools/ci/verify_native_outputs.py --directory "$out" --expected-source "$SOURCE_SHA" > "$out/native-output-verification.json"
(cd "$out" && sha256sum hanmo_resident_journey libhanmo_world_bridge.so libhanmo_resident.a libhanmo_world.a libhanmo_flecs.a resident_bridge_identity.json dependencies.lock.json locked.cmake native-build-receipt.json) > "$out/SHA256SUMS"
ccache --show-stats
