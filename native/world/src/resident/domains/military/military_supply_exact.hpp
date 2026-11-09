#pragma once
#include "military_exact_sources.hpp"
#include "training_exact.hpp"
#include "military_formation_source.hpp"
#include "military_return_source.hpp"
namespace hanmo::resident {
struct ExactPeriodContext;
struct GroupMilitarySupplyRebindSource;
// Code-owned identity of the existing feed() rule: genuine shortage adds four
// fatigue to every named mouth, including a same-value capped write. No new rate.
inline constexpr const char* original_military_supply_rule_ref="military_supply_original_fatigue_add_four_v1";
struct ExactMilitarySupplySourceView {
 const MilitarySupplyFact* original_supply=nullptr;
 const MilitarySupplyEffectSource2* exact_source=nullptr;
 ExactTrainingPeriodView original_period;
};
// Field1 复用原 /1 Supply 正文与唯一实际 Opening；不拥有第二期间/供粮账。
struct FieldMilitarySupplySourceView {
 const MilitarySupplyFact* original_supply=nullptr;
 ExactTrainingPeriodView original_period;
};
Outcome<FieldMilitarySupplySourceView> read_field_military_supply_source(
 const CommonStore&,const MilitaryState&,const ExactTrainingPeriodRead&,
 const SourceRef&,Integer schema,const WorldMoment& actual_now,ExactArithmeticBudget={});
struct ExactMilitarySupplyActorConsequence {
 StableId actor_id;SourceRef original_supply_ref;StableId period_ref;
 MomentOwnerSource opening_source;WorldMoment occurred_at;
 Integer source_ordinal=0;StableId original_rule_ref;
};
Outcome<ExactMilitarySupplyActorConsequence> read_field_military_supply_actor_consequence(
 const CommonStore&,const MilitaryState&,const ExactTrainingPeriodRead&,
 const SourceRef&,Integer schema,Integer source_ordinal,const WorldMoment& actual_now,
 ExactArithmeticBudget={});
Outcome<std::vector<MomentOwnerSource>> read_field_military_supply_opening_sources(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const ExactTrainingPeriodRead&,const MilitaryFormationSourceRead&,Integer actual_period_from,
 const WorldMoment& actual_now,ExactArithmeticBudget={},const MilitaryReturnSourceRead* returns=nullptr);
// Source-local original row/Opening/actual Common-cut verification. This does
// not call body, Growth or whole Military validators. The actual military owner
// separately proves Opening mouth eligibility and resource/roster changes.
Outcome<ExactMilitarySupplySourceView> read_exact_military_supply_source(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,
 const ExactTrainingPeriodRead&,const SourceRef&,Integer schema,
 const WorldMoment& actual_now,ExactArithmeticBudget={});
// Ordinal is in the ORIGINAL complete mouth_ids, not in a filtered actor list.
// Non-Actor mouths or full supply cannot authorize a body mutation.
Outcome<ExactMilitarySupplyActorConsequence> read_exact_military_supply_actor_consequence(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,
 const ExactTrainingPeriodRead&,const SourceRef&,Integer schema,Integer source_ordinal,
 const WorldMoment& actual_now,ExactArithmeticBudget={});
// Exact batch coverage is read from the original formation/return/integration
// sources at the actual Opening Common cut, never from today's active Army set.
Outcome<std::vector<MomentOwnerSource>> read_exact_military_supply_opening_sources(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryCatalog&,
 const MilitaryAccess&,const ExactTrainingPeriodRead&,const MilitaryFormationSourceRead&,
 Integer actual_period_from,const WorldMoment& actual_now,ExactArithmeticBudget={},const MilitaryReturnSourceRead* returns=nullptr);
struct ExactMilitarySupplyMouthResponsibility {
 ExactMilitarySupplySourceView original;
 Integer original_mouth_ordinal=0;
};
// A missing responsibility is a positive read of the COMPLETE actual Opening
// batch. Calling before that batch is installed fails; it cannot authorize a
// second household payment during an unfinished military Opening.
Outcome<std::optional<ExactMilitarySupplyMouthResponsibility>> read_exact_military_supply_mouth_responsibility(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,const MilitaryCatalog&,
 const MilitaryAccess&,const ExactTrainingPeriodRead&,const MilitaryFormationSourceRead&,
 Integer actual_period_from,const StableId& actual_mouth,const WorldMoment& actual_now,
 ExactArithmeticBudget={},const MilitaryReturnSourceRead* returns=nullptr);
class MilitarySupplyExactConditionWrite {
public:
 virtual ~MilitarySupplyExactConditionWrite()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryExactState&)const noexcept=0;
 // The actual Supply head and exact period source are installed. The concrete
 // S4 writer reads each original mouth ordinal, writes its unique Condition,
 // and immediately consumes each fresh capability token before the next actor.
 virtual Status apply_actual_supply_conditions_in_candidate(const SourceRef& actual_supply)=0;
};
struct ExactMilitaryStateValidationContext;
// Opening was actually adopted by its original owner before this function. Its
// real from Moment equals the trusted current phase; tick is only its to label.
Outcome<std::vector<MomentOwnerSource>> open_exact_military_supply_in_candidate(
 CommonStore&,MilitaryState&,MilitaryExactState&,const MilitaryCatalog&,const MilitaryAccess&,
 const ExactMilitaryStateValidationContext&,const ExactPeriodContext&,
 MilitarySupplyExactConditionWrite&);
}

namespace hanmo::resident {
// Same original Opening batch; no legacy dynamic root and no second supply table.
Outcome<std::vector<MomentOwnerSource>> open_field_military_supply_in_candidate(
 CommonStore&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const ExactMilitaryStateValidationContext&,const ExactPeriodContext&,
 const std::vector<GroupMilitarySupplyRebindSource>& actual_supply_movements);
}
