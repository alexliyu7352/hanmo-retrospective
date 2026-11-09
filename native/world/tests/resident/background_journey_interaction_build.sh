#!/usr/bin/env bash
# 仅覆盖编译C本次两份生产源和窄测试；依赖已完成的P2基础库，不重建中央/Bridge或资源。
set -euo pipefail
repo=${1:?C repository}; central=${2:?matching central source}; libraries=${3:?finished P2 baseline library directory}; workspace=${4:?oracle workspace}; build=${5:?new evidence directory}
[[ ! -e "$build" ]] || { echo 'Evidence directory already exists' >&2; exit 1; }
mkdir -p "$build/src/resident/domains" "$build/src/resident/views" "$build/lib"
cp -r "$repo/native/world/src/resident/domains/"{military,interaction,conditions} "$build/src/resident/domains/"
cp "$repo/native/world/src/resident/views/journey_interaction."{hpp,cpp} "$build/src/resident/views/"
cp "$libraries/"libhanmo_{resident,world,flecs}.a "$build/lib/"
cp "$libraries/generated/build_identity.hpp" "$build/lib/"
flags=(-std=c++17 -O1 -ffp-contract=off -I "$central/native/world/include" -I "$build/src" -I "$central/native/world/src")
c++ "${flags[@]}" -c "$build/src/resident/domains/military/military_rules.cpp" -o "$build/military_rules.o"
c++ "${flags[@]}" -c "$build/src/resident/views/journey_interaction.cpp" -o "$build/journey_interaction.o"
c++ "${flags[@]}" "$repo/native/world/tests/resident/background_journey_interaction.cpp" "$build/journey_interaction.o" "$build/military_rules.o" "$build/lib/libhanmo_resident.a" "$build/lib/libhanmo_world.a" "$build/lib/libhanmo_flecs.a" -pthread -o "$build/background_journey_interaction"
"$build/background_journey_interaction" "$repo/native/world/tests/resident/background_journey_interaction_oracle.json" "$workspace" | tee "$build/native.log"
