// 使者任务只保存受领责任与履约来源；外交条款、代表权限和政治答复仍由政治域持有。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 完成只指使者按委任完成递交和所需回报，不代表目标组织接受提议或条约已经生效。
enum class EnvoyTaskResult { completed,cancelled,failed };
// 只记已停止交接的历史快照，不驱动新的工时或任务状态；原Common Action释放后仍可核已做事实。
struct EnvoyHandoverInterruption {
    StableId fact_id,task_id,contract_id,acceptance_fact_id,action_id,action_kind;
    StableId provider_actor_id,place_id,reason;
    Integer started_tick=0,stopped_tick=0,completed_work_ticks=0;
    std::optional<StableId> cause_fact_id;
};
// 受领引用独立mission_authority事实；普通本人亲递提议不能冒充一份任务批准。
struct EnvoyTaskAcceptance {
    StableId fact_id,task_id,contract_id,mission_authority_fact_id;
    StableId proposal_id,proposal_fact_id;
    Integer terms_revision=0;
    StableId issuer_organization_id,target_organization_id,envoy_actor_id;
    StableId target_office_id,delivery_site_id,report_office_id,report_site_id;
    StableId continuity_policy_id,display_name;
    Integer accepted_revision=0,accepted_tick=0;
    // 来自政治批准的绝对失效边界；完成tick必须严格小于此值，不能套旧采购的含截止宽限。
    Integer delivery_expires_tick=0;
    // 是否返报由明确作者委任决定；报告地点固定为原Office/site，不追踪原发令者实时位置。
    bool return_report_required=false;
};
// 政治域唯一presentation事实证明真实代表收到同一提议版本；任务仅引用，不复制政治送达账。
struct EnvoyTaskDelivery {
    StableId fact_id,task_id,contract_id,action_id,acceptance_fact_id;
    StableId diplomatic_presentation_fact_id;
    Integer completed_tick=0;
};
// 返报或取消留下唯一任务终结来源；已有实际外交递交不能因后来取消任务被删除。
struct EnvoyTaskCompletion {
    StableId fact_id,task_id,contract_id,acceptance_fact_id;
    std::optional<StableId> delivery_fact_id,report_action_id;
    EnvoyTaskResult result=EnvoyTaskResult::completed;
    Integer completed_tick=0;
    StableId reason;
    // 仅v2编码：历史返报必须引用政治域唯一报告收据，不能以旧ActionID独立证明接收。
    std::optional<StableId> diplomatic_report_fact_id;
};
// 状态只从同ID的唯一TaskRecord投影，不在sidecar保存另一份status或remaining_ticks。
struct EnvoyTaskSource {
    EnvoyTaskAcceptance acceptance;
    StableId task_status;
    std::optional<EnvoyTaskDelivery> delivery;
    std::optional<EnvoyTaskCompletion> completion;
};
// 消费者按稳定ID读取同候选事实；接触、递交与政治同意三种结果不可互相代替。
class TaskEnvoyRead {
public:
    virtual ~TaskEnvoyRead()=default;
    virtual Outcome<EnvoyTaskSource> envoy_task_source(const StableId& task_id)const=0;
    virtual Outcome<EnvoyTaskDelivery> envoy_task_delivery(const StableId& fact_id)const=0;
    virtual Outcome<EnvoyTaskCompletion> envoy_task_completion(const StableId& fact_id)const=0;
};
}
