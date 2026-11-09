#pragma once
#include "dynamic_effect_model.hpp"
#include "dynamic_land_consequence.hpp"
#include "dynamic_action.hpp"
#include "dynamic_land_commander.hpp"
#include "dynamic_land_command_shock.hpp"
#include "dynamic_land_disposition.hpp"
namespace hanmo::resident {
// Borrowed original-source reconstruction. Values are private calculation input,
// never a second saved current state or caller-provided damage/eligibility flags.
struct DynamicLandEffectOriginalInputs {
 StableId session_ref;Integer generation=0;WorldMoment effective_at,started_at;
 SourceRef actual_effect_group_source;StableId original_battlefield_ref;
 SourceRef initialization_source,actual_contact_source;StableId original_casualty_rule_ref;
 std::vector<SourceRef> complete_due_effect_refs,precondition_source_refs,geometry_basis_refs;
 std::vector<DynamicMilitaryEffectOnceKey2> once_keys;
 std::vector<DynamicLandPostureUnit> before_effects;
 std::vector<DynamicLandActivationInput> actual_activations;
 // Original strike selections and geometry; the A layer supplies their actual
 // same-M morale/points/status/posture snapshots before quoting any attack.
 std::vector<DynamicLandStrikeInput> original_strike_inputs;
 DynamicLandCasualtyRule casualty_rule=DynamicLandCasualtyRule::legacy_factors;
 DynamicMilitarySessionRandom rng_before;
 std::vector<DynamicCommandShockLeaderRead> command_shock_leaders;
 std::vector<DynamicActualWithdrawalRead> actual_withdrawals;
};
class DynamicMilitaryEffectOriginalRead {
public:
 virtual ~DynamicMilitaryEffectOriginalRead()=default;
 // Resolves original action/entry/definition/geometry/condition source heads at
 // the retained group's real cut. It must never substitute today's current.
 virtual Outcome<DynamicLandEffectOriginalInputs> read_original_land_effect_inputs(const DynamicBattleEffectGroup2&)const=0;
 // Complete F/G consumer coverage, including commander/rout, S4/S5, objective,
 // CLC and retained committed effects. Empty/no-effect is read from real owners.
 virtual Status validate_original_effect_group_consumers(const DynamicBattleEffectGroup2&)const=0;
};
struct DynamicLandEffectCalculation {
 DynamicLandPostureMerge postures;
 std::vector<DynamicLandStrikeInput> strike_inputs;
 std::vector<DynamicLandAllocationUnit> strike_snapshot;
 std::vector<DynamicLandQualifiedSupport> actual_supports;
 DynamicLandStrikeMerge strikes;
 std::vector<DynamicLandStatusChange> strike_status_changes;
 DynamicLandCommanderMerge commander_losses;
 DynamicLandCommandShockMerge command_shock;
 DynamicLandDispositionMerge dispositions;
};
Outcome<DynamicLandEffectCalculation> calculate_dynamic_land_effect_group(
 const MilitaryBattleCatalog&,const DynamicMilitaryActionCatalog2&,
 const DynamicLandEffectOriginalInputs&,ExactArithmeticBudget={});
// Original pure calculation adopts only fixed A/E result fields. The caller is
// the actual complete group producer and separately supplies real consumer refs.
Status adopt_dynamic_land_calculated_fields(DynamicBattleEffectGroup2&,
 const DynamicLandEffectOriginalInputs&,const DynamicLandEffectCalculation&);
Status validate_dynamic_land_calculated_fields(const MilitaryBattleCatalog&,
 const DynamicMilitaryActionCatalog2&,const DynamicLandEffectOriginalInputs&,
 const DynamicBattleEffectGroup2&,ExactArithmeticBudget={});
class NativeDynamicMilitaryLandConsequenceSources final:public DynamicMilitaryLandConsequenceSources {
 const CommonStore& common_;const MilitaryExactState& exact_;
 const MilitaryBattleCatalog& battle_catalog_;const DynamicMilitaryActionCatalog2& action_catalog_;
 const DynamicMilitaryEffectOriginalRead& originals_;const CurrentMomentRead& clock_;
 ExactArithmeticBudget budget_;
public:
 NativeDynamicMilitaryLandConsequenceSources(const CommonStore&,const MilitaryExactState&,
  const MilitaryBattleCatalog&,const DynamicMilitaryActionCatalog2&,
  const DynamicMilitaryEffectOriginalRead&,const CurrentMomentRead&,ExactArithmeticBudget={});
 Outcome<DynamicMilitaryLandConsequenceRead> read_actual_land_consequence(const SourceRef&)const override;
};
// Stable cold validation is stronger than the same-candidate casualty consumer:
// every retained group must have the complete original F/G consumer closure.
Status validate_dynamic_military_effect_groups(const CommonStore&,const MilitaryState&,
 const MilitaryExactState&,const MilitaryBattleCatalog&,const DynamicMilitaryActionCatalog2&,
 const DynamicMilitaryEffectOriginalRead&,const CurrentMomentRead&,ExactArithmeticBudget={});
}
