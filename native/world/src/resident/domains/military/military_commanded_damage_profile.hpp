// 显式Military14只在完整13之上启用；旧1..13不写新存在位。
#pragma once
#include "military_commanded_damage_model.hpp"
#include "military_combat_conditions_profile.hpp"
namespace hanmo::resident {
inline constexpr unsigned military_commanded_damage_checkpoint_feature=1073741824u;
inline constexpr const char* military_commanded_damage_module="military_commanded_damage";
inline Status validate_military_commanded_damage_modules(const std::vector<std::pair<StableId,StableId>>& modules){
 auto checked=validate_military_combat_conditions_modules(modules);if(!checked.ok)return checked;
 unsigned selected=0,prior=0;
 for(const auto& m:modules){if(m.first==military_commanded_damage_module){if(m.second!="1")return common_failure("MilitaryCommandedDamageProfileMismatch");++selected;}if(m.first==military_combat_conditions_module&&m.second=="1")++prior;}
 if(selected>1)return common_failure("MilitaryCommandedDamageProfileMismatch");
 return selected&&prior!=1?common_failure("MilitaryCommandedDamagePrerequisiteRequired"):common_success();
}
// 标题冷入口与小门共用唯一真实版本判定；14追加层仍精确核所有旧marker，未来值不能冒旧格式。
inline Status validate_military_commanded_checkpoint_layout(std::uint32_t version,const std::vector<std::pair<StableId,StableId>>& modules){
 auto checked=validate_military_commanded_damage_modules(modules);if(!checked.ok)return checked;auto has=[&](const char* name){return std::find(modules.begin(),modules.end(),std::make_pair(StableId{name},StableId{"1"}))!=modules.end();};
 if(version<1||version>14||(version==14)!=has(military_commanded_damage_module))return common_failure("MilitaryCommandedDamageCodecVersionMismatch");
 const auto prior=version==14?13u:version;const bool fields=has(military_field_command_module),officers=has(military_officer_module),requests=has("military_requests");
 if((prior==13)!=has(military_combat_conditions_module)||(prior==12||prior==13)!=has(military_garrison_rest_module)||(prior==11||prior==12||prior==13)!=has(military_command_shock_module)||(prior==10||prior==11||prior==12||prior==13)!=has(military_rout_module)||(prior==9||prior==10||prior==11||prior==12||prior==13)!=has(military_supply_attrition_module)||(prior==8||prior==9||prior==10||prior==11||prior==12||prior==13)!=has(military_recovery_module)||(prior==7||prior==8||prior==9||prior==10||prior==11||prior==12||prior==13)!=fields||(!fields&&(prior==6)!=officers)||(!officers&&(prior==5)!=requests))return common_failure("MilitaryOfficerAssignmentProfileMismatch");
 return common_success();
}
}
