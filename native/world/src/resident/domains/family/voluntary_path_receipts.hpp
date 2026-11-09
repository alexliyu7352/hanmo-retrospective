#pragma once
#include "voluntary_path_state.hpp"
namespace hanmo::resident {
Status validate_voluntary_path_result_bindings(const CommonStore&,const FamilyState&,const VoluntaryPathState&);
// 只能绑定真正before/after差分；before_identity来自Resident原当前World，不从result倒算。
// 结果必须为同Common真实缓存对象且原history最后条。失败由外层整候选回滚。
Status bind_voluntary_path_result_in_candidate(CommonStore&,const WorldIdentity& before_identity,
    const FamilyState& before_family,const VoluntaryPathState& before,FamilyState& after_family,
    VoluntaryPathState& after,const StableId& actor,Integer request_seq,const CommitResult& actual_result);
Outcome<StableId> read_voluntary_path_source_for_result(const CommonStore&,const FamilyState&,
    const VoluntaryPathState&,const CommitResult& actual_result);
}
