#pragma once
#include "training_exact.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/source_retention.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// Original accepted command/Office, roster, resources, and real whole-span
// participation are source-local reads. They may not re-enter S0 history.
class ExactFormationOriginalRead {
public:
 virtual ~ExactFormationOriginalRead()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&)const noexcept=0;
 virtual Status validate_formation_acceptance(const DynamicMilitaryActionFact2&)const=0;
 virtual Status validate_formation_actual_service(const DynamicMilitaryActionFact2&,const MomentSpan&)const=0;
 virtual Status validate_formation_completed_result(const DynamicMilitaryActionFact2&)const=0;
};
struct ExactFormationReadContext {
 const MilitaryCatalog& catalog;const MilitaryAccess& access;const TimeScaleRef& scale;
 const ExactFormationOriginalRead& original;const ExactTrainingPeriodRead& periods;const CurrentMomentRead& clock;
 ExactArithmeticBudget budget{};
};
// Genuine original garrison ownership-at-start join. Other accepted Office,
// roster, body, resource and whole-service obligations remain independent.
Status validate_exact_formation_garrison_acceptance(const CommonStore&,const MilitaryState&,
 const ExactFormationReadContext&,const DynamicMilitaryActionFact2&);
// Finite actual authority leaf only; other original acceptance obligations remain.
Status validate_exact_formation_campaign_authority(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ExactFormationReadContext&,const DynamicMilitaryActionFact2&);
// Original successful completion rechecks Office and friendly garrison at its
// actual application head. Body, campaign state and service remain independent.
Status validate_exact_formation_completion_authority(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ExactFormationReadContext&,const DynamicMilitaryActionFact2&);
// Original pre-start mobilization/training availability, distinct from the
// independent roster/fit/commander, army membership and resource proofs.
Status validate_exact_formation_unit_activity_acceptance(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactFormationReadContext&,
 const DynamicMilitaryActionFact2&);
struct ExactFormationWriteContext {
 ExactFormationReadContext read;std::function<Status(const StableId&)> release_action;
 // After real resource/body/row adoption and its Common completion source,
 // bind each successful original location tap, install the unique Army writer,
 // and consume actual spatial input changes in the same candidate.
 std::function<Status(const DynamicMilitaryActionFact2&,Integer first_location_sequence)> adopt_actual_locations;
};
// Real original Opening selected at completion: fractional P_T; integer P_(T-1).
// This is the initialization cursor label only and never fabricates Supply.
Outcome<ExactTrainingPeriodView> read_exact_formation_supply_period(const ExactFormationReadContext&,const WorldMoment& actual_completed_at);
ActionTimeRegistration exact_formation_action_registration();
class NativeFormationActionTimeRead final:public ActionTimeOriginalSourceRead {
 const CommonStore& common_;const MilitaryState& military_;const MilitaryActionTimeState& state_;
 const ActionElapsedStore& elapsed_;ExactFormationReadContext context_;
public:
 NativeFormationActionTimeRead(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& e,ExactFormationReadContext x):common_(c),military_(m),state_(s),elapsed_(e),context_(x){}
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
Outcome<StableId> start_exact_formation_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactFormationWriteContext&,const ActionCoreView&,const ArmyForm&,const MilitaryQuote&);
Status advance_exact_formation_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactFormationWriteContext&,const StableId&,const MomentSpan&);
Outcome<MilitaryEffect> complete_exact_formation_action_in_candidate(CommonStore&,MilitaryState&,MilitaryActionTimeState&,ActionElapsedStore&,const ExactFormationWriteContext&,const StableId&);
Status validate_exact_formation_sources(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactFormationReadContext&,bool allow_current_completion_candidate=false);
Outcome<const DynamicMilitaryActionFact2*> read_verified_exact_formation_result(const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,const ExactFormationReadContext&,const SourceRef&,Integer schema);
// The Common completion application has its own source ID. The completed
// owned-field is addressed by Action ID; these are never interchangeable.
Outcome<const DynamicMilitaryActionFact2*> read_verified_exact_formation_application(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,const ActionElapsedStore&,
 const ExactFormationReadContext&,const SourceRef& actual_application,Integer schema);

// Source-local genuine failed completion check and reservation release. No
// whole acceptance/roster/occupation recursion and no Army/placement application.
Outcome<const DynamicMilitaryActionFact2*> read_exact_formation_completion_check(
 const CommonStore&,const MilitaryState&,const MilitaryActionTimeState&,
 const ActionElapsedStore&,const TimeScaleRef&,const CurrentMomentRead&,
 const SourceRef&,Integer source_schema,ExactArithmeticBudget={});
Status collect_exact_formation_check_pins(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ActionElapsedStore&,const TimeScaleRef&,
 const CurrentMomentRead&,SourcePinSink&,ExactArithmeticBudget={});
Status validate_exact_formation_check_pin(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ActionElapsedStore&,const TimeScaleRef&,
 const CurrentMomentRead&,const SourcePin&,ExactArithmeticBudget={});
std::vector<RequiredSourceConsumerSlot> exact_formation_check_consumer_slots();
Outcome<bool> exact_formation_check_source_may_release(const CommonStore&,const MilitaryState&,
 const MilitaryActionTimeState&,const ActionElapsedStore&,const TimeScaleRef&,
 const CurrentMomentRead&,const SourceRef&,Integer source_schema,ExactArithmeticBudget={});

}
