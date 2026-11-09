#pragma once
#include "condition_capture.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct ConditionReceiptV2;
struct GrowthPermanentState;
class GrowthCapabilityRevisionWrite;
Status validate_gather_condition_source_v1(const CharacterConditionSources &,
                                           const ConditionReceiptV2 &);
Status validate_gather_condition_coverage_v1(const CharacterConditionSources &);
Outcome<BoundActorConditionWrite> apply_gather_period_condition_in_candidate_v1(
    CommonStore &, AuthoredActorFoundationState &,
    const CharacterConditionSources &, const StableId &actual_gather_receipt,
    const GrowthPermanentState &, GrowthCapabilityRevisionWrite &);
} // namespace hanmo::resident
