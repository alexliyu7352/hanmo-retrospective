// The original S3 Common Action owner. No elapsed clock or second work total.
#pragma once
#include "work_task_sources.hpp"
#include "work_supervision_execution_terms.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"
#include "resident/kernel/action_time_validation.hpp"
namespace hanmo::resident {
struct SupervisionStartInputs {StableId actor_id;SourceRef accepted_presence_source;ActorConditionHead condition_at_start;std::vector<MomentOwnerSource> actual_sources;};
// These are original authority/presence/capability readers, not command payloads.
// Historical start validation uses the retained source cut, never today's actor.
class SupervisionEligibilityRead {
public:virtual ~SupervisionEligibilityRead()=default;
 virtual Outcome<SupervisionStartInputs> read_current_start_inputs(const WorkEpisode&,const WorldMoment&)const=0;
 virtual Status validate_historical_start_inputs(const WorkEpisode&,const SupervisionActionStartSource&)const=0;
 virtual Status validate_actual_service_span(const WorkEpisode&,const SupervisionActionStartSource&,const MomentSpan&)const=0;
 virtual Status validate_actual_release(const WorkEpisode&,const SupervisionActionStartSource&,SupervisionReleaseKind,const MomentOwnerSource&,const WorldMoment&,const WorkOriginalRead&)const=0;
};
struct WorkReassignmentSourceContext;
struct SupervisionSourceContext {
 const CommonStore& common;const TaskState& tasks;const WorkTaskRead& definitions;
 const SupervisionEligibilityRead& eligibility;const WorkOriginalRead& original;const CurrentMomentRead& clock;
 const SupervisionExecutionApprovalRead& approvals;const ActionElapsedStore& elapsed;const TimeScaleRef& scale;
 // /6 必须指向同一 owner 的真实接受根；旧版不补造此源。
 const WorkReassignmentSourceContext* reassignments=nullptr;
};
// 注册采用实际物理选择；/5 与 /6 不能共用伪造的 profile_version。
std::vector<ActionTimeRegistration> supervision_action_time_registrations(Integer selected_version);
// 原事实前缀关联叶不回调资格/生命周期；成功态仍不代替原完整来源验证。
enum class WorkExecutionBoundaryKind {unresolved,never_started,active,ended};
struct WorkExecutionBoundaryBeforeCut {
 WorkExecutionBoundaryKind state=WorkExecutionBoundaryKind::unresolved;
 const FactRecord* execution_header=nullptr;
 const FactRecord* terminal_header=nullptr;
 const WorldMoment* ended_at=nullptr;
};
Outcome<WorkExecutionBoundaryBeforeCut> read_supervision_execution_boundary_before_cut(
 const CommonStore&,const TaskState&,const ActionElapsedStore&,const TimeScaleRef&,
 const WorkEpisode&,const SourceRef* actual_cut);
Outcome<WorldMoment> read_supervision_execution_until(const SupervisionSourceContext&,const SupervisionActionStartSource&);
Outcome<const SupervisionExecutionTerms*> read_supervision_execution_terms(const SupervisionSourceContext&,const StableId& action_id);
bool supervision_action_kind(const StableId&)noexcept;
bool supervision_source_kind(const StableId&)noexcept;
Outcome<const SupervisionActionStartSource*> read_supervision_action_start(const SupervisionSourceContext&,const StableId& source_id);
Outcome<WorkExecutionSourceRead> read_supervision_work_execution(const SupervisionSourceContext&,const WorkEpisode&,const MomentOwnerSource&);
Outcome<WorldMoment> read_supervision_source_moment(const SupervisionSourceContext&,const SourceRef&,Integer schema);
Status validate_supervision_sources(const SupervisionSourceContext&);
// Only the authenticated accepted executor can start. The F clock subsequently
// consumes actual eligible supervisor spans. WorkActualStart and measured Work
// still require their original applied S5/S7 service evidence; adoption grants none.
// Only the original read-only prefix; retained qualification/F adoption still run in the candidate writer.
Status validate_supervision_start_request(const SupervisionSourceContext&,const StableId& work_id,const StableId& authenticated_actor);
Outcome<SourceRef> begin_supervision_action_in_candidate(CommonStore&,TaskState&,ActionElapsedStore&,const SupervisionSourceContext&,const StableId& work_id,const StableId& authenticated_actor);
// H 实际服务到额且原目标未足：同候选写真终局、释放 Action，保 Work.paused。
// 所有本执行真实到期 WorkPeriod 先消费；不终止 Task、不退款或自动续批。
Outcome<SourceRef> close_supervision_window_in_candidate(CommonStore&,TaskState&,ActionElapsedStore&,const SupervisionSourceContext&,const StableId& action_id);
Outcome<SourceRef> release_supervision_action_in_candidate(CommonStore&,TaskState&,ActionElapsedStore&,const SupervisionSourceContext&,const StableId& action_id,SupervisionReleaseKind,const MomentOwnerSource& actual_cause);
Status refresh_supervision_remaining_in_candidate(CommonStore&,const SupervisionSourceContext&);
class SupervisionActionTimeRead final:public ActionTimeOriginalSourceRead {
 const SupervisionSourceContext& context_;
public:explicit SupervisionActionTimeRead(const SupervisionSourceContext&x):context_(x){}
 Status enumerate_action_time_owners(const ActionTimeRegistration&,ActionTimeOwnerIdSink&)const override;
 Outcome<ActionTimeOwnerProof> read_action_time_owner(const ActionTimeRegistration&,const StableId&)const override;
 Outcome<WorldMoment> read_action_time_source(const ElapsedTimeSource&)const override;
 Status validate_fixed_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const ActionElapsedState&,const std::vector<FixedServiceTransition>&)const override;
 Status validate_shared_action_sources(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const SharedWorkElapsedState&)const override;
 Outcome<RunStop> read_action_continuation_stop(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<OriginalActionRemaining> read_original_action_remaining(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<ExhaustedAuthorizationWindow> read_exhausted_authorization_window(const ActionTimeRegistration&,const ActionTimeOwnerProof&,const WorldMoment&)const override;
 Outcome<StableId> read_gate_result_id(const SharedWorkGateResult&)const override;
 Outcome<SharedWorkGateResult> read_original_gate_result(const StableId&)const override;
 Status validate_original_gate_result(const SharedWorkGateResult&)const override;
};
}
