// S3 I15/24 + IC-V1a. One original Task Work owner; all service comes from S2/S5/S7.
#pragma once
#include "work_result_source.hpp"
#include "resident/domains/growth/growth_skill_qualification_v2.hpp"
#include "resident/domains/affairs/state_need.hpp"
#include "resident/domains/local_economy/economic_work_sources.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct TaskState;class CodecVisitor;class ProjectionWriter;class WorkSkillInputsRead;
inline bool task_work_execution_version(Integer value)noexcept{return value==3||value==4||value==5||value==6;}
inline bool task_work_skill_inputs_version(Integer value)noexcept{return value==4||value==5||value==6;}
// /6 显式承接 /5 的原有限执行条款；旧 /3 /4 不因空表获得 F 语义。
inline bool task_work_fixed_execution_terms_version(Integer value)noexcept{return value==5||value==6;}
// 新替任及终止来源只由实际选择的 /6 承载，不从缺省空历史推断。
inline bool task_work_lifecycle_sources_version(Integer value)noexcept{return value==6;}
enum class WorkPhase {active,paused,work_complete,stopped};
enum class S3WorkPeriodDisposition {credited,not_credited,interrupted};
enum class WorkPeriodReason {qualified,leader_absent,leader_incapacitated,insufficient_personnel,supply_missing,off_route,authority_paused,source_no_work,active_conflict,source_interrupted};
enum class WorkStopReason {cancelled,deadline_elapsed,leader_dead,principal_dissolved,authority_terminated,target_invalidated,unrecoverable_execution_failure};
enum class WorkResultDisposition {completed,partial_stopped,no_work_stopped};
struct S3WorkEpisodeRef {StableId work_id;};
// References resolve through the original S1 place/anchor catalogue or immutable
// location owner. These references are not caller-supplied coordinates.
struct PatrolCircuit {std::vector<SourceRef> checkpoint_refs;std::vector<SourceRef> allowed_passage_refs;};
struct StationWatch {SourceRef station_ref;};
struct PatrolPlan {
 StableId jurisdiction_id;ScopeRef authority_scope;StableId leader_actor_id;std::vector<StableId> unit_ids;
 std::variant<PatrolCircuit,StationWatch> route;Integer required_half_days=0,minimum_available_people=0;
 StateNeedDeadline completion_deadline;SourceRef supply_order_ref,expense_terms_ref,continuity_policy_ref;StableId rule_id,rule_version;
};
struct SupervisedWorkPlan {
 WorkKind kind=WorkKind::agronomy_supervision;StableId supervisor_actor_id;WorkRef economic_work_ref;
 Integer required_half_days=0;StateNeedDeadline completion_deadline;SourceRef target_site_or_anchor_ref;
 std::optional<SourceRef> required_skill_ref;SourceRef expense_terms_ref,continuity_policy_ref;StableId rule_id,rule_version;
};
using WorkPlan=std::variant<PatrolPlan,SupervisedWorkPlan>;
// Original approved service terms, excluding the separately accepted expense source.
// All consumers share this comparison; it does not prove authority or acceptance.
bool same_work_plan_terms(const WorkPlan&,const WorkPlan&);
struct WorkCommonActionBinding {StableId action_id;};
struct WorkMilitaryActivityBinding {SourceRef military_activity_ref;Integer source_schema=0;};
using WorkExecutionSource=std::variant<WorkCommonActionBinding,WorkMilitaryActivityBinding>;
struct WorkMilitaryPeriodSource {SourceRef fact_ref;Integer source_schema=2;};
struct WorkEconomicPeriodSource {WorkRef work_ref;MomentSpan window;std::vector<MomentOwnerSource> source_parts;};
using WorkPeriodSource=std::variant<WorkMilitaryPeriodSource,WorkEconomicPeriodSource>;
struct WorkStartRequest {WorkAuthorizationRef authorization;WorkPlan plan;};
struct WorkAcceptanceSource {
 StableId fact_id,work_id;WorkAuthorizationRef authorization;MomentOwnerSource original_task_acceptance;
 SourceRef expense_terms_ref;WorldMoment accepted_at;WorldMoment earliest_service_at;
};
struct WorkEpisode {
 StableId work_id,acceptance_source_ref;WorkAuthorizationRef authorization;WorkPlan frozen_plan;
 WorkPhase phase=WorkPhase::active;std::optional<WorkExecutionSource> current_execution;
 std::vector<StableId> accepted_period_refs,leader_change_fact_refs;std::vector<MomentOwnerSource> original_execution_sources;
 std::optional<StableId> last_interruption_ref,terminal_result_ref,actual_start_ref;
};
// Version 2 of the receipt carries a real full service window, not two floored ticks.
struct WorkPeriodReceipt2 {
 StableId fact_id,work_id;WorkKind kind=WorkKind::patrol;MomentSpan window;WorldMoment actual_until;
 StableId leader_actor_id;WorkPeriodSource source;WorkExecutionSource execution_source;
 std::vector<MomentOwnerSource> actual_anchor_sources;Integer credited_half_days=0;
 S3WorkPeriodDisposition disposition=S3WorkPeriodDisposition::not_credited;WorkPeriodReason reason=WorkPeriodReason::source_no_work;
 std::optional<MomentOwnerSource> reason_source;StableId rule_version;
};
struct WorkMilitaryStartBasis {WorkMilitaryPeriodSource period_fact_ref;};
struct WorkSupervisionStartBasis {StableId work_period_receipt_ref;std::vector<MomentOwnerSource> economic_period_refs;};
struct WorkActualStartSource {
 StableId start_fact_id,work_id;WorkAuthorizationRef authorization;WorkExecutionSource execution;
 std::variant<WorkMilitaryStartBasis,WorkSupervisionStartBasis> actual_basis;
 WorldMoment started_at;Integer source_schema=1;
};
struct WorkLeaderChange {
 StableId fact_id,work_id,old_actor_id,new_actor_id;MomentOwnerSource authority_source,actual_consent_source,actual_arrival_source;
 WorldMoment replacement_at;WorkExecutionSource new_execution_source;MomentOwnerSource original_execution_source;
};
// Physical /4 retains the actual before-head at the original result boundary.
// qualification is filled by the original K3 reader after that source exists.
struct WorkResultSkillInput {ActorConditionHead condition_before;StableId skill_id;std::optional<GrowthSkillQualificationV2> qualification;};
struct WorkResult {
 StableId fact_id,work_id,task_id;WorkAuthorizationRef authorization;WorkKind kind=WorkKind::patrol;
 StableId plan_rule_version;std::optional<WorldMoment> first_work_at;WorldMoment finished_at;
 Integer credited_half_days=0,required_half_days=0;
 std::vector<MomentOwnerSource> checkpoint_completion_refs,period_summary_refs,actual_leader_refs,encounter_result_refs,actual_stage_result_refs;
 std::vector<SourceRef> expense_receipt_refs;WorkResultDisposition disposition=WorkResultDisposition::no_work_stopped;
 std::optional<WorkStopReason> stop_reason;std::optional<MomentOwnerSource> actual_stop_source;
 std::optional<WorkResultSkillInput> skill_input;
};
// Actual Task proposal/acceptance sources. The TaskRecord remains the only current Task.
struct WorkTaskOfferSource {
 StableId fact_id;WorkProgramAuthorizationScope authorization;StableId executor_actor_id,presenting_actor_id;
 MomentOwnerSource presentation_contact;WorldMoment offered_at,valid_until;
};
struct WorkTaskAcceptanceSource {
 StableId fact_id,task_id;SourceRef offered_task_ref;StableId executor_actor_id;
 MomentOwnerSource acceptance_contact;WorldMoment accepted_at,earliest_service_at;std::optional<StableId> accepted_period_id;
};
// /6 的本人替任接受只绑定原 Work 和真实 S6 再委任；不产生第二个 Task 接受或费用。
struct WorkReassignmentAcceptanceSource {
 StableId fact_id,work_id;
 SourceRef reassignment_approval_ref,work_acceptance_ref;
 StableId new_actor_id;
 MomentOwnerSource acceptance_contact;
 WorldMoment accepted_at,earliest_service_at;
 std::optional<StableId> accepted_period_id;
};
// 原 Action 开始源。/3 /4 的 until_tick 字节保历史；/5 仅作为冻结
// goal deadline 投影，不提供 F 工期或 WorkActualStart 收费证据。
struct SupervisionActionStartSource {
 StableId fact_id,action_id,work_id,actor_id;SourceRef work_acceptance_ref;
 WorldMoment started_at;Integer until_tick=0;SourceRef accepted_presence_source;ActorConditionHead condition_at_start;std::vector<MomentOwnerSource> original_start_sources;
 std::optional<GrowthSkillQualificationV2> skill_qualification;
};
struct SupervisionWindowClosureSource {StableId fact_id,start_source_ref;WorldMoment closed_at;};
enum class SupervisionReleaseKind {work_completed,actual_interruption,actual_work_stop};
struct SupervisionActionTerminalSource {
 StableId fact_id,start_source_ref;WorldMoment ended_at;SupervisionReleaseKind disposition=SupervisionReleaseKind::actual_interruption;
 MomentOwnerSource actual_release_cause;
};
struct SupervisionExecutionTerms {
 StableId work_id;Integer execution_ordinal=0;WorkAuthorizationRef original_authorization_ref;
 MomentOwnerSource original_approved_execution_terms_ref;StableId action_id;
 WorkKind kind=WorkKind::agronomy_supervision;Integer service_limit_half_days=0;
 std::optional<MomentOwnerSource> actual_start_source_ref,actual_terminal_result_ref;
 // /6 仅首窗允许另行冻结真实本人接受；后序身份只沿实际 H 源。
 std::optional<SourceRef> initial_successor_acceptance_ref;
};
struct TaskWorkExecutionState {
 Integer version=3;OrderedTable<WorkTaskOfferSource> work_task_offers;OrderedTable<WorkTaskAcceptanceSource> work_task_acceptances;OrderedTable<WorkAcceptanceSource> acceptances;OrderedTable<WorkEpisode> episodes;
 OrderedTable<WorkPeriodReceipt2> period_receipts;OrderedTable<WorkActualStartSource> actual_starts;
 OrderedTable<WorkLeaderChange> leader_changes;OrderedTable<WorkResult> results;
 OrderedTable<SupervisionActionStartSource> supervision_starts;
 OrderedTable<SupervisionWindowClosureSource> supervision_window_closures;
 OrderedTable<SupervisionActionTerminalSource> supervision_terminals;
 // 显式 /5：保 /4 前缀，H 来自真实批准；旧 /3 /4 不猜迁。
 OrderedTable<SupervisionExecutionTerms> supervision_execution_terms;
 // /6 显式追加，旧物理版本不补造替任历史。
 OrderedTable<WorkReassignmentAcceptanceSource> work_reassignment_acceptances;
};
// Pure original-owner views. Source corruption is an error; genuine 0-work has
// its actual reason source. No callback may submit an attendance/credit number.
struct WorkAcceptedRead {WorkStartRequest frozen_request;MomentOwnerSource actual_task_acceptance;WorldMoment accepted_at,earliest_service_at;std::optional<SourceRef> actual_expense_terms;std::optional<WorkExecutionSource> actual_execution;};
struct WorkPeriodRead {
 WorkPeriodSource canonical_source;MomentSpan window;WorldMoment actual_until;StableId leader_actor_id;
 WorkExecutionSource execution_source;std::vector<MomentOwnerSource> actual_anchor_sources;
 bool source_provided=false;WorkPeriodReason reason=WorkPeriodReason::source_no_work;
 std::optional<MomentOwnerSource> reason_source;std::optional<WorldMoment> actual_positive_start;
 std::vector<MomentOwnerSource> encounter_result_refs,stage_result_refs;
};
struct WorkExecutionSourceRead {WorkExecutionSource binding;MomentOwnerSource original_source;StableId actual_leader_actor_id;WorldMoment started_at,finite_until;std::optional<MomentOwnerSource> original_terminal;std::optional<WorldMoment> ended_at;};
struct WorkCompletionRead {bool route_or_stage_complete=false;std::vector<MomentOwnerSource> checkpoint_completion_refs,actual_stage_result_refs,actual_leader_refs;};
struct WorkStopRead {WorkStopReason reason=WorkStopReason::unrecoverable_execution_failure;MomentOwnerSource actual_cause;bool execution_stopped=false;};
class WorkOriginalRead {
public:virtual ~WorkOriginalRead()=default;
 virtual const WorkSkillInputsRead* skill_inputs()const noexcept{return nullptr;}
 virtual Outcome<WorkAcceptedRead> read_accepted_work(const WorkAuthorizationRef&)const=0;
 virtual Status validate_current_start(const WorkAcceptedRead&,const WorldMoment&)const=0;
 virtual Outcome<WorkExecutionSourceRead> read_original_execution(const WorkEpisode&,const MomentOwnerSource&)const=0;
 virtual Outcome<WorkPeriodRead> read_original_period(const WorkEpisode&,const WorkPeriodSource&)const=0;
 virtual Outcome<WorkCompletionRead> read_completion(const WorkEpisode&,const std::vector<StableId>& actual_receipt_ids,const WorldMoment& through)const=0;
 virtual Outcome<WorkStopRead> read_actual_stop(const WorkEpisode&,const MomentOwnerSource&,const WorldMoment&)const=0;
 virtual Status validate_leader_change(const WorkEpisode&,const WorkLeaderChange&)const=0;
 virtual Status validate_original_source(const MomentOwnerSource&)const=0;
};
// This is the original S5 charge adapter, not an S3 payment writer. It consumes
// these real source records and returns only existing/new original fee receipts.
class WorkExpenseConsumer {
public:virtual ~WorkExpenseConsumer()=default;
 virtual Status consume_actual_start(const WorkActualStartSource&)=0;
 virtual Status consume_measured_work(const WorkPeriodReceipt2&)=0;
 virtual Outcome<std::vector<SourceRef>> consume_work_result(const WorkResult&)=0;
};
// Original completed-result tail. The consumer is the actual selected Growth
// writer and must preserve source-once adoption in the same outer candidate.
// It is never called by a result query, report delivery, or partial-stop writer.
class WorkCompletedResultConsumer {
public:virtual ~WorkCompletedResultConsumer()=default;
 virtual bool bound_to(const CommonStore&,const TaskState&)const noexcept=0;
 virtual Status consume_completed_result(const WorkResultSource&,const WorldMoment&completed_at)=0;
};
class WorkExpenseAcceptanceWrite {
public:virtual ~WorkExpenseAcceptanceWrite()=default;
 virtual Outcome<SourceRef> accept_work_expense(const StableId& work_id,const SourceRef& work_acceptance_source,const WorkAcceptedRead&,const WorldMoment&)=0;
 // 原 Task、Work 与 S6 接受绑定全部落入同一未发布候选后，才允许最终
 // 绑定预算责任。失败由外层撤销整份候选，不能放宽历史来源校验。
 virtual Status finalize_work_expense_acceptance(const WorkAcceptanceSource&)=0;
};
// Borrowed immutable assignment only. No copied plan, current Duty, periods or
// ActualStart is traversed; S2 must still prove its own authority/qualification.
struct WorkPatrolAssignmentRead {
 const WorkAcceptanceSource* acceptance=nullptr;
 const PatrolPlan* plan=nullptr;
 // 借真实初始 plan 或本人接替接受，不把原 plan 的旧 leader 改写成新身份。
 const StableId* actual_leader_actor_id=nullptr;
 const WorkReassignmentAcceptanceSource* successor_acceptance=nullptr;
};
struct WorkTaskSourceContext;class WorkTaskRead;class ExactResultOwnerRead;struct WorkReassignmentSourceContext;
// The original acceptance sources only: no later Work progress or Duty owner.
// All referents must belong to, and outlive reads in, this actual candidate.
struct WorkAcceptanceSourceContext {
 const WorkTaskSourceContext& tasks;const WorkTaskRead& definitions;
 const ExactResultOwnerRead& original_sources;
};
Outcome<const WorkAcceptanceSource*> read_work_acceptance_source(const WorkAcceptanceSourceContext&,const StableId&);
// Reassignment sources are supplied for this call, never captured in the three
// earlier construction roots. A replacement requires the full original reader.
Outcome<WorkPatrolAssignmentRead> read_work_patrol_assignment_source(const WorkAcceptanceSourceContext&,const SourceRef&,Integer source_schema,const WorkReassignmentSourceContext*);
class WorkExecutionRead {
 const CommonStore& common_;const TaskState& tasks_;const WorkOriginalRead& originals_;
public:WorkExecutionRead(const CommonStore&,const TaskState&,const WorkOriginalRead&);
 bool bound_to(const CommonStore& c,const TaskState& t)const noexcept{return &c==&common_&&&t==&tasks_;}
 Outcome<const WorkAcceptanceSource*> read_acceptance(const StableId&)const;
 Outcome<WorkPatrolAssignmentRead> read_patrol_assignment(const SourceRef& actual_work_acceptance,Integer source_schema)const;
 Outcome<const WorkEpisode*> read_work(const StableId&)const;
 Outcome<const WorkPeriodReceipt2*> read_period(const StableId&)const;
 Outcome<const WorkActualStartSource*> read_actual_start(const StableId&work,const StableId&start)const;
 // Only the S5 fee-evidence reader uses this acyclic original-result leg.
 Outcome<const WorkResult*> read_result_for_expense_evidence(const SourceRef&,Integer schema)const;
 Outcome<const WorkResult*> read_result(const SourceRef&,Integer schema)const;
};
Outcome<StableId> begin_work_in_candidate(CommonStore&,TaskState&,const WorkOriginalRead&,WorkExpenseAcceptanceWrite&,const WorkAuthorizationRef&,const WorldMoment&);
Status bind_original_work_execution_in_candidate(TaskState&,const WorkOriginalRead&,const StableId&work,const MomentOwnerSource&,const WorldMoment&);
Status close_original_work_execution_binding_in_candidate(TaskState&,const WorkOriginalRead&,const StableId&work,const MomentOwnerSource&actual_terminal,const WorldMoment&);
Outcome<StableId> consume_work_period_in_candidate(CommonStore&,TaskState&,const WorkOriginalRead&,WorkExpenseConsumer&,const StableId&work,const WorkPeriodSource&,const WorldMoment&);
Outcome<StableId> complete_work_if_ready_in_candidate(CommonStore&,TaskState&,const WorkOriginalRead&,WorkExpenseConsumer&,const StableId&work,const WorldMoment&,WorkCompletedResultConsumer* actual_completion_consumer=nullptr);
Outcome<StableId> stop_work_in_candidate(CommonStore&,TaskState&,const WorkOriginalRead&,WorkExpenseConsumer&,const StableId&work,const MomentOwnerSource&actual_stop,const WorldMoment&);
Outcome<StableId> replace_work_leader_in_candidate(CommonStore&,TaskState&,const WorkOriginalRead&,const StableId&work,WorkLeaderChange,const WorldMoment&);
Outcome<WorldMoment> read_work_source_moment(const CommonStore&,const TaskState&,const WorkOriginalRead&,const SourceRef&,Integer schema);
Status validate_task_work_execution(const CommonStore&,const TaskState&,const WorkOriginalRead&,const WorldMoment&);
Status visit_task_work_execution_checkpoint(CodecVisitor&,TaskWorkExecutionState&);
// Empty state only from the actual explicit physical selection.
Outcome<TaskWorkExecutionState> make_initial_task_work_execution(Integer selected_version);
bool owns_task_work_original_fact_kind(const StableId&)noexcept;
Status validate_task_work_original_fact_headers(const CommonStore&,const TaskWorkExecutionState*);
// Emits the borrowed original record fields, without a wrapper or duplicate
// Common header. false guarantees that the sink has not been written.
Outcome<bool> emit_task_work_original_fact_payload(ProjectionWriter&,const TaskWorkExecutionState*,const FactRecord&);
Outcome<TaskWorkExecutionState> decode_task_work_execution(const BoundaryValue&);
Outcome<BoundaryValue> encode_task_work_execution(const TaskWorkExecutionState&);
Status emit_task_work_execution_projection(ProjectionWriter&,std::string_view,const TaskWorkExecutionState&);
}
