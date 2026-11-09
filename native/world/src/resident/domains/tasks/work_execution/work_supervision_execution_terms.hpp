// TIME-S3-1 §2.2：原有限监督执行的批准条款；不保存 elapsed 或目标验工量。
#pragma once
#include "work_execution_contract.hpp"
namespace hanmo::resident {
// 本值由原 S6 批准/已接受执行条款读者返回。H 必须确实写在原批准中，
// 不能从 WorkPlan.required_half_days、completion_deadline、余额或当前 M 推导。
struct ApprovedSupervisionExecution {
 WorkAuthorizationRef authorization;MomentOwnerSource original_terms_source;
 WorkKind kind=WorkKind::agronomy_supervision;Integer service_limit_half_days=0;
 // 借真实原计划或同 Work 替任批准的 actor；空值不是缺省执行者。
 const StableId* actual_executor_actor_id=nullptr;
 // 同步借用真实接受 row，绝不借指向临时 SourceRef 的指针。
 const WorkReassignmentAcceptanceSource* initial_successor_acceptance=nullptr;
};
// 仅在同步读调用中借用原执行上下文，核同候选及前序真实终局；不保存循环 provider。
struct SupervisionSourceContext;
class SupervisionExecutionApprovalRead {
public:virtual ~SupervisionExecutionApprovalRead()=default;
 virtual bool bound_to(const CommonStore&,const TaskState&)const noexcept=0;
 virtual Outcome<ApprovedSupervisionExecution> read_current_supervision_execution(
  const WorkEpisode&,Integer next_execution_ordinal,const WorldMoment& actual_now,
  const SupervisionSourceContext&)const=0;
 virtual Outcome<ApprovedSupervisionExecution> read_original_supervision_execution(
  const WorkEpisode&,Integer execution_ordinal,const MomentOwnerSource& actual_action_start,
  const WorldMoment& started_at,const SupervisionSourceContext&)const=0;
};
Status validate_supervision_execution_terms_shape(const SupervisionExecutionTerms&);
Status validate_approved_supervision_execution(const WorkEpisode&,const ApprovedSupervisionExecution&);
// 原 row 的单份字段表供完整 Work 段复用。这里没有独立 section/目录或 writer。
Outcome<SupervisionExecutionTerms> decode_supervision_execution_terms(const BoundaryValue&,Integer selected_version=5);
Outcome<BoundaryValue> encode_supervision_execution_terms(const SupervisionExecutionTerms&,Integer selected_version=5);
Status visit_supervision_execution_terms_checkpoint(CodecVisitor&,SupervisionExecutionTerms&,Integer selected_version=5);
Status emit_supervision_execution_terms_fields(ProjectionWriter&,const SupervisionExecutionTerms&,Integer selected_version=5);
// Global Common Action.params is a projection of this original /5 row. The
// section remains its only owning storage; unknown action kinds write nothing.
Outcome<bool> emit_task_work_original_action_payload(ProjectionWriter&,const CommonStore&,
 const TaskWorkExecutionState*,const ActionCoreView&);
Outcome<bool> validate_task_work_original_action_payload(const CommonStore&,
 const TaskWorkExecutionState*,const ActionCoreView&,const BoundaryValue& actual_params);
Status validate_task_work_original_action_headers(const CommonStore&,const TaskWorkExecutionState*);

}
