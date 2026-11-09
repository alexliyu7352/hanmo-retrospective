// 地方生活接缝只核跨域引用和回收根；资产、计划、知识仍各有唯一领域所有者。
#pragma once
#include "world_bundle.hpp"
namespace hanmo::resident {
std::vector<StableId> collect_regional_life_pins(const WorldBundle&);
Status prune_regional_life(WorldBundle&);
Status validate_regional_world(const WorldBundle&);
}
