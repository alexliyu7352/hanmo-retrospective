// S2 section 2.3 B/C/D. Quotes are all computed before the first original draw.
#pragma once
#include "dynamic_attack_quote.hpp"
#include "dynamic_action_model.hpp"
#include "dynamic_land_allocation.hpp"
#include "dynamic_session_random.hpp"
namespace hanmo::resident {
struct DynamicLandStrikeInput {
    SourceRef effect_ref;StableId action_ref;Integer action_instance_ordinal=0,effect_ordinal=0;
    StableId battlefield_id,tactic_id;
    // A real committed projectile retains its shooter snapshot even after death.
    std::optional<SourceRef> actual_release_source;
    DynamicMilitaryDamageLeaf original_damage_leaf=DynamicMilitaryDamageLeaf::legacy_factors;
    Integer original_damage_version=1;
    BattleUnit original_attacker,original_target;
    DynamicMilitaryAttackGeometry actual_geometry;
    DynamicMilitaryAttackConditions actual_conditions;
    std::vector<SourceRef> original_precondition_sources,original_geometry_sources;
    StableId timing_definition_ref;
};
enum class DynamicLandStrikeFailure { none,target_unavailable,friendly_target,out_of_range,sight_blocked,flank_needs_movement,flank_needs_side_or_rear };
struct DynamicLandStrikeResult {
    SourceRef effect_ref;StableId action_ref,attacker_unit_ref,target_unit_ref;
    Integer action_instance_ordinal=0,effect_ordinal=0;
    DynamicMilitaryDamageLeaf original_damage_leaf=DynamicMilitaryDamageLeaf::legacy_factors;
    Integer original_damage_version=1;
    DynamicLandStrikeFailure failure=DynamicLandStrikeFailure::none;
    Integer raw=0,potential=0,chance=0,flank_morale=0,tactic_morale=0;
    std::optional<DynamicMilitaryRandomSlot> actual_random;
};
struct DynamicLandStrikeMerge {
    std::vector<DynamicLandStrikeResult> strike_results;
    std::vector<DynamicMilitaryRandomSlot> rng_slots;
    std::vector<DynamicLandStrikeDamage> complete_strikes;
    DynamicLandMergedAllocation allocation;
    DynamicMilitarySessionRandom rng_after;
};
// Pure whole-group preparation. Input qualification/source validity belongs to
// the real effect-group owner. No Common/S2 state, world clock or current RNG is
// mutated; technical failure discards the local quote/draw preparation.
Outcome<DynamicLandStrikeMerge> merge_dynamic_land_strikes(
    const MilitaryBattleCatalog&,const std::vector<DynamicLandStrikeInput>& complete_inputs,
    const std::vector<DynamicLandAllocationUnit>& full_strike_snapshot,
    const std::vector<DynamicLandQualifiedSupport>& actual_supports,
    DynamicLandCasualtyRule,const DynamicMilitarySessionRandom& rng_before,
    ExactArithmeticBudget budget={});
// Cold validation repeats only original pure mathematics against retained input;
// it never advances the live stream or creates a second business draw.
Status validate_dynamic_land_strikes(const MilitaryBattleCatalog&,
    const std::vector<DynamicLandStrikeInput>&,const std::vector<DynamicLandAllocationUnit>&,
    const std::vector<DynamicLandQualifiedSupport>&,DynamicLandCasualtyRule,
    const DynamicMilitarySessionRandom& rng_before,const DynamicLandStrikeMerge&,
    ExactArithmeticBudget budget={});
}
