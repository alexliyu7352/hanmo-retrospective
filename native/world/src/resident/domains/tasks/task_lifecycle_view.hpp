// 当面办理只投影本人已知委托和原任务账；报价与提交仍由原命令网关负责。
#pragma once
#include "task_model.hpp"
namespace hanmo::resident {
struct TaskOfferResponseState;
struct GovernanceTaskState;struct EnvoyTaskState;class DiplomaticMissionRead;class DiplomacyPresentationRead;class DiplomaticEnvoyReportRead;
using TaskConversationPreview=std::function<Outcome<TaskQuote>(const TaskCommand&)>;
// 不接受调用方指定远方经办人；唯一来源是观察者当前say/choice会话。
Outcome<BoundaryValue> query_task_conversation(const CommonStore&,const TaskState&,const TaskCatalog&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& observer,
    Integer cursor,const TaskConversationPreview&,const TaskOfferResponseState* responses=nullptr);
// 显式登记治理后，仅其真实本人sidecar任务由治所/手记展示；普通委托仍保原未知来源拒绝。
Outcome<BoundaryValue> query_task_conversation_with_governance(const CommonStore&,const TaskState&,const TaskCatalog&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& observer,
    Integer cursor,const TaskConversationPreview&,const GovernanceTaskState&,bool registered,const TaskOfferResponseState* responses=nullptr);
// 仅已登记且真实本人外交使命由专属页展示；旧普通任务的未知来源仍严格拒绝。
Outcome<BoundaryValue> query_task_conversation_with_envoy(const CommonStore&,const TaskState&,const TaskCatalog&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId& observer,
    Integer cursor,const TaskConversationPreview&,const GovernanceTaskState* registered_governance,
    const EnvoyTaskState&,bool registered,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,
    const TaskOfferResponseState* responses=nullptr,const DiplomaticEnvoyReportRead* reports=nullptr);

}
