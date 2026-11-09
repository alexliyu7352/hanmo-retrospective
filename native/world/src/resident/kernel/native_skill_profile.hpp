// 显式十六技艺只属于新情景能力；没有字段的旧情景不增经验块或模块身份。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
// 冷边界严格拒绝未知政策与非原生情景，不能由六项值存在就推测升级。
inline bool native_extended_skills(const BoundaryValue& scenario){
    const auto* profile=scenario.find("skill_profile");if(!profile)return false;
    if(boundary::string(*profile,"scenario/skill_profile")!="full_sixteen_v1")boundary::fail("UnsupportedSkillProfile","scenario/skill_profile");
    if(!scenario.find("dynamic_world"))boundary::fail("ExtendedSkillsNativeProfileRequired","scenario/skill_profile");
    return true;
}
}
