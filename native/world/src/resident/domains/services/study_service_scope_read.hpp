// ST3 原 Study 终止的静态地址适配。所有载荷仍由 Growth 原表拥有。
#pragma once
#include "service_actual_termination_contract.hpp"
#include "service_actual_termination_result.hpp"
#include "resident/domains/growth/study_service_termination.hpp"
namespace hanmo::resident {
struct StudyServiceScopeReadContext {
 const CommonStore& common;const GrowthState& growth;const GrowthCatalog& catalog;
 const ActorFoundationCatalog& foundation;const GrowthAccess& access;
 const VerifiedDeathRead& deaths;const CurrentMomentRead& clock;
};
// 只返回已全验的原槽；同 action 接受两支/条款必须精确相同，正常完成没有此槽。
// 不要求已释放 Action 再出现，不生成 termination ID、课费、退款或第二份结果。
Outcome<const StudyServiceTerminalV2*> read_original_study_service_termination(
 const StudyServiceScopeReadContext&,const GrowthStudyService&,const StudyTerminal&,const ReadStamp&);
// 普通逐课实付的实际 ST3 投影。先原接受/终局/Death/原逐课收费全验，
// 没有原预付或原欠款时这两数组真实为空；预约释放不充退款来源。
// Returns the explicit no-advance result /4; nested service identities stay /3.
Outcome<OwnedServiceActualTerminationResult> read_study_service_actual_termination(
 const StudyServiceScopeReadContext&,const CharacterConditionSources&,
 const GrowthStudyService&,const StudyTerminal&,const ReadStamp&);
}
