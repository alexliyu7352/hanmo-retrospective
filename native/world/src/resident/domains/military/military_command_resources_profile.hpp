// Military15最后一位仅本域声明；共享启用须完整14和顺序策略，旧格式不追加存在位。
#pragma once
#include "military_sequential_profile.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t military_command_resources_checkpoint_feature=2147483648u;
static_assert(military_command_resources_checkpoint_feature==0x80000000u,"Military15 requires the exact unsigned final checkpoint bit");
inline constexpr const char* military_command_resources_module="military_command_resources";
inline Status validate_military_command_resources_modules(const std::vector<std::pair<StableId,StableId>>& modules){
 auto checked=validate_military_sequential_modules(modules);if(!checked.ok)return checked;unsigned selected=0,prior=0;
 for(const auto& m:modules){if(m.first==military_command_resources_module){if(m.second!="1")return common_failure("MilitaryCommandResourcesProfileMismatch");++selected;}if(m.first==military_sequential_module&&m.second=="1")++prior;}
 if(selected>1)return common_failure("MilitaryCommandResourcesProfileMismatch");return selected&&prior!=1?common_failure("MilitaryCommandResourcesPrerequisiteRequired"):common_success();
}
// 只映射本15到真实14前置，未知16不能借高位冒已知布局。
inline Status validate_military_command_resources_checkpoint_layout(std::uint32_t version,const std::vector<std::pair<StableId,StableId>>& modules){
 auto checked=validate_military_command_resources_modules(modules);if(!checked.ok)return checked;const auto present=std::find(modules.begin(),modules.end(),std::make_pair(StableId{military_command_resources_module},StableId{"1"}))!=modules.end();
 if(version<1||version>15||(version==15)!=present)return common_failure("MilitaryCommandResourcesCodecVersionMismatch");return validate_military_commanded_checkpoint_layout(version==15?14u:version,modules);
}
}
