// Original LongRest -4 fatigue / +1 HP, without injury or death inference.
#pragma once
#include "character_condition_read.hpp"
#include "resident/domains/growth/long_rest_source.hpp"
#include "resident/domains/growth/study_lesson_capability.hpp"
namespace hanmo::resident {
// The original step owns food and scalar mutations. This same-candidate entry
// captures its real writes, then stores one embedded period and exact receipts.
Outcome<GrowthFixedStudyApplication> apply_long_rest_fixed_gate_in_candidate(CommonStore&,GrowthState&,AuthoredActorFoundationState&,const CharacterConditionSources&,const GrowthCatalog&,const CommonCatalog&,const GrowthAccess&,const EstateState&,const ActionTimeRegistration&,const GrowthFixedStepPermit&);
Status validate_long_rest_condition_receipt(const CharacterConditionSources&,const ConditionReceiptV2&,const FrozenBodyScalars& actual_before);
Status validate_long_rest_condition_coverage(const CharacterConditionSources&);
}
