#pragma once
#include "growth_fixed_start.hpp"
#include "study_pending_release.hpp"
#include "resident/domains/actor_foundation/military_availability.hpp"
#include <functional>
namespace hanmo::resident {
// 不可公开构造的候选证明，只让原消费者识别真正等待封存的新来源。
class GrowthPendingStartedProof;
struct GrowthConditionInputEffects {
 StudyPendingReleaseProofs pending_study_releases;
 std::vector<StableId> adopted_boundary_ids,suspended_actors;
 std::vector<std::pair<StableId,GrowthEffect>> completed_actions;
};
// The complete outer candidate owns capture and release. All supplied bindings
// must belong to that live capture. Original result handlers run before their
// unique FSL terminal, and this consumer never mutates body scalars.
Outcome<GrowthConditionInputEffects> consume_growth_condition_inputs_in_candidate(
 CommonStore&,GrowthState&,const CharacterConditionSources&,ActionElapsedStore&,
 const GrowthFixedCaptureServices&,const std::vector<BoundActorConditionWrite>&,
 const std::vector<BoundActorAvailabilityWrite>&,
 const std::function<Status(const StableId&)>& original_release,const GrowthPendingStartedProof* pending=nullptr);
// Also called at real movement/occupancy command tails, including an actual
// same-place movement opening which does not need an Actor.location mutation.
// Sources are enumerated by the original S1 history reader, never synthesized
// from the current location. It shares the same original result/release writer.
Outcome<GrowthConditionInputEffects> consume_growth_spatial_inputs_in_candidate(
 CommonStore&,GrowthState&,const CharacterConditionSources&,ActionElapsedStore&,
 const GrowthFixedCaptureServices&,
 const std::function<Status(const StableId&)>& original_release,const GrowthPendingStartedProof* pending=nullptr);
// Runs immediately after original Interaction/Party/Army occupation writers and
// again before stable publication. Original source order preserves same-M net-zero
// changes; no event is inferred from today's final quiet boolean.
Outcome<GrowthConditionInputEffects> consume_growth_quiet_inputs_in_candidate(
 CommonStore&,GrowthState&,const CharacterConditionSources&,ActionElapsedStore&,
 const GrowthFixedCaptureServices&,const std::function<Status(const StableId&)>& original_release,const GrowthPendingStartedProof* pending=nullptr);

}
