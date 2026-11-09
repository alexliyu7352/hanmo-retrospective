#!/usr/bin/env bash
# 窄C域构建只编实际规则及共同依赖；源根可为中央整合树或经核SHA的轻量测试覆盖树。
set -euo pipefail
repo=${1:?repository root}; source_root=${2:?native/world/src root}; json_include=${3:?locked JSON include}; oracle=${4:?1000 oracle folder}
public_include=${HANMO_RESIDENT_PUBLIC_INCLUDE:-"$repo/native/world/include"}
build="$repo/.local/resident-background"; mkdir -p "$build"
extra=()
if [[ -f "$source_root/resident/domains/military/military_contact.cpp" ]]; then extra=("$source_root"/resident/domains/travel/*.cpp "$source_root"/resident/geometry/*.cpp "$source_root"/resident/indexes/*.cpp); fi
objects=()
for source in "$source_root"/resident/domains/{military,interaction,conditions}/*.cpp "$source_root"/resident/storage/{common_store,common_validation,common_codec}.cpp "$source_root"/resident/checkpoint/ordered_value.cpp "${extra[@]}"; do
  key=$(echo "$source" | sed 's|.*/resident/||; s|/|_|g; s/\.cpp$/.o/'); object="$build/$key"
  if [[ ! -e "$object" || "$source" -nt "$object" ]] || [[ -n "$(find "$source_root" "$public_include" -name '*.hpp' -newer "$object" -print -quit 2>/dev/null)" ]]; then
    c++ -std=c++17 -O1 -ffp-contract=off -I "$public_include" -I "$source_root" -I "$json_include" -c "$source" -o "$object"
  fi
  objects+=("$object")
done
for test in background_domains background_runtime; do
  c++ -std=c++17 -O1 -ffp-contract=off -I "$public_include" -I "$source_root" "$repo/native/world/tests/resident/$test.cpp" "${objects[@]}" -o "$build/$test"
  "$build/$test" "$oracle"
done
