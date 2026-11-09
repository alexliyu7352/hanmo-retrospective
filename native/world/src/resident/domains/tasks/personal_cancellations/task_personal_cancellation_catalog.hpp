// 私人发信人离开原岗位后的具名销约许可；只绑定已选Q01原委托，不能借此办理公约、采购或回书。
#pragma once
#include "resident/domains/tasks/personal_messages/task_personal_message_catalog.hpp"
namespace hanmo::resident {
inline constexpr const char* task_personal_cancellation_module="task_personal_message_cancellations";
inline constexpr const char* task_personal_cancellation_capability="task_personal_message_cancellations_v1";
inline constexpr const char* task_personal_cancellation_profile="known_private_sender_cancellation_v1";
inline constexpr const char* task_personal_cancellation_command="task_personal_message_cancel";
struct TaskPersonalCancellationPolicy {StableId id,offer_id,personal_template_id,sender_say_node_id;};
struct TaskPersonalCancellationCatalog {Integer version=1;std::vector<TaskPersonalCancellationPolicy> policies;};
const TaskPersonalCancellationPolicy* task_personal_cancellation_policy(const TaskPersonalCancellationCatalog&,const StableId& offer_id);
// 原Task、Q01和Known目录先完整通过；此门只将原issuer本人销约SAY绑定为销约接收者，不赋予岗位权限。
Status validate_task_personal_cancellation_catalog(const TaskPersonalCancellationCatalog&,const TaskCatalog&,const TaskPersonalMessageCatalog&,const KnownPersonalDialogueCatalog&,const InteractionCatalog&);
Outcome<TaskPersonalCancellationCatalog> decode_task_personal_cancellation_catalog(const BoundaryValue&,const TaskCatalog&,const TaskPersonalMessageCatalog&,const KnownPersonalDialogueCatalog&,const InteractionCatalog&);
BoundaryValue encode_task_personal_cancellation_catalog(const TaskPersonalCancellationCatalog&);
}
