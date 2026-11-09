// TI11原批准/后继批准的F执行读；S3独占原执行行和S0 elapsed。
#pragma once
#include "governance_supervision_scope.hpp"
#include "resident/domains/tasks/work_execution/work_supervision_execution_terms.hpp"
namespace hanmo::resident {
// 仅借历史与连续性；S3真实源在调用栈传入，不把循环依赖存成裸指针或共享provider。
class NativeGovernanceSupervisionExecutionApprovalRead final:public SupervisionExecutionApprovalRead {
 const GovernanceWorkHistoryContext& history_;
 const GovernanceSupervisionContinuityRead& continuity_;
public:
 explicit NativeGovernanceSupervisionExecutionApprovalRead(
  const GovernanceWorkHistoryContext& history,const GovernanceSupervisionContinuityRead& continuity)
  :history_(history),continuity_(continuity){}
 bool bound_to(const CommonStore&,const TaskState&)const noexcept override;
 Outcome<ApprovedSupervisionExecution> read_current_supervision_execution(
  const WorkEpisode&,Integer next_execution_ordinal,const WorldMoment& actual_now,const SupervisionSourceContext&)const override;
 Outcome<ApprovedSupervisionExecution> read_original_supervision_execution(
  const WorkEpisode&,Integer execution_ordinal,const MomentOwnerSource& actual_action_start,
  const WorldMoment& started_at,const SupervisionSourceContext&)const override;
};
}
