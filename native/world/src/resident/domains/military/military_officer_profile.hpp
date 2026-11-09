// 调任两个主权必须同情景显式准入；空序列不改变旧Military1..5与政治存档。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t military_officer_checkpoint_feature=16384u;
inline constexpr const char* military_officer_module="military_officer_assignments";
inline constexpr const char* military_officer_political_module="political_military_appointments";
// 只比较完整目录身份；单边、重复或未知版本均拒绝，不由任一marker补出另一边。
inline Status validate_military_officer_module_pair(const std::vector<std::pair<StableId,StableId>>& modules){
    unsigned military=0,politics=0;for(const auto& module:modules){if(module.first==military_officer_module){if(module.second!="1")return common_failure("MilitaryOfficerAssignmentProfileMismatch");++military;}if(module.first==military_officer_political_module){if(module.second!="1")return common_failure("MilitaryOfficerAssignmentProfileMismatch");++politics;}}
    return military==politics&&military<=1?common_success():common_failure("MilitaryOfficerAssignmentProfileMismatch");
}
}
