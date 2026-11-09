#!/usr/bin/env bash
# 下一闭包单域构建；所有源/头/原始状态路径显式给定，不写中央代码或读取未选中的环境。
set -euo pipefail
repo=${1:?repo}; src=${2:?unified source root}; public=${3:?public include root}; json=${4:?locked JSON include}
catalog=${5:?catalog}; tick4=${6:?tick4}; tick5=${7:?tick5}; tick6=${8:?tick6}; tick7=${9:?tick7}
build="$repo/.local/resident-departure"; mkdir -p "$build"; objects=()
for source in "$src"/resident/domains/{military,interaction,conditions,travel}/*.cpp "$src"/resident/{geometry,indexes}/*.cpp "$src"/resident/storage/{common_store,common_validation,common_codec}.cpp "$src"/resident/checkpoint/ordered_value.cpp; do
 key=$(echo "$source" | sed 's|.*/resident/||; s|/|_|g; s/\.cpp$/.o/'); object="$build/$key"
 if [[ ! -e "$object" || "$source" -nt "$object" ]] || [[ -n "$(find "$src" "$public" -name '*.hpp' -newer "$object" -print -quit 2>/dev/null)" ]]; then
  c++ -std=c++17 -O1 -ffp-contract=off -I "$public" -I "$src" -I "$json" -c "$source" -o "$object"
 fi
 objects+=("$object")
done
c++ -std=c++17 -O1 -ffp-contract=off -I "$public" -I "$src" "$repo/native/world/tests/resident/background_departure.cpp" "${objects[@]}" -o "$build/background_departure"
"$build/background_departure" "$catalog" "$tick4" "$tick5" "$tick6" "$tick7"
