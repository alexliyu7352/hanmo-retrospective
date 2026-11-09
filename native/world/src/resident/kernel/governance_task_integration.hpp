// 治理阶段只适配原世界候选；行动释放与ActionCompletion仍由统一phase_plan办理。
#pragma once
#include "resident/domains/tasks/governance_task_step.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 原actions_step在通用减时之前调用；暂停保持原剩余，完成结果交中央统一释放动作。
Outcome<GovernanceTaskStep> step_governance_task_world(WorldBundle&,const StableId& action_id,const DynamicStepInput&);
// clock_commit之后、finish_dynamic_boundary之前调用一次；待结算不重开劳动。
Status settle_governance_task_world(WorldBundle&,const DynamicStepInput&);
}
