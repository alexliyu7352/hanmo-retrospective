// 治理只在原actions_step推进一次；政治连续性决定暂停/留责，行动结束仍由中央统一释放。
#pragma once
#include "governance_task_rules.hpp"
#include "resident/domains/politics/politics_governance_contract.hpp"
namespace hanmo::resident {
struct TaskExecutionServices;
// 此结果只属于本次阶段调用，不持久化另一个时钟或任务状态。
struct GovernanceTaskStep {
    bool suspended=false,action_finished=false,success=true;
    StableId task_id,code;
    std::optional<StableId> fact_id;
};
using GovernanceReleaseResources=std::function<Status(const TaskGovernanceRead&,const GovernanceTaskSource&)>;
Outcome<GovernanceTaskStep> step_governance_task_action(CommonStore&,TaskState&,GovernanceTaskState&,const GovernanceAuthorityRead&,const StableId& action_id,Integer to_tick,const GovernanceOutcomeFreeze&,const GovernanceReleaseResources&,const TaskExecutionServices* execution=nullptr);
}
