// 原battle§15.12 /
// D§5.2。S2保存唯一军务与完整服务窗；不保存第二军籍/工时Action。
#pragma once
#include "../military_authority_contract.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
enum class MilitaryDutyKind { patrol,blockade,custody_guard };
enum class MilitaryDutyPhase { assembling,active,suspended,released };
enum class MilitaryDutyControlOperation { suspend,resume,cancel,return_to_owner };
enum class MilitaryDutyDisposition { provided,interrupted,suspended };
struct DutyTravelAction {StableId action_id,journey_ref;};
struct DutyStationAction {StableId action_id;};
struct DutyRegisteredMilitaryActivity {StableId activity_ref;Integer activity_schema_version=0;};
using DutyExecutionBinding=std::variant<DutyTravelAction,DutyStationAction,DutyRegisteredMilitaryActivity>;
struct StartMilitaryDuty {
    StableId order_or_assignment_ref,duty_rule_id;std::vector<StableId> unit_ids,named_guard_actor_ids,fleet_ids;
    StableId route_or_station_choice_ref;std::optional<Integer> expected_order_revision;
};
struct MilitaryDutyTerms {
    StableId duty_action_id,order_or_assignment_ref;MilitaryAuthorityRef authority_ref;
    MilitaryDutyKind duty_kind=MilitaryDutyKind::patrol;StableId leader_actor_id;
    std::vector<StableId> unit_ids,named_guard_actor_ids,fleet_ids;
    std::optional<DutyExecutionBinding> execution_binding;Integer start_tick=0,until_tick=0;
    StableId duty_rule_id,duty_rule_version,route_or_station_definition_ref;
    std::vector<PassageBoundaryRef> boundary_refs;std::vector<SourceRef> custody_source_refs;
    SourceRef supply_authorization_ref;StableId qualified_period_policy_ref,report_recipient_ref;
    MilitaryDutyPhase state=MilitaryDutyPhase::assembling;
};
struct DutyServiceWindowRef {StableId duty_action_id;Integer service_epoch=0,ordinal=0;WorldMoment opened_at,planned_close_at;};
// The original Common and applied-condition prefixes are distinct owners.
// This records their actual simultaneous consumption, not a new event ledger.
struct MilitaryDutyConditionConsumption {
    CommonActualFactPrefix common_prefix;
    CharacterConditionAppliedPrefix condition_prefix;
    std::vector<ActorConditionHead> consumed_heads;
};
struct MilitaryDutyQualification {
    std::vector<MilitaryParticipationRef> actual_participant_refs;
    std::vector<StableId> actual_member_ids,eligible_force_member_ids;
    bool leader_present=false,authority_valid=false,route_or_station_valid=false,supplied=false;
    std::vector<MomentOwnerSource> participant_basis_refs,route_or_station_sources,supply_sources,authority_sources;
    std::vector<MomentOwnerSource> encounter_result_refs,unresolved_conflict_refs;
    // Any normal failed condition names its actual original cause. Missing proof
    // remains a failed read, never a fabricated zero-attendance period.
    std::optional<MomentOwnerSource> unavailable_cause;
    // Mandatory object on /4 wire; absent only in the original /2 and /3.
    std::optional<MilitaryDutyConditionConsumption> condition_consumption;
};
struct MilitaryDutyServiceSegment {MomentSpan span;MilitaryDutyQualification qualification;};
struct DutyWindowOpen {};
struct DutyWindowClosed {StableId result_ref;};
struct MilitaryDutyWindowState {
    StableId id;DutyServiceWindowRef window;WorldMoment last_accounted_at;
    WorkUnits actual_service_units;std::vector<MomentOwnerSource> qualification_source_refs;
    Integer min_eligible_force_members=0;bool leader_present_throughout=true;
    std::vector<MomentOwnerSource> conflict_source_refs;std::variant<DutyWindowOpen,DutyWindowClosed> state;
    MilitaryDutyQualification opening_qualification;
    std::vector<MilitaryDutyServiceSegment> actual_segments;
    std::optional<MilitaryDutyQualification> closing_qualification;
};
// Non-persistent, synchronous owner/slot evidence. A current capture is never
// a fallback for a historical opening, segment or closing observation.
struct MilitaryDutyQualificationHistoryUse {
    enum class Kind {current_capture,window_opening,window_segment,window_closing};
    Kind kind=Kind::current_capture;
    const MilitaryDutyWindowState* window=nullptr;
    std::size_t segment_index=0;
};
struct MilitaryDutyPeriodFact2 {
    StableId fact_id;DutyServiceWindowRef window;WorldMoment actual_until;
    DutyExecutionBinding execution_binding;StableId order_or_assignment_ref;
    MilitaryDutyKind duty_kind=MilitaryDutyKind::patrol;StableId leader_actor_id;bool leader_present=false;
    std::vector<StableId> unit_ids;std::vector<MilitaryParticipationRef> actual_participant_refs;
    Integer actual_participant_count=0,eligible_force_member_count=0;
    std::vector<MomentOwnerSource> route_or_station_source_refs,participant_basis_refs,supply_fact_refs,encounter_result_refs,unresolved_conflict_refs;
    Integer provided_half_days=0;MilitaryDutyDisposition disposition=MilitaryDutyDisposition::interrupted;
    std::optional<MomentOwnerSource> reason_ref;
    StableId duty_rule_version;MilitaryAuthorityRef authority_ref;
};
struct MilitaryDutyRule {
    StableId id,version;MilitaryDutyKind kind=MilitaryDutyKind::patrol;
    Integer maximum_authorization_half_days=0,minimum_eligible_force_members=0;
    StableId qualified_period_policy_ref,leader_capability_purpose_ref,member_capability_purpose_ref;
    bool requires_leader=true,requires_supply=true,rejects_conflict=true;
};
struct MilitaryDutyCatalog {OrderedTable<MilitaryDutyRule> rules;};
struct MilitaryDutyAcceptedSource {StableId id,duty_action_id;WorldMoment accepted_at;SourceRef original_assignment_source;Integer assignment_source_schema=0;};
// This is the actual adoption source in the original Duty execution payload.
// It links the already-real S1 execution, and grants no new Action or clock.
struct MilitaryDutyActualBindingSource {
    StableId id;DutyExecutionBinding execution_binding;
    MomentOwnerSource original_execution_source;
    WorldMoment original_started_at,bound_at;
};
struct MilitaryDutyAuthorizationEnded {};
struct MilitaryDutyActualMovementEnded {MomentOwnerSource actual_terminal_source;};
struct MilitaryDutyMovementStopSource {
    StableId id;WorldMoment stopped_at;
    std::variant<MilitaryDutyAuthorizationEnded,MilitaryDutyActualMovementEnded> reason;
};
struct MilitaryDutyExecution {
    StableId duty_action_id,accepted_source_ref;Integer service_epoch=0,next_period_ordinal=0;
    std::optional<StableId> current_window_ref,last_result_ref;
    std::optional<MilitaryDutyActualBindingSource> actual_binding;
    std::optional<MilitaryDutyMovementStopSource> movement_stop;
};
struct MilitaryDutyControlSource {
    StableId id,duty_action_id;MilitaryDutyControlOperation operation=MilitaryDutyControlOperation::suspend;
    MomentOwnerSource current_authority_source;WorldMoment occurred_at;
    MilitaryDutyPhase resulting_phase=MilitaryDutyPhase::suspended;
    std::optional<MomentOwnerSource> safe_boundary_stop_source,continuation_source;
};
struct MilitaryStationActionSource {
    StableId id,duty_action_id;ActionCoreView actual_action;WorldMoment started_at;
    std::vector<MomentOwnerSource> actual_presence_sources;
};
struct MilitaryStationTerminalSource {
    StableId id,station_source_ref;WorldMoment ended_at;
    MomentOwnerSource original_cause;bool authorization_expired=false;
};
// Explicit dynamic_military/4 sources. This first route alternative names the
// entire original accepted PatrolPlan.route; it never copies one checkpoint.
struct MilitaryDutyWorkPatrolRoute {
    SourceRef work_acceptance_ref;StableId choice_ref;
};
struct MilitaryDutyAssignmentSource {
    StableId id;WorldMoment issued_at;StableId issuing_actor_id;
    MilitaryDutyKind duty_kind=MilitaryDutyKind::patrol;
    MilitaryAuthorityRef authority_ref;std::vector<MomentOwnerSource> authority_sources;
    StableId leader_actor_id;std::vector<StableId> unit_ids,named_guard_actor_ids,fleet_ids;
    Integer authorized_until_tick=0;StableId duty_rule_id,duty_rule_version,qualified_period_policy_ref;
    MilitaryDutyWorkPatrolRoute route_definition;
    MomentOwnerSource supply_authorization;
    std::optional<MomentOwnerSource> original_work_assignment;
};
// A request selects original keys, not an authority snapshot or a success flag.
// A represented selector is not a claim that its original permission reader is
// implemented. There is no public issuing writer until both actual command
// permission and the original supply-commitment authority can be proven.
struct MilitaryDutyLegacyAuthoritySelection {StableId office_id;};
struct MilitaryDutyOrganizationAuthoritySelection {StableId office_id,grant_id;};
struct MilitaryDutyPrivateAuthoritySelection {StableId agreement_id,command_assignment_id;};
using MilitaryDutyAuthoritySelection=std::variant<MilitaryDutyLegacyAuthoritySelection,MilitaryDutyOrganizationAuthoritySelection,MilitaryDutyPrivateAuthoritySelection>;
struct IssueMilitaryDutyAssignment {
    MomentOwnerSource original_work_assignment;
    MilitaryDutyAuthoritySelection authority;
    MomentOwnerSource supply_authorization;
    StableId duty_rule_id;Integer authorized_until_tick=0;StableId route_choice_ref;
};
struct MilitaryDutyState {
    OrderedTable<MilitaryDutyTerms> terms;OrderedTable<MilitaryDutyExecution> executions;
    OrderedTable<MilitaryDutyAcceptedSource> accepted_sources;OrderedTable<MilitaryDutyWindowState> windows;
    OrderedTable<MilitaryDutyPeriodFact2> period_facts;OrderedTable<MilitaryDutyControlSource> controls;
    OrderedTable<MilitaryStationActionSource> station_sources;OrderedTable<MilitaryStationTerminalSource> station_terminals;
    // Appended only by the explicitly selected /4 wire.
    OrderedTable<MilitaryDutyAssignmentSource> assignments;
};
// The same original command owner validates every issued source, including a
// source not yet consumed by Duty Terms. Implementations must use issued_at and
// its strict actual Common cut, not a synthetic start request or today's office.
class MilitaryDutyAssignmentSourceRead {
public:
    virtual ~MilitaryDutyAssignmentSourceRead()=default;
    virtual bool bound_to(const CommonStore&,const MilitaryDutyState&,const MilitaryDutyCatalog&)const noexcept=0;
    virtual Outcome<const MilitaryDutyAssignmentSource*> read_assignment(const SourceRef&,Integer source_schema)const=0;
};
struct MilitaryDutyStartRead {MilitaryDutyTerms accepted_terms;MomentOwnerSource assignment_source;std::vector<SourceRef> journey_request_refs;};
struct MilitaryDutyControlRead {MomentOwnerSource actual_authority;bool release_is_safe=false;std::optional<MomentOwnerSource> safe_boundary_stop_source,continuation_source;};
// These providers read their original records in this candidate. Neither public
// commands nor the window writer can submit an attendance count or success flag.
class MilitaryDutyOriginalRead {
public:
    virtual ~MilitaryDutyOriginalRead()=default;
    virtual Outcome<MilitaryDutyStartRead> read_start(const StartMilitaryDuty&,const WorldMoment&)const=0;
    virtual Status validate_accepted_terms(const MilitaryDutyTerms&,const MilitaryDutyAcceptedSource&)const=0;
    // Mandatory actual Common capture, not a substitute for source-specific
    // condition/authority history. Calls are synchronous, read-only and retain
    // neither this prefix nor any of its fields. Point captures and positive
    // half-open service spans keep their distinct original endpoint semantics.
    virtual Outcome<MilitaryDutyQualification> read_qualification(const MilitaryDutyTerms&,const MomentSpan&,const CommonActualFactPrefix& capture)const=0;
    virtual Status validate_qualification_history(const MilitaryDutyTerms&,const MilitaryDutyServiceSegment&,const CommonActualFactPrefix& capture,const MilitaryDutyQualificationHistoryUse&)const=0;
    virtual Outcome<MilitaryDutyControlRead> read_control(const MilitaryDutyTerms&,MilitaryDutyControlOperation,const SourceRef&,const WorldMoment&)const=0;
    virtual Status validate_control_history(const MilitaryDutyTerms&,const MilitaryDutyControlSource&)const=0;
    virtual Outcome<WorldMoment> read_actual_source_moment(const MomentOwnerSource&)const=0;
    virtual Status validate_execution_binding(const MilitaryDutyTerms&,const WorldMoment&)const=0;
};
// The S3 owner implements these two real same-candidate consumers. The Duty
// owner publishes its original binding/terminal first; neither callback grants
// attendance or completes the Work merely because the execution has started.
class MilitaryDutyExecutionConsumer {
public:
    virtual ~MilitaryDutyExecutionConsumer()=default;
    virtual Status begin(const StableId& duty_id,const MomentOwnerSource& actual_binding,const WorldMoment& bound_at)=0;
    virtual Status end(const StableId& duty_id,const MomentOwnerSource& actual_terminal,const WorldMoment& ended_at)=0;
};
class MilitaryDutyMovementSources;
struct CharacterConditionSources;
struct MilitaryDutyContext {const MilitaryDutyCatalog& catalog;const MilitaryDutyOriginalRead& original;const CurrentReadStampRead& stamps;const TimeScaleRef& scale;ExactArithmeticBudget budget={};std::function<Status(const StableId&)> release_action;const MilitaryDutyMovementSources* movement_executions=nullptr;MilitaryDutyExecutionConsumer* execution_consumer=nullptr;Integer selected_dynamic_military=0;const MilitaryDutyAssignmentSourceRead* assignments=nullptr;const CharacterConditionSources* condition_sources=nullptr;};
struct MilitaryDutyStartResult {StableId duty_action_id,accepted_terms_ref;std::optional<DutyExecutionBinding> execution_binding;std::vector<SourceRef> journey_request_refs;MilitaryDutyPhase state=MilitaryDutyPhase::assembling;};
Status validate_military_duty_catalog(const MilitaryDutyCatalog&);
// One finite layout gate for codec and native validation; never infer selection
// from empty source tables or absent optional evidence.
Status validate_military_duty_selected_layout(const MilitaryDutyState&,Integer selected_dynamic_military);
// Source-local slot/capture association only. Original qualification remains
// mandatory for actual roster, purpose, authority and complete span coverage.
Status validate_military_duty_qualification_history_use(const CommonStore&,const MilitaryDutyState&,const MilitaryDutyContext&,const MilitaryDutyTerms&,const MilitaryDutyServiceSegment&,const CommonActualFactPrefix&,const MilitaryDutyQualificationHistoryUse&);

Outcome<MilitaryDutyStartResult> start_military_duty_in_candidate(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const StartMilitaryDuty&,const WorldMoment&);
// Actual arrival/assignment activates the one original binding. A station action
// is created here only after the real stationary presence check succeeds.
Outcome<StableId> activate_military_station_in_candidate(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const StableId& duty,const WorldMoment&);
Outcome<StableId> open_military_duty_window_in_candidate(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const StableId& duty,const WorldMoment&);
Status provide_military_duty_span_in_candidate(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const StableId& duty,const MomentSpan&);
Outcome<StableId> close_military_duty_window_in_candidate(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const StableId& duty,const WorldMoment&,MilitaryDutyDisposition,const std::optional<MomentOwnerSource>& reason={});
Outcome<StableId> control_military_duty_in_candidate(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const StableId& duty,MilitaryDutyControlOperation,const SourceRef&,const WorldMoment&);
Status expire_military_station_in_candidate(CommonStore&,MilitaryDutyState&,const MilitaryDutyContext&,const StableId& duty,const WorldMoment&,const std::function<Status(const StableId&)>& original_release_action);
Outcome<const MilitaryDutyPeriodFact2*> read_military_duty_period(const CommonStore&,const MilitaryDutyState&,const MilitaryDutyContext&,const SourceRef&,Integer schema);
Status validate_military_duty_sources(const CommonStore&,const MilitaryDutyState&,const MilitaryDutyContext&,const WorldMoment&);
Status visit_military_duty_payloads(CodecVisitor&,MilitaryDutyState&);
Status visit_military_duty_rule(CodecVisitor&,MilitaryDutyRule&);
Status visit_military_authority_ref(CodecVisitor&,MilitaryAuthorityRef&);
Outcome<MilitaryDutyState> decode_military_duty_payloads(const BoundaryValue&);
Outcome<BoundaryValue> encode_military_duty_payloads(const MilitaryDutyState&);
Status emit_military_duty_payloads(ProjectionWriter&,std::string_view,const MilitaryDutyState&);
Outcome<MilitaryDutyRule> decode_military_duty_rule(const BoundaryValue&);
Status emit_military_duty_rule(ProjectionWriter&,std::string_view,const MilitaryDutyRule&);
ActionTimeRegistration military_station_action_time_registration();
class NativeMilitaryStationActionTimeRead final:public ActionTimeOriginalSourceRead {
public:
    NativeMilitaryStationActionTimeRead(const CommonStore&,const MilitaryDutyState&,const MilitaryDutyContext&);
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
private:const CommonStore& common_;const MilitaryDutyState& state_;const MilitaryDutyContext& context_;
};
}
