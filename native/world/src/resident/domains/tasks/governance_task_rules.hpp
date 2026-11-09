// 治理规则只调用同候选具名领域回调；普通待结算保劳动，硬错误由世界事务整体回滚。
#pragma once
#include "governance_task_state.hpp"
#include "task_model.hpp"
namespace hanmo::resident {
class PublicReliefResultRead;
// 适配器借用唯一TaskRecord和本域事实，不能从任意存在的公共fact推导治理成功。
class GovernanceTaskRead final:public TaskGovernanceRead {
public:
    GovernanceTaskRead(const CommonStore&,const TaskState&,const GovernanceTaskState&);
    Outcome<GovernanceTaskSource> governance_task_source(const StableId&)const override;
    Outcome<std::optional<GovernanceTaskSource>> governance_task_for_authority(const StableId&)const override;
    Outcome<GovernanceWorkCompletion> governance_work_completion(const StableId&)const override;
    Outcome<GovernanceTaskCompletion> governance_task_completion(const StableId&)const override;
private:
    const CommonStore& common_;const TaskState& tasks_;const GovernanceTaskState& governance_;
};
// 工期是原Common Action减至零的结果；冻结S6计划不复制其礼学/民心数值。
using GovernanceOutcomeFreeze=std::function<Outcome<StableId>(const TaskGovernanceRead&,const GovernanceWorkCompletion&)>;
Outcome<GovernanceWorkCompletion> finish_governance_work(CommonStore&,TaskState&,GovernanceTaskState&,const StableId& action_id,Integer to_tick,const GovernanceOutcomeFreeze&);
// 这一个协调口只能在clock_commit后调用；S5普通阻塞以pending返回，拒绝吞掉硬错误。
struct GovernanceSettlementServices {
    std::function<Outcome<GovernanceSettlementAttempt>(const TaskGovernanceRead&,const GovernanceTaskSource&)> distribute;
    const PublicReliefResultRead* receipts=nullptr;
    std::function<Status(const TaskGovernanceRead&,const GovernanceTaskCompletion&)> apply_outcome;
};
Status settle_ready_governance_tasks(CommonStore&,TaskState&,GovernanceTaskState&,const GovernanceSettlementServices&);
// 取消只释放本契约剩余预留及原Common Action，已完成劳动事实保留，不应用政治增益。
struct GovernanceCancellationServices {
    std::function<Status(const TaskGovernanceRead&,const GovernanceTaskSource&)> release_resources;
    std::function<Status(const StableId& action_id)> release_action;
};
// 原取消资格的纯读报价；不释放预留/动作，尚未clock_commit的工作完成源不能提前清算。
Outcome<GovernanceTaskSource> preview_governance_cancellation(const CommonStore&,const TaskState&,const GovernanceTaskState&,const StableId& actor_id,const StableId& task_id);
// 仅governance_cancel可借此通过公共busy门；具体目标仍须由原取消预览再次匹配。
bool governance_cancellation_busy_allowed(const CommonStore&,const TaskState&,const GovernanceTaskState&,const StableId& actor_id);
Outcome<GovernanceTaskCompletion> cancel_governance_task(CommonStore&,TaskState&,GovernanceTaskState&,const StableId& actor_id,const StableId& task_id,const GovernanceCancellationServices&);
// 原contract预留生命周期读取真实任务active/ready，不要求建立虚假的可花采购预算钱包。
bool governance_reservation_exists(const TaskState&,const GovernanceTaskState&,const StableId& contract_id);
Status validate_governance_tasks(const CommonStore&,const TaskState&,const GovernanceTaskState&);
// 仅中央已登记治理模块可委派真实sidecar同ID记录；其它任务继续原定义/钱包/行动校验。
Status validate_tasks_with_governance(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const GovernanceTaskState&,bool registered,const TaskBattleRead& battles={},const PoliticalTakeoverRead* authority=nullptr);
}
