// 师承提交尾只绑定本轮前后差分新增事件；没有缓存/请求原件不能追认旧来源。
#pragma once
#include "family_apprenticeship.hpp"
namespace hanmo::resident {
struct FamilyEffect;
// 原事实历史和此永久结果门必须都通过才可发布/冷恢复；候选写中允许新事件暂未绑定。
Status validate_apprenticeship_result_bindings(const CommonStore&,const FamilyApprenticeshipState&);
// CommitResult 必须是本 Common 实际缓存的同一个对象，不能传自行构造的同值壳。
Status bind_apprenticeship_result_in_candidate(CommonStore&,const FamilyApprenticeshipState& before,FamilyApprenticeshipState& after,const StableId& actor,Integer request_sequence,const CommitResult& actual_result);
Outcome<StableId> read_apprenticeship_source_for_result(const CommonStore&,const FamilyApprenticeshipState&,const CommitResult& actual_result);
Status validate_apprenticeship_effect(const FamilyApprenticeshipState&,const StableId& source_fact_id,const StableId& kind,const StableId& actor,Integer tick,const FamilyEffect&);
}
