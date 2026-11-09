// 伤员恢复显式Military8/524288；不得靠最高版本暗启任何旧军旅或政治能力。
#pragma once
#include "military_field_command_profile.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t military_recovery_checkpoint_feature=524288u;
inline constexpr const char* military_recovery_module="military_wounded_recovery";
// 不依赖尚未注册的Naval7换船；要求真实已注册Military7野外交接及其Military6配对前置；具名兵员必须有完整个人康复模块。
inline Status validate_military_recovery_modules(const std::vector<std::pair<StableId,StableId>>& modules){
 auto old=validate_military_field_command_module_pair(modules);if(!old.ok)return old;unsigned recovery=0,field=0,personal=0;
 for(const auto& row:modules){if(row.first==military_recovery_module){if(row.second!="1")return common_failure("MilitaryRecoveryProfileMismatch");++recovery;}if(row.first==military_field_command_module&&row.second=="1")++field;if(row.first=="personal_injury_recovery"){if(row.second!="1")return common_failure("MilitaryRecoveryPersonalProfileMismatch");++personal;}}
 if(recovery>1||personal>1)return common_failure("MilitaryRecoveryProfileMismatch");return recovery&&(field!=1||personal!=1)?common_failure("MilitaryRecoveryPrerequisiteRequired"):common_success();
}
}
