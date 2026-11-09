// 野外继任独立显式pair；Military7不能以高版本暗启驻营Military6能力。
#pragma once
#include "military_officer_profile.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t military_field_command_checkpoint_feature=32768u;
inline constexpr const char* military_field_command_module="military_field_commands";
inline constexpr const char* military_field_command_political_module="political_field_appointments";
// 没有野外pair仍走原驻营规则；启用则要求两个原驻营模块都已精确声明。
inline Status validate_military_field_command_module_pair(const std::vector<std::pair<StableId,StableId>>& modules){
 auto original=validate_military_officer_module_pair(modules);if(!original.ok)return original;
 unsigned military=0,politics=0,prerequisite=0;
 for(const auto& m:modules){if(m.first==military_field_command_module){if(m.second!="1")return common_failure("MilitaryFieldCommandProfileMismatch");++military;}if(m.first==military_field_command_political_module){if(m.second!="1")return common_failure("MilitaryFieldCommandProfileMismatch");++politics;}if(m.first==military_officer_module&&m.second=="1")++prerequisite;}
 if(military!=politics||military>1)return common_failure("MilitaryFieldCommandProfileMismatch");
 return military&&prerequisite!=1?common_failure("MilitaryFieldCommandPrerequisiteRequired"):common_success();
}
}
