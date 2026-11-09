// Formal03 3.1 / V4.1: the original immutable base and verified positive
// milestones, kept separate from all Condition/equipment effects. This is a
// source component, not CharacterCapability's selected effective attribute.
#pragma once
#include "growth_attribute_origin_v2.hpp"
#include "growth_permanent_contributions.hpp"
#include <array>
#include <cstddef>
namespace hanmo::resident {
struct GrowthAttributeContributionsReadV2 {
 GrowthAttributeOriginReadV2 origin;
 ActorAttributes permanent_growth;
 // Five original attributes, at most five distinct +1 results each. Borrowed
 // in actual Common append order; no owned sum, effective balance or new row.
 std::array<const AttributeMilestone*,25> original_milestones{};
 std::size_t milestone_count=0;
};
Outcome<GrowthAttributeContributionsReadV2> read_growth_attribute_contributions_v2(
 const GrowthPermanentSources&,const StableId& actor);
// The original boundary owner must first prove its actual payload and M.
// Same-M application is selected by that real Common head's append position.
// Full world capability-chain validation remains with its original owner;
// this reader does not recurse through Condition or assert temporary effects
// absent. It grants neither purpose eligibility nor a request's auxiliary stat.
Outcome<GrowthAttributeContributionsReadV2> read_growth_attribute_contributions_before_source_v2(
 const GrowthPermanentSources&,const StableId& actor,
 const SourceRef& actual_boundary,const WorldMoment& actual_at);
}
