// STATE-TASK-1 的真实照护贡献只读接缝；不复制 Care 表，也不凭受领发奖。
#pragma once
#include "resident/domains/family/care_coverage.hpp"
#include "state_need_task.hpp"
namespace hanmo::resident {
struct TaskState;
struct AffairsState;
// 返回的地址均借原同世界根；外层源租约必须活到原 Task 采用完成。
struct StateTaskCareContributionRead {
  const StateNeedTaskContract *contract = nullptr;
  const CareCoverageSatisfied *satisfaction = nullptr;
  const CareBinding *binding = nullptr;
  const AcceptedCaregiver *accepted = nullptr;
  const CareHandoverExecution *execution = nullptr;
  const CareHandoverExecutionBoundary1 *started = nullptr;
  const CareTemporalEvent *completed = nullptr;
};
// 原 Care 开始 writer 用这条唯一 active claim；没有认领正常返回空，不能
// 任选同人的另一合同。返回原受领地址供其真实 authority/obligation 字段采用。
Outcome<const StateNeedTaskContract *> read_state_task_care_start_contract(
    const CommonStore &, const TaskState &, const AffairsState &,
    const CareAcceptanceContext &, const AcceptedCaregiver &,
    const WorldMoment &actual_start);
// 尚未完成的执行也可核原 claim。缺原头/开始链是错误，不作为“其它任务”。
// 费用、停止与满意贡献共用此正文，不各自复制归属判断。
Outcome<bool> care_execution_belongs_to_state_task(
    const CommonStore &, const TaskState &, const StateNeedTaskContract &,
    const CareHandoverExecution &, const AcceptedCaregiver &);
// 缺源或错根报错；别人或不属于本 claim 的照护返回无贡献，由原结算走
// resolved_elsewhere。真实 M 取 TemporalEvent，绝不把 effective_tick 补成 M。
Outcome<std::optional<StateTaskCareContributionRead>>
read_state_task_care_contribution(const CommonStore &, const TaskState &,
                                  const AffairsState &,
                                  const CareCoverageSources &,
                                  const StateNeedTaskContract &,
                                  const CareGoalSatisfaction &);
} // namespace hanmo::resident
