// 原 Work 生命周期源只借真实同候选根，不回调完整 Work/start/skill 证明自身。
#pragma once
#include "governance_service_continuity.hpp"
#include "resident/domains/tasks/work_execution/work_patrol_sources.hpp"
namespace hanmo::resident {
struct GovernanceWorkLifecycleSourceContext {
 const GovernanceWorkHistoryContext& history;
 const GovernanceServiceContinuityContext& continuity;
 const ActionElapsedStore& elapsed;
 const TimeScaleRef& scale;
 const PatrolWorkSources& patrol;
};
Status validate_governance_work_lifecycle_bindings(const GovernanceWorkLifecycleSourceContext&,const ReadStamp&);
Status validate_governance_work_reassignment_acceptance_boundary(const GovernanceWorkLifecycleSourceContext&,const WorkEpisode&,const WorldMoment&,const SourceRef* actual_acceptance_cut,const ReadStamp&);
Status validate_governance_work_reassignment_request(const GovernanceWorkLifecycleSourceContext&,const ApproveGovernanceWorkReassignment&,const ReadStamp&);
Outcome<SourceRef> approve_governance_work_reassignment_in_candidate(CommonStore&,OrganizationRuntime&,GovernanceProgramState&,const GovernanceWorkLifecycleSourceContext&,const ApproveGovernanceWorkReassignment&,const ReadStamp&);
Outcome<const GovernanceWorkReassignmentApproval*> read_governance_work_reassignment(const GovernanceWorkLifecycleSourceContext&,const SourceRef&,const ReadStamp&);
Outcome<SourceRef> record_governance_work_deadline_termination_in_candidate(CommonStore&,GovernanceProgramState&,const GovernanceWorkLifecycleSourceContext&,const StableId& work_id,const ReadStamp&);
Outcome<const GovernanceWorkDeadlineTermination*> read_governance_work_deadline_termination(const GovernanceWorkLifecycleSourceContext&,const SourceRef&,const ReadStamp&);
Status validate_governance_work_lifecycle_sources(const GovernanceWorkLifecycleSourceContext&,const ReadStamp&);
}
