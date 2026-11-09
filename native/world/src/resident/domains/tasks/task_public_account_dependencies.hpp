// 公库接管前读取原任务责任引用；不批准承接，不改变原issuer或任何钱货归属。
#pragma once
#include "task_model.hpp"
namespace hanmo::resident {
struct GovernanceTaskState;struct EnvoyTaskState;struct MilitaryOccupationFact;
// 由真实军事接管清单提供；不能把玩家任意容器列表作为权限。
struct TaskPublicAccountSelection {
    Owner original_owner;
    std::vector<StableId> wallet_ids,inventory_ids;
};
enum class TaskAccountPurpose { funding,reward,payee,escrow,delivery,return_custody,shipment_source,shipment_cargo,shipment_destination };
struct TaskAccountReference {
    TaskAccountPurpose purpose=TaskAccountPurpose::funding;
    StableId container_id;
    bool selected=false;
};
// 托管批次仍读唯一Common库存；这里只引用它，不复制数量、预留或可花余额。
struct TaskCustodyReference {StableId task_id,inventory_id,lot_id;bool selected_container=false;};
struct TaskPublicAccountDependency {
    StableId responsibility_id;
    std::optional<StableId> contract_id;
    Owner original_issuer;
    SubjectRef assignee;
    std::optional<SubjectRef> delivery_beneficiary;
    std::vector<StableId> task_ids,action_ids,shipment_ids;
    std::vector<TaskAccountReference> wallets,inventories;
    std::vector<TaskCustodyReference> cargo;
    // 状态逐次取原Task/Contract，不生成另一份可写生命周期。
    std::vector<std::pair<StableId,StableId>> task_statuses;
    std::optional<ContractStatus> contract_status;
    std::optional<StableId> settlement_id;
    std::optional<bool> execution_stopped;
    StableId stop_reason;
    // 仅从真实Task/profile2与不可变静态目录派生；供政治域识别本轮具名现金适配，不是作者权力开关。
    bool static_cash_execution_supported=false;
};
struct TaskPublicAccountDependencies {
    WorldIdentity source_identity;
    std::vector<TaskPublicAccountDependency> responsibilities;
};
// 调用前须完整验证同一World；仅在资产Owner真正转移前枚举，包括已清偿的原历史责任。
// 治理/使者只有真实已注册sidecar匹配才能分流；nullptr只代表本World未启用该域。
// 治理资源依赖由S5实际relief条款枚举，不能把它造为普通采购预算。
Outcome<TaskPublicAccountDependencies> read_task_public_account_dependencies(
    const CommonStore&,const TaskState&,const TaskCatalog&,
    const GovernanceTaskState*,const EnvoyTaskState*,const TaskPublicAccountSelection&);
// 先由军事/政治域核准真实占领全链；本口只从原序号、冻结条款与原资产清单还原责任。
// 不读取今日Owner作为历史存在条件，不把当前货批或后来Task补入旧清单。
Outcome<TaskPublicAccountDependency> read_task_public_account_responsibility_history(
    const CommonStore&,const TaskState&,const TaskCatalog&,const MilitaryOccupationFact&,const StableId& responsibility_id,const DynamicStepInput* phase=nullptr);
// 完整原键集合用于发现政治登记漏项；没有创建来源的Shipment明确拒绝历史推断。
Outcome<std::vector<StableId>> read_task_public_account_responsibility_keys(
    const CommonStore&,const TaskState&,const TaskCatalog&,const GovernanceTaskState*,const EnvoyTaskState*,const MilitaryOccupationFact&,const DynamicStepInput* phase=nullptr);
}
