// S2 §2.4: the one immutable effect-group row. No tactical current is duplicated.
#pragma once
#include "dynamic_land_postures.hpp"
#include "dynamic_land_strikes.hpp"
namespace hanmo::resident {
// Point changes are one summed/clamped change per unit, after all simultaneous
// activations. They remain separate from each individual activation's outcome.
struct DynamicTacticPointChange2 {
 StableId unit_ref;Integer before_points=0,delta=0,cap=0,after_points=0;
 std::optional<SourceRef> actual_cost_source_ref;
};
struct DynamicSupportConsumption2 {
 StableId supporter_unit_ref,target_unit_ref;
 SourceRef activation_ref,consumed_by_effect_ref;
 Integer actual_shared_members=0;
};
using DynamicPostureEffect2=std::variant<DynamicLandActivationResult,DynamicTacticPointChange2,
 DynamicLandReformChange,DynamicSupportConsumption2>;
struct DynamicBattleStrikeResult2 {
 // actual_random is absent here: the single saved random evidence is rng_slots.
 DynamicLandStrikeResult original_result;
 Integer actual_target_count=0,actual_support_count=0;
 std::optional<SourceRef> consumed_support_activation_ref;
 std::optional<StableId> supporter_unit_ref;
 // References to the unique member_allocations of this same original group.
 // Identity is the real military member ID, never another count or a new roster.
 std::vector<StableId> target_member_refs,support_member_refs;
};
enum class DynamicMoraleLayer2 {postures,casualties,original_consequences};
struct DynamicBattleMoraleChange2 {
 DynamicMoraleLayer2 layer=DynamicMoraleLayer2::postures;
 DynamicLandMoraleChange original_change;
};
// The naval owner supplies actual original damage/condition sources. Land groups
// require this array empty; Disabled and sunk/claim remain distinct native results.
struct DynamicBattleVesselChange2 {
 StableId vessel_ref;Integer before_integrity=0,actual_damage=0,after_integrity=0;
 std::vector<SourceRef> original_effect_refs,actual_result_refs;
};
struct DynamicMilitaryEffectOnceKey2 {
 StableId session_ref,action_ref;Integer effect_ordinal=0;
};
struct DynamicBattleEffectGroup2 {
 StableId id,session_ref;WorldMoment effective_at;StableId solver_ref;
 std::vector<SourceRef> due_effect_refs,precondition_source_refs,geometry_basis_refs;
 std::vector<DynamicPostureEffect2> posture_effects;
 std::vector<DynamicBattleStrikeResult2> strike_results;
 std::vector<DynamicMilitaryRandomSlot> rng_slots;
 std::vector<DynamicLandMemberAllocation> member_allocations;
 std::vector<DynamicBattleMoraleChange2> morale_changes;
 std::vector<DynamicLandStatusChange> status_changes;
 std::vector<DynamicBattleVesselChange2> vessel_changes;
 std::vector<SourceRef> condition_source_refs,population_source_refs;
 std::optional<SourceRef> objective_result_ref;
 std::vector<SourceRef> terminal_refs;
 std::vector<DynamicMilitaryEffectOnceKey2> once_keys;
};
}
