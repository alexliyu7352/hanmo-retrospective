// Original ordinary service completion, including its exact net-zero writes.
#pragma once
#include "resident/domains/growth/study_lesson_capability.hpp"
namespace hanmo::resident {
struct CharacterInjurySourcesStateV1;
// 普通rest的source1完成只借同候选实际伤情根；不提供治疗或长休替身。
Outcome<GrowthFixedStudyApplication> apply_injury_rest_fixed_gate_in_candidate(
    CommonStore &, GrowthState &, AuthoredActorFoundationState &,
    CharacterInjurySourcesStateV1 &, const CharacterConditionSources &,
    const GrowthCatalog &, const CommonCatalog &, SocialState &,
    InteractionState &, const GrowthAccess &, const ActionTimeRegistration &,
    const GrowthFixedStepPermit &);
// 核原服务注册、原目录和已完成来源的双向覆盖，不以当前空应用表代替接线。
Status validate_original_injury_rest_service_sources(
    const CharacterConditionSources &);
Outcome<GrowthFixedStudyApplication> apply_service_fixed_gate_in_candidate(CommonStore&,GrowthState&,AuthoredActorFoundationState&,const CharacterConditionSources&,const GrowthCatalog&,const CommonCatalog&,SocialState&,InteractionState&,const GrowthAccess&,const ActionTimeRegistration&,const GrowthFixedStepPermit&);
Status validate_original_work_service_condition(const CharacterConditionSources&,const ConditionReceiptV2&);
Status validate_original_work_service_condition_coverage(const CharacterConditionSources&);
}
