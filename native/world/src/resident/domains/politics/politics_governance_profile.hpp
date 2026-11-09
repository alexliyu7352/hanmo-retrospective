// 两个作者领域共同选择唯一治理组合；任务能力由它们派生，没有第三个作者开关。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
// 新局、目录恢复共用严格准入；仅缺省两段时关闭，部分启用与未知版本明确拒绝。
inline bool read_politics_governance_profile(const BoundaryValue& dynamic) {
    using namespace boundary;
    const auto* politics=dynamic.find("politics_governance");
    const auto* relief=dynamic.find("public_relief");
    if(!politics&&!relief)return false;
    if(!politics||!relief)fail("IncompleteGovernanceProfile","dynamic_world");
    if(get_string(dynamic,"profile","dynamic_world")!="local_society_v1")fail("GovernanceNativeProfileRequired","dynamic_world");
    if(!dynamic.find("politics"))fail("GovernanceRequiresPolitics","dynamic_world/politics_governance");
    if(!get_bool(*politics,"enabled","dynamic_world/politics_governance"))fail("GovernanceProfileRequired","dynamic_world/politics_governance/enabled");
    if(get_int(*relief,"version","dynamic_world/public_relief")!=1)fail("PublicReliefVersionMismatch","dynamic_world/public_relief/version");
    return true;
}
}
