// 地形策略沿原M14以上完整顺序轮重放，marker不改变任何旧Military保存段。
#pragma once
#include "military_sequential_profile.hpp"
#include "military_field_terrain_contract.hpp"
namespace hanmo::resident {
inline Status validate_military_field_terrain_modules(const std::vector<std::pair<StableId,StableId>>& modules){auto checked=validate_military_sequential_modules(modules);if(!checked.ok)return checked;unsigned selected=0,prior=0;for(const auto& m:modules){if(m.first==military_field_terrain_module){if(m.second!="1")return common_failure("MilitaryFieldTerrainProfileMismatch");++selected;}if(m.first==military_sequential_module&&m.second=="1")++prior;}if(selected>1)return common_failure("MilitaryFieldTerrainProfileMismatch");return selected&&prior!=1?common_failure("MilitaryFieldTerrainPrerequisiteRequired"):common_success();}
}
