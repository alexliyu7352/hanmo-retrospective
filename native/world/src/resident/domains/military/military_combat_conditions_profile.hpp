// 显式Military13要求完整12，旧1..12不写战斗条件存在位。
#pragma once
#include "military_combat_conditions_model.hpp"
#include "military_garrison_rest_profile.hpp"
namespace hanmo::resident {
inline constexpr unsigned military_combat_conditions_checkpoint_feature=134217728u;
inline constexpr const char* military_combat_conditions_module="military_combat_conditions";
inline Status validate_military_combat_conditions_modules(const std::vector<std::pair<StableId,StableId>>& modules){auto checked=validate_military_garrison_rest_modules(modules);if(!checked.ok)return checked;unsigned selected=0,prior=0;for(const auto& m:modules){if(m.first==military_combat_conditions_module){if(m.second!="1")return common_failure("MilitaryCombatConditionProfileMismatch");++selected;}if(m.first==military_garrison_rest_module&&m.second=="1")++prior;}if(selected>1)return common_failure("MilitaryCombatConditionProfileMismatch");return selected&&prior!=1?common_failure("MilitaryCombatConditionPrerequisiteRequired"):common_success();}
}
