// 显式Military12只接完整11，旧格式不追加空休整序列。
#pragma once
#include "military_garrison_rest_model.hpp"
#include "military_command_shock_profile.hpp"
namespace hanmo::resident {
inline constexpr unsigned military_garrison_rest_checkpoint_feature=67108864u;
inline constexpr const char* military_garrison_rest_module="military_garrison_rest";
inline Status validate_military_garrison_rest_modules(const std::vector<std::pair<StableId,StableId>>& modules){auto valid=validate_military_command_shock_modules(modules);if(!valid.ok)return valid;unsigned selected=0,prior=0;for(const auto& m:modules){if(m.first==military_garrison_rest_module){if(m.second!="1")return common_failure("MilitaryGarrisonRestProfileMismatch");++selected;}if(m.first==military_command_shock_module&&m.second=="1")++prior;}if(selected>1)return common_failure("MilitaryGarrisonRestProfileMismatch");return selected&&prior!=1?common_failure("MilitaryGarrisonRestPrerequisiteRequired"):common_success();}
}
