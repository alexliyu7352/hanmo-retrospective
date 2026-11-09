// S2 §2.3 A: whole-set posture/cost preparation before any strike quote.
#pragma once
#include "dynamic_action_model.hpp"
#include "dynamic_land_allocation.hpp"
namespace hanmo::resident {
// Sole exact value comparison, shared by the pure result, adoption and source readers.
bool dynamic_military_status_values_equal(const DynamicMilitaryStatus&,const DynamicMilitaryStatus&,ExactArithmeticBudget={});
bool dynamic_military_posture_values_equal(const DynamicMilitaryPosture&,const DynamicMilitaryPosture&,ExactArithmeticBudget={});
// Only immutable activation fields; Support's later actual consumption must
// be separately proved by its unique original strike/consumption source.
bool dynamic_military_posture_activation_values_equal(const DynamicMilitaryPosture&,const DynamicMilitaryPosture&,ExactArithmeticBudget={});
const SourceRef* dynamic_military_posture_activation(const DynamicMilitaryPosture&);

// The original A-layer bounded point/morale update. Inputs are already summed;
// this function does not choose a cost, target, cap or recovery entitlement.
Outcome<Integer> clamp_dynamic_military_bounded_delta(Integer before,Integer delta,Integer cap);
struct DynamicLandPostureUnit {
 BattleUnit original_unit;DynamicMilitaryUnitTacticalState original_tactical;
 MilitarySpatialPoint actual_position;SourceRef position_source;
 std::vector<SourceRef> actual_qualification_sources;
 // Real original learned-entry/command-shock sources, never caller booleans.
 std::optional<SourceRef> learned_tactic_entry_source,confused_source;
};
struct DynamicLandActivationInput {
 SourceRef effect_ref;StableId action_ref,unit_ref,timing_definition_ref;
 Integer action_instance_ordinal=0,effect_ordinal=0;MilitaryOrder original_order;
 std::vector<SourceRef> original_service_sources;
};
enum class DynamicLandActivationFailure {none,unit_unavailable,insufficient_points,support_unavailable,reform_not_required};
struct DynamicLandActivationResult {
 SourceRef effect_ref;StableId action_ref,unit_ref;Integer action_instance_ordinal=0,effect_ordinal=0;
 StableId original_action_kind,original_tactic_ref;DynamicLandActivationFailure failure=DynamicLandActivationFailure::none;
 Integer original_point_cost=0;std::vector<StableId> actual_target_unit_refs;
 std::optional<DynamicMilitaryPosture> installed_posture;
 std::vector<SourceRef> original_qualification_sources;
};
struct DynamicLandPointChange {
 StableId unit_ref;Integer before_points=0,delta=0,cap=0,after_points=0;
 std::vector<SourceRef> actual_effect_refs;std::vector<SourceRef> actual_cost_source_refs;
};
struct DynamicLandStatusChange {
 StableId unit_ref;DynamicMilitaryStatusKind kind=DynamicMilitaryStatusKind::inspired;
 std::optional<DynamicMilitaryStatus> before;DynamicMilitaryStatus after;
 SourceRef actual_activation_ref;
};
struct DynamicLandReformChange {StableId unit_ref;SourceRef actual_confused_source,actual_activation_ref;};
struct DynamicLandPostureMerge {
 std::vector<DynamicLandPostureUnit> strike_snapshot;
 std::vector<DynamicLandActivationResult> posture_effects;
 std::vector<DynamicLandPointChange> point_changes;
 std::vector<DynamicLandMoraleChange> morale_changes;
 std::vector<DynamicLandStatusChange> status_changes;
 std::vector<DynamicLandReformChange> reform_changes;
};
// Inputs are the complete actual due set, qualified by the original action and
// entry owners. This layer reads the same before snapshot for every activation,
// aggregates original changes once, and has no Common/ID/RNG side effects.
Outcome<DynamicLandPostureMerge> merge_dynamic_land_postures(const MilitaryBattleCatalog&,
 const DynamicMilitaryActionCatalog2&,const std::vector<DynamicLandPostureUnit>& before_effects,
 const std::vector<DynamicLandActivationInput>& canonical_activations,const WorldMoment&,
 ExactArithmeticBudget budget={});
Status validate_dynamic_land_postures(const MilitaryBattleCatalog&,const DynamicMilitaryActionCatalog2&,
 const std::vector<DynamicLandPostureUnit>&,const std::vector<DynamicLandActivationInput>&,
 const WorldMoment&,const DynamicLandPostureMerge&,ExactArithmeticBudget budget={});
}
