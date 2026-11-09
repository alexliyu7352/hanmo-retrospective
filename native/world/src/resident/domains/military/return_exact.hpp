#pragma once
#include "training_exact_model.hpp"
#include "military_exact_sources.hpp"
#include "military_formation_source.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct ExactReturnSourceReadContext;
struct MilitaryFieldHistoryRead;
struct ExactPeriodContext;
struct PopulationMilitaryState;
struct PopulationMilitaryCatalog;
class ExactTrainingPeriodRead;
class CharacterAvailabilityProvenanceSink;
// Accepted Army/Office/ownership history, actual whole-span participation and
// original resource results. Current army members or Office holders cannot prove
// their historical values. These source-local reads do not re-enter S0.
class ExactReturnOriginalRead {
public:
 virtual ~ExactReturnOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&)const noexcept=0;
 virtual Status validate_return_acceptance(const DynamicMilitaryLogisticsFact2&)const=0;
 virtual Status validate_return_actual_service(const DynamicMilitaryLogisticsFact2&,const MomentSpan&)const=0;
 virtual Status validate_return_completed_result(const DynamicMilitaryLogisticsFact2&)const=0;
};
struct ExactReturnReadContext {
 const MilitaryCatalog& catalog;const MilitaryAccess& access;const TimeScaleRef& scale;
 const ExactReturnOriginalRead& original;const CurrentMomentRead& clock;
 const MilitaryExactState* exact;const MilitaryFormationSourceRead& formations;
 const CharacterConditionSources& conditions;
 const PopulationMilitaryState& population;
 const PopulationMilitaryCatalog* population_catalog;
 const ExactTrainingPeriodRead& periods;
 ExactArithmeticBudget budget{};
 const CatalogIdentity* identity=nullptr;const MilitaryFieldHistoryRead* field_history=nullptr;
 ExactReturnReadContext(const MilitaryCatalog& c,const MilitaryAccess& a,const TimeScaleRef& s,const ExactReturnOriginalRead& o,const CurrentMomentRead& k,const MilitaryExactState& e,const MilitaryFormationSourceRead& f,const CharacterConditionSources& body,const PopulationMilitaryState& p,const PopulationMilitaryCatalog* pc,const ExactTrainingPeriodRead& periods_,ExactArithmeticBudget b={}):catalog(c),access(a),scale(s),original(o),clock(k),exact(&e),formations(f),conditions(body),population(p),population_catalog(pc),periods(periods_),budget(b){}
 ExactReturnReadContext(const MilitaryCatalog& c,const MilitaryAccess& a,const TimeScaleRef& s,const ExactReturnOriginalRead& o,const CurrentMomentRead& k,const CatalogIdentity& i,const MilitaryExactState* e,const MilitaryFormationSourceRead& f,const CharacterConditionSources& body,const PopulationMilitaryState& p,const PopulationMilitaryCatalog* pc,const ExactTrainingPeriodRead& periods_,ExactArithmeticBudget b={},const MilitaryFieldHistoryRead* h=nullptr):catalog(c),access(a),scale(s),original(o),clock(k),exact(e),formations(f),conditions(body),population(p),population_catalog(pc),periods(periods_),budget(b),identity(&i),field_history(h){}

};
// Real original formation→integration→casualty roster and original alive filter
// at the actual start head. No full acceptance/occupation recursion. Returned
// pointers borrow retained original rows for this read-only candidate lifetime.
Outcome<CheckpointVector<const StableId*>> read_exact_return_original_participants(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,
 const ExactReturnReadContext&,const StableId& actual_action_id);
// Same actual participant reducer, with every checked availability dependency
// forwarded synchronously to its original owner sink. Discard sink output if
// this read fails. Typed unresolved owner registrations remain the caller's
// explicit obligation, never invented here as SourceRef/schema values.
Outcome<CheckpointVector<const StableId*>> visit_exact_return_participant_availability_provenance(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,
 const ExactReturnReadContext&,const StableId& actual_action_id,
 CharacterAvailabilityProvenanceSink&);
// Genuine original garrison ownership-at-start join. Other accepted Office,
// roster, body, resource and whole-service obligations remain independent.
Status validate_exact_return_garrison_acceptance(const CommonStore&,const MilitaryState&,
 const ExactReturnReadContext&,const DynamicMilitaryLogisticsFact2&);
// Finite actual authority leaf only; other original acceptance obligations remain.
Status validate_exact_return_office_authority(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ExactReturnReadContext&,const DynamicMilitaryLogisticsFact2&);
// Original successful unload rechecks friendly ownership at its real application.
Status validate_exact_return_garrison_completion(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ExactReturnReadContext&,const DynamicMilitaryLogisticsFact2&);
Status validate_exact_return_garrison_completion(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ExactReturnSourceReadContext&,const DynamicMilitaryLogisticsFact2&);
struct ExactReturnWriteContext {
 ExactReturnReadContext read;std::function<Status(const StableId&)> release_action;
 // Real same-candidate location taps are bound after their original result and
 // Common completion source exist. The S1 callback owns the actual bindings.
 std::function<Status(const DynamicMilitaryLogisticsFact2&,Integer first_location_sequence)> adopt_actual_locations;
};
ActionTimeRegistration exact_return_action_registration();
class NativeReturnActionTimeRead final:public ActionTimeOriginalSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryActionTimeState& state_;
 const ActionElapsedStore& elapsed_;ExactReturnReadContext context_;
public:
 NativeReturnActionTimeRead(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& e,ExactReturnReadContext x):common_(c),military_(m),state_(s),elapsed_(e),context_(x){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
Outcome<StableId> start_exact_return_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactReturnWriteContext&,const ActionCoreView&,const ArmyLogistics&,const MilitaryQuote&);
Status advance_exact_return_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactReturnWriteContext&,const StableId&,const MomentSpan&);
Outcome<MilitaryEffect> complete_exact_return_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactReturnWriteContext&,const StableId&);
Status validate_exact_return_sources(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactReturnReadContext&,bool allow_current_completion_candidate=false);
Outcome<const DynamicMilitaryLogisticsFact2*> read_verified_exact_return_result(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactReturnReadContext&,const SourceRef&,Integer schema);
Outcome<const DynamicMilitaryReturnFact2*> read_verified_exact_return_receipt(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,
 const ActionElapsedStore&,const ExactReturnReadContext&,const SourceRef&,Integer schema);
// Original campaign adoption remains at its real Calendar closing. The receipt
// itself is checked at its exact completed_at; no tick+1 completion is inferred.
Status acknowledge_exact_military_return_at_calendar_close_in_candidate(
 CommonStore&,MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,
 const ExactReturnReadContext&,const ExactTrainingPeriodRead&,const ExactPeriodContext&,
 const StableId& actual_order,const StableId& actual_return_receipt);

}
