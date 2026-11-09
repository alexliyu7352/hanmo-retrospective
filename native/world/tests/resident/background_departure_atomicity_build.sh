#!/usr/bin/env bash
# 只编译本测试并链接已冻结库，不配置或重建生产库；GNU链接包装仅在此测试可执行文件内生效。
set -euo pipefail
repo=${1:?test repository}; central=${2:?matching central source}; libraries=${3:?frozen library directory}; oracle=${4:?actual100 oracle directory}; evidence=${5:?new evidence directory}
[[ ! -e "$evidence" ]] || { echo 'Evidence directory already exists' >&2; exit 1; }
mkdir -p "$evidence"
cp "$libraries"/libhanmo_{resident,world,flecs}.a "$evidence/"
cp "$libraries/frozen-identity.json" "$evidence/library-identity.json"
cp "$libraries/generated/build_identity.hpp" "$evidence/"
cp "$repo/native/world/tests/resident/background_departure_atomicity.cpp" "$evidence/test.cpp"
python3 - "$evidence" <<'PY'
import hashlib,json,pathlib,sys
p=pathlib.Path(sys.argv[1]);identity=json.loads((p/'library-identity.json').read_text())
for name in ['libhanmo_resident.a','libhanmo_world.a','libhanmo_flecs.a']:
    assert hashlib.sha256((p/name).read_bytes()).hexdigest()==identity['artifacts'][name]['sha256'],name
PY
c++ -std=c++17 -O1 -ffp-contract=off -I "$central/native/world/include" -I "$central/native/world/src" -c "$evidence/test.cpp" -o "$evidence/test.o"
symbol=_ZN5hanmo8resident20begin_terrain_travelERNS0_11CommonWriteERNS0_11TravelStateENSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERKNS0_11TravelQuoteE
c++ "$evidence/test.o" -Wl,--wrap="$symbol" "$evidence/libhanmo_resident.a" "$evidence/libhanmo_world.a" "$evidence/libhanmo_flecs.a" -pthread -o "$evidence/background_departure_atomicity"
"$evidence/background_departure_atomicity" "$oracle" | tee "$evidence/run.log"
