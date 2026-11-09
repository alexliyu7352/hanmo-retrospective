// Military10与原9完整前置成对；仅新选场景启用，不以目录最高版本暗开旧档。
#pragma once
#include "military_rout_contract.hpp"
#include "military_supply_attrition_profile.hpp"
namespace hanmo::resident {
inline Status validate_military_rout_modules(const std::vector<std::pair<StableId,StableId>>& modules){
 auto old=validate_military_supply_attrition_modules(modules);if(!old.ok)return old;unsigned selected=0,prior=0;
 for(const auto& row:modules){if(row.first==military_rout_module){if(row.second!="1")return common_failure("MilitaryRoutProfileMismatch");++selected;}if(row.first==military_supply_attrition_module&&row.second=="1")++prior;}
 if(selected>1)return common_failure("MilitaryRoutProfileMismatch");return selected&&prior!=1?common_failure("MilitaryRoutPrerequisiteRequired"):common_success();
}
}
