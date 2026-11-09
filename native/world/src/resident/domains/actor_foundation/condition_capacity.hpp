// Actual registered-purpose boolean transitions from original Condition writes.
#pragma once
#include "condition_capacity_contract.hpp"
#include "character_capability.hpp"
#include "condition_source_retention.hpp"
namespace hanmo::resident {
SourceRef character_capacity_transition_source(const StableId&);
// No Common mutation/notification, no current-capacity cache, no new clock.
// Fresh output requires this original still-current actual capture token.
// Only selected registered boolean gates that really changed produce a row.
Status record_actual_condition_capacity_changes_in_candidate(CommonStore&,
 AuthoredActorFoundationState&,const CharacterConditionSources&,
 const BoundActorConditionWrite&);
Status validate_condition_purpose_capacity_coverage(const CharacterConditionSources&);
Outcome<const CapacityTransitionV2*> read_verified_condition_purpose_capacity_transition(
 const CharacterConditionSources&,const SourceRef&,Integer source_schema);
// Original owner-retention graph only; central additionally checks all external PINs.
Outcome<bool> condition_purpose_capacity_source_owner_may_release(
 const CharacterConditionSources&,const SourceRef&);
Status visit_condition_purpose_capacity_source_pins(const CharacterConditionSources&,
 CharacterConditionPinSink&);
Status validate_condition_purpose_capacity_source_pin(const CharacterConditionSources&,
 const SourcePin&);
}
