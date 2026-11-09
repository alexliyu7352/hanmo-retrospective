// V3.2: base attributes from the same true Origin2 as skills/age. This is not
// the selected effective-attribute reducer and grants no social/body eligibility.
#pragma once
#include "resident/domains/actor_foundation/created_origin.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct GrowthAttributeOriginReadV2 {
 const CharacterOriginRecordV2* origin=nullptr;
 const ActorAttributes* base_attributes=nullptr;
};
Outcome<GrowthAttributeOriginReadV2> read_growth_attribute_origin_v2(
 const CharacterConditionSources&,const StableId& actor);
// Original boundary owner must prove this source's actual M first. Same-M
// presence is the original Common append cut; no skill or birthday surrogate.
Outcome<GrowthAttributeOriginReadV2> read_growth_attribute_origin_before_source_v2(
 const CharacterConditionSources&,const StableId& actor,
 const SourceRef& actual_boundary,const WorldMoment& actual_at);
}
