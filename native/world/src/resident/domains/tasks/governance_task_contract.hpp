// 治理任务跨域只读合同：S3持接受/劳动/最终事实，S5持资源收据，S6持唯一冻结结果计划。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 本版仅登记安民这一真实治理行为；其它事业必须增加明确typed能力，不能换名称冒充。
enum class GovernanceTaskKind { appease };
enum class GovernanceTaskResult { completed,cancelled,failed };
// 接受事实冻结权源与责任引用，不复制家庭资产、民心或可花预算；预留成功后才整体发布。
struct GovernanceTaskAcceptance {
    StableId fact_id,task_id,contract_id,authority_source_fact_id,organization_id,executor_actor_id;
    StableId jurisdiction_id,site_id,rule_id,rule_version,relief_terms_id,reservation_id,display_name;
    GovernanceTaskKind kind=GovernanceTaskKind::appease;
    Integer accepted_revision=0,accepted_tick=0,work_ticks=0,deadline_tick=0;
};
// 只证明唯一Common Action已经做满；这份事实尚不代表钱粮已分发或民心已增加。
struct GovernanceWorkCompletion {
    StableId fact_id,task_id,contract_id,action_id,acceptance_fact_id,outcome_plan_id;
    Integer started_tick=0,completed_tick=0;
};
// 只保未完劳动终止时的历史快照；不是第二倒计时，也不代表责任已经清算或获得完整成果。
struct GovernanceWorkInterruption {
    StableId fact_id,task_id,contract_id,action_id,acceptance_fact_id,reason;
    Integer stopped_tick=0,completed_work_ticks=0;
    std::optional<StableId> cause_fact_id;
};
// 最终事实只能引用同一任务/动作/工作源的实际资源收据和S6计划，不接受玩家自报成功。
struct GovernanceTaskCompletion {
    StableId fact_id,task_id,contract_id,action_id,acceptance_fact_id,work_completion_fact_id,outcome_plan_id;
    std::vector<StableId> resource_receipt_ids;
    GovernanceTaskResult result=GovernanceTaskResult::completed;
    Integer completed_tick=0;
    StableId reason;
};
// task_status每次从唯一TaskRecord投影；此DTO不进存档，也不是可独立变化的第二生命周期。
struct GovernanceTaskSource {
    GovernanceTaskAcceptance acceptance;
    StableId task_status;
    std::optional<GovernanceWorkCompletion> work;
    std::optional<GovernanceTaskCompletion> completion;
    std::optional<GovernanceWorkInterruption> interruption;
};
// 各消费者按真实稳定ID读取同候选；收到普通失败须保留原码，不能伪造空成功来源。
class TaskGovernanceRead {
public:
    virtual ~TaskGovernanceRead()=default;
    virtual Outcome<GovernanceTaskSource> governance_task_source(const StableId& task_id)const=0;
    // 精确原authority曾受领的唯一Task；没有实现的适配器必须明确Unavailable，不能冒空集合。
    virtual Outcome<std::optional<GovernanceTaskSource>> governance_task_for_authority(const StableId& authority_id)const;
    virtual Outcome<GovernanceWorkCompletion> governance_work_completion(const StableId& fact_id)const=0;
    virtual Outcome<GovernanceTaskCompletion> governance_task_completion(const StableId& fact_id)const=0;
};
// 结算的普通阻塞是成功的pending结果；异常/源不一致返回失败交整个世界候选回滚。
struct GovernanceSettlementAttempt {
    enum class Kind { pending,settled };
    Kind kind=Kind::pending;
    StableId code;
    std::vector<StableId> resource_receipt_ids;
};
}
