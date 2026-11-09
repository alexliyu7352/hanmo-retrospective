// 原批准、本人Task接受及Work冻结条款的历史闭包；不是当前继续服务许可。
#pragma once
#include "governance_program.hpp"
#include "resident/domains/tasks/work_execution/work_task_sources.hpp"
namespace hanmo::resident {
struct GovernanceAcceptedWorkHistory {
 const GovernanceProgramApproval* approval=nullptr;
 const GovernanceProgramSpec* specification=nullptr;
 const WorkTaskOfferSource* task_offer=nullptr;
 const WorkTaskAcceptanceSource* task_acceptance=nullptr;
 const WorkAcceptanceSource* work_acceptance=nullptr;
 const GovernanceExecutionBinding* offer_binding=nullptr;
 const GovernanceExecutionBinding* acceptance_binding=nullptr;
 // Exactly one view of the same original immutable WorkPlan; no copied plan.
 const SupervisedWorkPlan* supervision=nullptr;
 const PatrolPlan* patrol=nullptr;
};
// 只借本次同候选；不得跨clone缓存这些指针。历史查询不要求签发者今日仍任职。
struct GovernanceWorkHistoryContext {
 const GovernanceProgramReadContext& programs;
 const TaskState& tasks;
 const WorkTaskRead& task_definitions;
};
// 只投影原保留关联；完整权限/目标/预算仍由原外层读口验证。
struct GovernanceWorkExecutorIdentity {
 const StableId* actor_id=nullptr;
 const FactRecord* predecessor=nullptr;
 const WorkReassignmentAcceptanceSource* acceptance=nullptr;
};
Outcome<GovernanceWorkExecutorIdentity> read_governance_work_executor_before_cut(
 const GovernanceWorkHistoryContext&,const WorkEpisode&,const SourceRef* actual_cut);
Outcome<GovernanceWorkExecutorIdentity> read_governance_supervision_executor_identity_at(
 const GovernanceWorkHistoryContext&,const WorkEpisode&,const WorldMoment&,const SourceRef* actual_start_cut);
// 指定真实接受的身份关联核，供签发 H 前选择 executor；不授予完整接受/批准。
Outcome<GovernanceWorkExecutorIdentity> read_governance_accepted_work_executor_at(
 const GovernanceWorkHistoryContext&,const WorkEpisode&,const SourceRef& acceptance,const WorldMoment&,const SourceRef* actual_cut);
Outcome<const StableId*> read_governance_supervision_executor_at(
 const GovernanceWorkHistoryContext&,const WorkEpisode&,const WorldMoment&,const SourceRef* actual_start_cut);
Outcome<GovernanceAcceptedWorkHistory> read_governance_accepted_work_history(
 const GovernanceWorkHistoryContext&,const WorkEpisode&);
// null cut仅指真实当前开工预读；非null必须为同Work的真实已持久开工源。
Outcome<GovernanceAcceptedWorkHistory> read_governance_supervision_start_history(
 const GovernanceWorkHistoryContext&,const WorkEpisode&,const WorldMoment&,
 const SourceRef* actual_start_cut);
Status validate_governance_supervision_service_window(
 const GovernanceWorkHistoryContext&,const WorkEpisode&,
 const SupervisionActionStartSource&,const MomentSpan&);
}
