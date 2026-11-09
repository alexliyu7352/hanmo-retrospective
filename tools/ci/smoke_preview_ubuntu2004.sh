#!/usr/bin/env bash
# Run the actual exported executable in the target userspace, with a fresh private test home.
set -euo pipefail
source /etc/os-release
[[ "$ID" == ubuntu && "$VERSION_ID" == 20.04 ]]
src=$1
output=$2
engine=$3
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends python3 binutils libx11-6 libxcursor1 libxinerama1 libxrandr2 libxi6 libgl1 libasound2 libfontconfig1
folder=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["folder"])' "$output/preview-path.json")
export XDG_DATA_HOME="$output/smoke-state/data" XDG_CACHE_HOME="$output/smoke-state/cache" XDG_CONFIG_HOME="$output/smoke-state/config"
mkdir -p "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_CONFIG_HOME"
python3 "$src/tools/ci/check_elf_compatibility.py" "$folder" > "$output/package-glibc-abi.json"
while IFS= read -r -d '' elf; do
  ldd -r "$elf" >> "$output/package-target-linker.log"
done < <(find "$folder" -type f \( -name '*.so' -o -name 'hanmo.x86_64' \) -print0)
if grep -E 'not found|undefined symbol|version .* not found' "$output/package-target-linker.log"; then exit 1; fi
# This fixed export template intentionally forbids --path and ignores external --script.
# Exercise its real title process, then inspect the exact PCK with the matching editor.
(cd "$folder" && timeout 30 ./hanmo.x86_64 --headless --audio-driver Dummy --quit-after 120) \
  > "$output/preview-title.log" 2>&1
if grep -E 'SCRIPT ERROR:|(^|[[:space:]])ERROR:' "$output/preview-title.log"; then exit 1; fi
python3 - "$folder/BUILD.json" "$engine" <<'PYENGINE'
import hashlib, json, pathlib, sys
expected = json.load(open(sys.argv[1]))['engine']['sha256']
assert hashlib.sha256(pathlib.Path(sys.argv[2]).read_bytes()).hexdigest() == expected
PYENGINE
runtime="$output/editor-pck-runtime"
mkdir -p "$runtime/native_resident_bridge"
cp "$folder/libhanmo_world_bridge.so" "$runtime/native_resident_bridge/libhanmo_world_bridge.so"
cmp "$folder/libhanmo_world_bridge.so" "$runtime/native_resident_bridge/libhanmo_world_bridge.so"
printf 'config_version=5\n' > "$runtime/project.godot"
timeout 90 "$engine" --headless --audio-driver Dummy --path "$runtime" --main-pack "$folder/hanmo.x86_64" \
  --script "$src/tools/ci/preview_smoke.gd" -- "$output/smoke-input.json" "$output/smoke-result.json" \
  > "$output/preview-start.log" 2>&1
if grep -E 'SCRIPT ERROR:|(^|[[:space:]])ERROR:' "$output/preview-start.log"; then exit 1; fi
python3 - "$output/smoke-result.json" <<'PY'
import hashlib, json, pathlib, sys
result = json.load(open(sys.argv[1]))
assert result['ok'] is True and result['scope'] == 'FIXED_EDITOR_PACKED_MAIN_START_HEADLESS', result
pathlib.Path(sys.argv[1]).with_name('target-smoke.json').write_text(json.dumps({'target':'Ubuntu 20.04','template_title_exit':0,'fixed_editor_packed_world_exit':0,'result_sha256':hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest()},indent=2)+'\n')
print('Exported title and exact packaged world passed separately in Ubuntu 20.04; GUI/full game NOT_RUN.')
PY
