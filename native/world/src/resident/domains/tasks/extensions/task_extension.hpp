// 私人委托延期只从同一个原委托人的当前明确声明产生；公约和动态条款须独立权源。
#pragma once
#include "task_extension_state.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
namespace hanmo::resident {
struct TaskExtensionCommand {StableId task_id,policy_id;};
struct TaskPublicAccountExecutionState;struct TaskStaticAssetState;struct TaskMessageHandoffState;
// 已启用的原永久凭据必须同候选借入；延期不得插进已经开始/完成的交货或结酬区间。
struct TaskExtensionHistorySources {
    const TaskPublicAccountExecutionState* payments=nullptr;
    const TaskStaticAssetState* assets=nullptr;
    const TaskMessageHandoffState* messages=nullptr;
};
// 原报价冻结唯一增量与真实说话来源；commit仍重新读取原Task、原期限和当面资格。
struct TaskExtensionQuote {
    AuthoredProviderSourceProof consent;
    StableId policy_id,task_id,offer_id;
    Integer accepted_tick=0,original_deadline_tick=0,deadline_tick=0;
};
// 首版仅原Actor采购/送信私约、本人report_role和原say；护送/公约/动态责任需另接真实历史。
Status validate_task_extension_catalog(const TaskExtensionCatalog&,const TaskCatalog&,const InteractionCatalog&);
Outcome<TaskExtensionCatalog> decode_task_extension_catalog(const BoundaryValue&,const TaskCatalog&,const InteractionCatalog&);
BoundaryValue encode_task_extension_catalog(const TaskExtensionCatalog&);
// 未显式选择profile不新增模块；启用时必须完整声明目录和能力，不能由安装并集补齐。
Outcome<bool> decode_task_extension_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_task_extension_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const TaskExtensionCatalog*,const TaskExtensionState*);
// 全冷门按原目录+唯一同意事实重建每份受影响Task的deadline；未知原历史不能直接接受当前数值。
Status validate_task_extensions(const CommonStore&,const TaskState&,const TaskCatalog&,const InteractionCatalog&,const TaskExtensionCatalog&,const TaskExtensionState&,const TaskExtensionHistorySources&,const DynamicStepInput* phase=nullptr);
Outcome<TaskExtensionQuote> preview_task_extension(const CommonStore&,const TaskState&,const TaskCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const TaskExtensionCatalog&,const TaskExtensionState&,const TaskExtensionHistorySources&,const StableId& actor,const TaskExtensionCommand&);
// 同候选Common/完整Task保存点由外层拥有；只追加同意事实并改原deadline，不修改资金或货物。
Outcome<StableId> commit_task_extension_in_candidate(CommonStore&,TaskState&,const TaskCatalog&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const TaskExtensionCatalog&,TaskExtensionState&,const TaskExtensionHistorySources&,const TaskExtensionQuote&);
}
