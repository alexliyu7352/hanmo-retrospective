// Military9显式尾段依赖完整Military8；旧1..8不会因目录中存在策略而暗启。
#pragma once
#include "military_recovery_profile.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t military_supply_attrition_checkpoint_feature=2097152u;
inline constexpr const char* military_supply_attrition_module="military_supply_attrition";
inline Status validate_military_supply_attrition_modules(const std::vector<std::pair<StableId,StableId>>& modules){
 auto old=validate_military_recovery_modules(modules);if(!old.ok)return old;unsigned selected=0,recovery=0;
 for(const auto& row:modules){if(row.first==military_supply_attrition_module){if(row.second!="1")return common_failure("MilitarySupplyAttritionProfileMismatch");++selected;}if(row.first==military_recovery_module&&row.second=="1")++recovery;}
 if(selected>1)return common_failure("MilitarySupplyAttritionProfileMismatch");return selected&&recovery!=1?common_failure("MilitarySupplyAttritionPrerequisiteRequired"):common_success();
}
}
