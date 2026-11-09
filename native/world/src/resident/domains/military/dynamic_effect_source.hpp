// One actual effect source, retained inside the original same-M group.
#pragma once
#include "dynamic_effect.hpp"
namespace hanmo::resident {
// The original Common append position of a finite actual military head.
// This is a read cut, never a stored revision or proof of its business payload.
Outcome<std::size_t> read_dynamic_military_fact_position(const CommonStore&,const SourceRef&);
// Borrowed actual group -> all original action once keys -> one generation.
Outcome<Integer> read_dynamic_military_effect_group_generation(
 const MilitaryExactState&,const DynamicBattleEffectGroup2&);
Outcome<const DynamicTacticPointChange2*> read_dynamic_military_tactic_point_change(
 const CommonStore&,const MilitaryExactState&,const SourceRef&,Integer schema,
 const StableId& session,Integer generation,const StableId& unit,
 const WorldMoment& actual_now,ExactArithmeticBudget={});
// Returns only the original activation that actually installed the retained
// row, not a later refresh which kept that row. No current mutation or replay.
Outcome<const DynamicLandStatusChange*> read_dynamic_military_retained_status_source(
 const CommonStore&,const MilitaryExactState&,const SourceRef& retained_activation,Integer schema,
 const StableId& session,Integer generation,const StableId& unit,DynamicMilitaryStatusKind,
 const WorldMoment& actual_now,ExactArithmeticBudget={});
struct DynamicMilitaryPostureOriginalSources {
 const DynamicLandActivationResult* actual_activation=nullptr;
 const DynamicSupportConsumption2* actual_support_consumption=nullptr;
};
// Borrow only the true installed posture and, for Support, its one later
// consumed strike. The professional eligibility/closure owner remains required.
Outcome<DynamicMilitaryPostureOriginalSources> read_dynamic_military_posture_original_sources(
 const CommonStore&,const MilitaryExactState&,const StableId& session,Integer generation,
 const StableId& unit,const DynamicMilitaryPosture&,const WorldMoment& actual_now,
 ExactArithmeticBudget={});
// Ordinal 0 is this action's one activation/release. Ordinal 1 is its actual
// irrevocable projectile impact. There is no placeholder source for an absent 0.
Outcome<DynamicMilitaryActualEffectRead> read_dynamic_military_effect_source(
 const CommonStore&,const MilitaryExactState&,const DynamicMilitaryActionCatalog2&,
 const SourceRef&,Integer source_schema,ExactArithmeticBudget={});
// Stable publication must prove both directions against the original action.
Status validate_dynamic_military_effect_action_coverage(const CommonStore&,
 const MilitaryExactState&,const DynamicMilitaryActionCatalog2&,ExactArithmeticBudget={});
}
