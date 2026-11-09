// OE1 original evidence consumption. These rows are Task acceptance responsibility only.
#pragma once
#include <string_view>
#include "resident/model/objective_evidence.hpp"
#include "resident/domains/craft/craft_delivery_contract.hpp"
#include "resident/domains/affairs/state_need.hpp"
namespace hanmo::resident {
class WorkExecutionRead;struct TaskState;struct TaskExecutionServices;class CommonStore;class CodecVisitor;class ProjectionWriter;
struct ObjectiveQuantityGoal {Integer amount=0;StableId unit_id;};
struct ObjectivePredicateGoal {StableId source_domain,source_kind;};
using ObjectiveGoalMeasure=std::variant<ObjectiveQuantityGoal,ObjectivePredicateGoal>;
enum class ObjectiveUsePolicy {exclusive,distinct_purpose};
struct ObjectiveCraftBinding {SourceRef order_ref;StableId job_id,item_id;Owner from_owner,to_owner;StableId acceptor_actor_id;bool allow_partial=false;};
struct FrozenObjectiveBase {
 StableId id,task_id;SourceRef task_acceptance_source;TermsRevisionRef task_terms;
 ObjectiveBaseCriterion criterion;ObjectiveGoalMeasure goal;
 std::optional<StableId> required_participant;std::optional<SubjectRef> required_recipient;
 std::optional<ProfessionalTargetRef> professional_target;std::vector<ProfessionalOutcomeKind> allowed_professional_kinds;
 std::optional<TermsRevisionRef> required_source_terms;std::optional<ObjectiveCraftBinding> craft;
 bool allow_natural_event=false,allow_partial=false,record_contribution=true;
 ObjectiveUsePolicy use_policy=ObjectiveUsePolicy::exclusive;StableId acceptance_purpose;
 StateNeedDeadline completion_gate;bool report_required=false;SubjectRef report_recipient;
 std::optional<StateNeedDeadline> report_gate;ProfessionalObserverScope observer;
};
struct ObjectiveAcceptanceReceipt {
 StableId acceptance_id,task_id,objective_id;ObjectiveBaseEvidenceSource source;ObjectiveBaseCriterion criterion;
 std::optional<StableId> actual_participant;std::optional<SubjectRef> recipient;
 std::optional<ProfessionalTargetRef> professional_target;Integer completed_tick=0;ObjectiveAcceptedMeasure measure;
 bool accepted=false;std::optional<StableId> contribution_use_ref,source_settlement_key;
 StableId source_owner;WorldMoment completed_at,accepted_at;std::vector<SourceRef> retained_result_refs;
};
struct ObjectiveContributionUse {StableId id,acceptance_id,objective_id,task_id,source_owner,settlement_key,purpose;ObjectiveUsePolicy policy=ObjectiveUsePolicy::exclusive;ObjectiveAcceptedMeasure measure;};
struct ObjectiveReportReceipt {StableId id,task_id;SourceRef original_report_source;SubjectRef recipient;WorldMoment received_at;std::vector<StableId> accepted_objective_ids;};
// version 取外层真实物理选择，不增加旧 /1 的 wire 根。/2 才接受 Work tag。
struct TaskObjectiveBaseState {Integer version=1;OrderedTable<FrozenObjectiveBase> objectives;OrderedTable<ObjectiveAcceptanceReceipt> acceptances;OrderedTable<ObjectiveContributionUse> contribution_uses;OrderedTable<ObjectiveReportReceipt> reports;};
// Only an original accepted Task terms owner supplies these frozen objective terms.
class TaskObjectiveTermsRead {public:virtual ~TaskObjectiveTermsRead()=default;virtual Outcome<FrozenObjectiveBase> read_accepted_objective(const StableId&task_id,const StableId&objective_id)const=0;};
struct LegacyObjectiveEvidenceView {LegacyTaskEvidenceRef source;StableId task_id;LegacyGoalCriterion criterion;std::optional<StableId> actual_participant;std::optional<SubjectRef> recipient;ObjectiveAcceptedMeasure measure;WorldMoment completed_at;StableId source_owner,settlement_key;std::vector<SourceRef> original_result_refs;};
struct ObjectiveEvidenceSnapshot {std::optional<StableId> actual_participant;std::optional<SubjectRef> recipient;std::optional<ProfessionalTargetRef> target;ObjectiveAcceptedMeasure measure;WorldMoment completed_at;StableId source_owner,settlement_key;std::vector<SourceRef> original_result_refs;};
class TaskObjectiveEvidenceSources {
public:virtual ~TaskObjectiveEvidenceSources()=default;
 virtual Outcome<std::shared_ptr<const ReadStamp>> capture_original_evidence(const FrozenObjectiveBase&,const ObjectiveBaseEvidenceSource&)const=0;
 virtual Outcome<LegacyObjectiveEvidenceView> read_legacy(const FrozenObjectiveBase&,const LegacyTaskEvidenceRef&,const ReadStamp&)const=0;
 virtual const CraftDeliveryReceiptRead* craft_delivery_reader()const noexcept=0;
 virtual const ProfessionalOutcomeSourceRead* professional_reader(const StableId&source_domain)const noexcept=0;
 // 只返回同一候选的原 S3 Work reader；未选择 Work 时可空，选中路径明确拒缺。
 virtual const WorkExecutionRead* work_reader()const noexcept{return nullptr;}
 virtual Status validate_original_read_stamp(const ReadStamp&)const=0;
 // Historical retention validates the immutable original payload at acceptance, without
 // applying a later supersession/admission policy retroactively to a settled contract.
 virtual Outcome<ObjectiveEvidenceSnapshot> read_retained_acceptance(const FrozenObjectiveBase&,const ObjectiveAcceptanceReceipt&)const=0;
 // A real Task return/report owner checks message content, named recipient, arrival/knowledge
 // requirements and original task terms. Merely posting a message is never a receipt.
 virtual Outcome<ObjectiveReportReceipt> read_actual_report(const StableId&task_id,const SourceRef&)const=0;
};
// 真实 WorkResult 只投影劳动完成 predicate；不写目标效果、报酬或知情。
Outcome<ObjectiveEvidenceSnapshot> read_work_objective_evidence(const CommonStore&,const TaskState&,const WorkExecutionRead&,const FrozenObjectiveBase&,const WorkEvidenceSource&);
Outcome<StableId> freeze_task_objective_in_candidate(CommonStore&,TaskState&,const TaskObjectiveTermsRead&,const StableId&task_id,const StableId&objective_id,const WorldMoment&actual);
Outcome<StableId> accept_task_objective_evidence_in_candidate(CommonStore&,TaskState&,const TaskObjectiveEvidenceSources&,const StableId&objective_id,const ObjectiveBaseEvidenceSource&,const WorldMoment&actual);
Outcome<StableId> record_task_objective_report_in_candidate(CommonStore&,TaskState&,const TaskObjectiveEvidenceSources&,const StableId&task_id,const SourceRef&,const WorldMoment&actual);
// Pure clearance for the original Task settlement writer. No money or knowledge is changed here.
Status task_objective_settlement_ready(const TaskState&,const StableId&task_id);
Status settle_objective_task_contract_in_candidate(CommonStore&,TaskState&,const StableId&task_id,const WorldMoment&actual,const TaskExecutionServices*);
Status validate_task_objective_base_sources(const CommonStore&,const TaskState&,const TaskObjectiveTermsRead&,const TaskObjectiveEvidenceSources&,const WorldMoment&actual);
Status visit_task_objective_base_checkpoint(CodecVisitor&,TaskObjectiveBaseState&);
Outcome<TaskObjectiveBaseState> decode_task_objective_base(const BoundaryValue&,Integer selected_version=1);
Outcome<BoundaryValue> encode_task_objective_base(const TaskObjectiveBaseState&);
Status emit_task_objective_base_projection(ProjectionWriter&,std::string_view,const TaskObjectiveBaseState&);
}
