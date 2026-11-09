// 显式11依赖完整10；禁止未知版本、重复标记或借装有新战场暗开旧场景。
#pragma once
#include "military_command_shock_contract.hpp"
#include "military_rout_profile.hpp"
namespace hanmo::resident {
inline Status validate_military_command_shock_modules(const std::vector<std::pair<StableId,StableId>>& modules){auto previous=validate_military_rout_modules(modules);if(!previous.ok)return previous;unsigned selected=0,prior=0;for(const auto& m:modules){if(m.first==military_command_shock_module){if(m.second!="1")return common_failure("MilitaryCommandShockProfileMismatch");++selected;}if(m.first==military_rout_module&&m.second=="1")++prior;}if(selected>1)return common_failure("MilitaryCommandShockProfileMismatch");return selected&&prior!=1?common_failure("MilitaryCommandShockPrerequisiteRequired"):common_success();}
}
