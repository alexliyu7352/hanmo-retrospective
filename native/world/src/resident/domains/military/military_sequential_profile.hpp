// 原Military14布局保持：新增marker只选择能从原entry/round逐步重演的算法，不再占32位。
#pragma once
#include "military_commanded_damage_profile.hpp"
namespace hanmo::resident {
inline constexpr const char* military_sequential_module="military_sequential_rounds";
inline Status validate_military_sequential_modules(const std::vector<std::pair<StableId,StableId>>& modules){
 auto checked=validate_military_commanded_damage_modules(modules);if(!checked.ok)return checked;unsigned selected=0,prior=0;
 for(const auto& m:modules){if(m.first==military_sequential_module){if(m.second!="1")return common_failure("MilitarySequentialProfileMismatch");++selected;}if(m.first==military_commanded_damage_module&&m.second=="1")++prior;}
 if(selected>1)return common_failure("MilitarySequentialProfileMismatch");return selected&&prior!=1?common_failure("MilitarySequentialPrerequisiteRequired"):common_success();
}
}
