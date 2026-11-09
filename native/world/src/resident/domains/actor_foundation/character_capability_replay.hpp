// Source-local validation of an original owner's frozen pre-effect input.
// This is not a current permission query and does not replace complete cold
// Condition/input-change reverse coverage at the installed candidate boundary.
#pragma once
#include "character_capability.hpp"
namespace hanmo::resident {
Status validate_frozen_character_capability_original_prefix(
 const CharacterConditionSources&,const FrozenFacilityCapabilityInputs&,
 const std::optional<StableId>& required_skill=std::nullopt);
// The whole supplied positive segment must use this exact adopted body head.
// A real intermediate Condition change requires another owner boundary; changes
// at the exclusive end may legitimately be that segment's own consequence.
Status validate_frozen_character_capability_original_span(
 const CharacterConditionSources&,const FrozenFacilityCapabilityInputs&,
 const MomentSpan&,const std::optional<StableId>& required_skill=std::nullopt);
}
