#!/usr/bin/env bash
# 真实100人输入的C边界与原调度尾检查；不重复B轨迹或中央整世界重放。
set -euo pipefail
repo=${1:?repo}; src=${2:?unified source root}; public=${3:?public include root}; json=${4:?locked JSON include}
oracle=${5:?actual100 oracle directory}; witness=${6:-"$repo/native/world/tests/resident/background_actual100_schedule_witness.json"}
build="$repo/.local/resident-actual100"; mkdir -p "$build"; objects=()
for source in "$src"/resident/domains/{military,interaction,conditions,travel}/*.cpp "$src"/resident/{geometry,indexes}/*.cpp "$src"/resident/storage/{common_store,common_validation,common_codec}.cpp "$src"/resident/checkpoint/ordered_value.cpp; do
 key=$(echo "$source" | sed 's|.*/resident/||; s|/|_|g; s/\.cpp$/.o/'); object="$build/$key"
 if [[ ! -e "$object" || "$source" -nt "$object" ]] || [[ -n "$(find "$src" "$public" -name '*.hpp' -newer "$object" -print -quit 2>/dev/null)" ]]; then
  c++ -std=c++17 -O1 -ffp-contract=off -I "$public" -I "$src" -I "$json" -c "$source" -o "$object"
 fi
 objects+=("$object")
done
c++ -std=c++17 -O1 -ffp-contract=off -I "$public" -I "$src" "$repo/native/world/tests/resident/background_actual100.cpp" "${objects[@]}" -o "$build/background_actual100"
"$build/background_actual100" "$oracle" "$witness"
