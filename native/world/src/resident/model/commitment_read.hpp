// Pure CK1 projection; no lifecycle records, mutable registry, or generic cancellation writer.
#pragma once
#include "commitment_ref.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/domains/affairs/state_need.hpp"
#include "resident/domains/social/knowledge_read.hpp"
namespace hanmo::resident {
enum class RegisteredCommitmentCommand {cancel_task,report_offer,cancel_dynamic_task,settle_task_contract,cancel_personal_appointment,collect_visit_appointment,accept_visit_appointment,cancel_visit_appointment,meet_visit_appointment,cancel_shop_order,deliver_shop_order,release_state_claim,report_state_task,settle_state_task,study_pause,study_resume,study_cancel,submit_challenge,cancel_treatment,amend_treatment,accept_transfer,cancel_patronage,amend_patronage,amend_ritual,cancel_ritual,return_custody,settle_ritual};
enum class CancellationGateState {available,needs_notice,needs_consent,blocked,not_applicable};
enum class SettlementGateState {not_required,pending,claimable,blocked,closed};
struct CommitmentDeadline {enum class Basis {start,completion,receipt};StateNeedDeadline gate;Basis basis=Basis::completion;};
struct CommitmentOperationGate {CancellationGateState state=CancellationGateState::blocked;std::optional<RegisteredCommitmentCommand> owner_command;std::optional<CommitmentDeadline> decision_gate;std::optional<StableId> visible_reason_code;};
struct CommitmentSettlementGate {SettlementGateState state=SettlementGateState::not_required;std::vector<SourceRef> outstanding_source_refs;std::optional<RegisteredCommitmentCommand> owner_command;std::optional<StableId> visible_reason_code;};
enum class TaskExecutionPhase {accepting,active,ready,success,failed,cancelled,settling};
struct TaskExecutionSummary {TaskExecutionPhase state=TaskExecutionPhase::accepting;StableId task_id,definition_id;std::optional<Integer> accepted_tick,objective_completed_tick,reported_tick;};
enum class TaskContractPhase {accepted,active,settling,settled};
struct TaskContractSummary {TaskContractPhase state=TaskContractPhase::accepted;bool execution_stopped=false;StableId stop_reason,item_id,unit_id;Integer agreed_quantity=0,delivered_quantity=0,eligible_quantity=0,paid_reward=0,refunded_budget=0,refunded_reward=0;};
enum class PersonalAppointmentPhase {accepted,meeting,fulfilled,cancelled,missed,interrupted};
struct PersonalAppointmentSummary {PersonalAppointmentPhase state=PersonalAppointmentPhase::accepted;StableId actor_id,provider_actor_id,place_id;Integer earliest_tick=0,latest_tick=0,duration_ticks=0;SourceRef accepted_source;std::optional<SourceRef> start_source,terminal_source;};
enum class VisitAppointmentMeetingPhase {not_started,meeting,fulfilled,cancelled,missed,interrupted};
struct VisitAppointmentSummary {SourceRef offered_source;std::optional<SourceRef> collected_source,accepted_source,meeting_source,terminal_source;StableId inviter_actor_id,invitee_actor_id,place_id;Integer accept_before_tick=0,earliest_tick=0,latest_tick=0,duration_ticks=0;VisitAppointmentMeetingPhase meeting=VisitAppointmentMeetingPhase::not_started;};
enum class ShopOrderCommercePhase {pending_order,accepted,delivered,cancelled,expired};
enum class ShopOrderOutcome {none,delivered,cancelled};
struct ShopOrderSummary {TaskExecutionPhase task_state=TaskExecutionPhase::active;ShopOrderCommercePhase commerce_state=ShopOrderCommercePhase::pending_order;ShopOrderOutcome outcome=ShopOrderOutcome::none;StableId order_id;SourceRef acceptance_source,issue_source;std::optional<SourceRef> settlement_source;Integer quantity=0,delivered_quantity=0,paid_price=0,refunded_price=0;};
struct StateNeedTaskSummary {StateTaskContractPhase phase=StateTaskContractPhase::active;StateNeedRef need_ref;StateGoalRef goal_ref;std::optional<StateClaimSettlementCause> closing_cause;bool report_required=false,report_received=false;std::optional<SourceRef> closing_source,settlement_source;};
using RegisteredLifecyclePayload=std::variant<TaskExecutionSummary,TaskContractSummary,PersonalAppointmentSummary,VisitAppointmentSummary,ShopOrderSummary,StateNeedTaskSummary>;
struct CommitmentLifecycleSummary {CommitmentRef ref;WorldIdentity source_identity;std::optional<Integer> owner_revision;std::vector<SubjectRef> disclosed_parties;RegisteredLifecyclePayload lifecycle;std::vector<CommitmentDeadline> disclosed_deadlines;CommitmentOperationGate cancellation;CommitmentSettlementGate settlement;};
struct KnowledgeStamp {SubjectRef observer;std::optional<Integer> observed_tick;Integer learned_tick=0;SourceRef source_ref;ReportReliability reliability=ReportReliability::witnessed;KnowledgeScope disclosure_scope=KnowledgeScope::self_known;std::optional<Integer> valid_until_tick;};
struct KnownCommitmentEntry {KnowledgeState state=KnowledgeState::unknown;CommitmentRef requested_ref;std::optional<CommitmentLifecycleSummary> summary;std::optional<KnowledgeStamp> knowledge;};
// Provider receives trusted read-slice authority at construction. It must reject a stale slice before touching its original owner.
class CommitmentRuleRead {public:virtual ~CommitmentRuleRead()=default;virtual Outcome<CommitmentLifecycleSummary> summary(const CommitmentRef&,const ReadStamp&)const=0;};
// Known sources reconstruct the disclosed historical slice. Never call the current internal reader then redact it.
class CommitmentKnowledgeSourceRead {public:virtual ~CommitmentKnowledgeSourceRead()=default;virtual Status authorize(const KnowledgeQueryContext&)const=0;virtual bool supports(const StableId&domain,const StableId&kind)const=0;virtual Outcome<KnownCommitmentEntry> known_snapshot(const CommitmentRef&,const KnowledgeQueryContext&)const=0;};
class CommitmentRead {const CommitmentKnowledgeSourceRead&sources_;public:explicit CommitmentRead(const CommitmentKnowledgeSourceRead&s):sources_(s){}Outcome<KnownCommitmentEntry> known_summary(const CommitmentRef&,const KnowledgeQueryContext&)const;};
Status validate_commitment_summary(const CommitmentLifecycleSummary&);
}
