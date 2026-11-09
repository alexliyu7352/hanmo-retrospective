#pragma once
#include "military_march_contract.hpp"
#include "military_finite_return_source.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "military_formation_source.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct MilitaryAuthoritySourceContext;
// 非持久选型与原只读借口一起传递；来源由外层不移动 Scope 拥有，不能借局部临时值。
struct MilitaryMarchAuthorSelection {
 StableId profile_id;Integer military_schema_version=0;
 Integer selected_authority_time=0;
 const MilitaryAuthoritySourceContext* authority_sources=nullptr;
};
// Uses the original preview/apply command, not a caller-authored authority DTO.
// The S1 caller has already allocated the candidate's unique opening/prefix;
// it must roll the complete candidate back on any error from this function.
Status adopt_military_campaign_departure_in_candidate(
    CommonStore&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
    const CurrentReadStampRead&,const CharacterConditionSources&,const MilitaryMarchAuthorSelection&,SpatialCarrierState&,const MovementExactState&,
    GroupMovementExecutionState&,const StableId& journey_id,
    const StableId& issuing_actor_id,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr);
Outcome<const MilitaryGroupOrderAcceptance*> read_military_group_order_acceptance(
    const CommonStore&,const MilitaryState&,const MilitaryCatalog&,
    const SpatialCarrierState&,const MovementExactState&,
    const GroupMovementExecutionState&,const CharacterConditionSources&,const SourceRef& opening_source,
    const WorldMoment& actual_current,const MilitaryMarchAuthorSelection&,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryFiniteReturnSourceRead* finite_returns=nullptr);
// Current acceptance and historical spending use the same original source.
Status validate_military_march_expense_authority(
    const CommonStore&,const MilitaryState&,const MilitaryCatalog&,
    const SpatialCarrierState&,const MovementExactState&,
    const GroupMovementExecutionState&,const CharacterConditionSources&,const MilitaryMarchExpenseAuthority&,
    const StableId& journey_id,const WorldMoment& actual_current,
    const MilitaryMarchAuthorSelection&,ExactArithmeticBudget budget={},const MilitaryFormationSourceRead* formations=nullptr,const MilitaryFiniteReturnSourceRead* finite_returns=nullptr);
// Candidate-owning factory binds these references from the WorldBundle passed
// to the current callback. No compiled callback captures a live world.
class NativeMilitaryMarchExpenseRead final:public MilitaryMarchExpenseSourceRead {
public:
 NativeMilitaryMarchExpenseRead(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const SpatialCarrierState&,const MovementExactState&,const GroupMovementExecutionState&,const CharacterConditionSources&,const TravelCatalog&,const CurrentReadStampRead&,MilitaryMarchAuthorSelection,const MilitaryFormationSourceRead* formations=nullptr,const MilitaryFiniteReturnSourceRead* finite_returns=nullptr);
 Outcome<WorldMoment> read_current_moment()const override;
 Outcome<ActualMilitaryMarchExpense> read_actual_military_march_expense(const SourceRef&,Integer,const MilitaryMarchExpenseAuthority&)const override;
 Outcome<std::optional<MilitaryMarchWalletDebit>> read_recorded_military_march_debit(const SourceRef&,Integer)const override;
private:
 const CommonStore& common_;const MilitaryState& military_;const MilitaryCatalog& military_catalog_;
 const SpatialCarrierState& carriers_;const MovementExactState& movement_;const GroupMovementExecutionState& executions_;
 const CharacterConditionSources& bodies_;const TravelCatalog& travel_catalog_;const CurrentReadStampRead& clock_;MilitaryMarchAuthorSelection author_;const MilitaryFormationSourceRead* formations_;const MilitaryFiniteReturnSourceRead* finite_returns_;
};
}
