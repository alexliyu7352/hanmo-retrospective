// 私人发信人离开原岗位后的具名受领许可；只绑定已选Q01原委托，不能借此办理公约、采购或回书；所列Offer全部受领必须有此源。
#pragma once
#include "resident/domains/tasks/personal_messages/task_personal_message_catalog.hpp"
namespace hanmo::resident {
inline constexpr const char* task_personal_acceptance_module="task_personal_message_acceptances";
inline constexpr const char* task_personal_acceptance_capability="task_personal_message_acceptances_v1";
inline constexpr const char* task_personal_acceptance_profile="known_private_sender_acceptance_v1";
inline constexpr const char* task_personal_acceptance_command="task_personal_message_accept";
struct TaskPersonalAcceptancePolicy {StableId id,offer_id,personal_template_id,sender_say_node_id;};
struct TaskPersonalAcceptanceCatalog {Integer version=1;std::vector<TaskPersonalAcceptancePolicy> policies;};
const TaskPersonalAcceptancePolicy* task_personal_acceptance_policy(const TaskPersonalAcceptanceCatalog&,const StableId& offer_id);
// 原Task、Q01和Known目录先完整通过；此门只将原issuer本人委托SAY绑定为原发信人，不赋予岗位权限。
Status validate_task_personal_acceptance_catalog(const TaskPersonalAcceptanceCatalog&,const TaskCatalog&,const TaskPersonalMessageCatalog&,const KnownPersonalDialogueCatalog&,const InteractionCatalog&);
Outcome<TaskPersonalAcceptanceCatalog> decode_task_personal_acceptance_catalog(const BoundaryValue&,const TaskCatalog&,const TaskPersonalMessageCatalog&,const KnownPersonalDialogueCatalog&,const InteractionCatalog&);
BoundaryValue encode_task_personal_acceptance_catalog(const TaskPersonalAcceptanceCatalog&);
}
