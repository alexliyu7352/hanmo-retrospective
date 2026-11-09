// Q01原私人送信：外出目标只在原Known私人会话明确接信，不继承任何岗位服务。
#pragma once
#include "resident/domains/tasks/task_model.hpp"
#include "resident/domains/interaction/personal_dialogues/known_personal_dialogue_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* task_personal_message_module="task_personal_message_deliveries";
inline constexpr const char* task_personal_message_capability="task_personal_message_deliveries_v1";
inline constexpr const char* task_personal_message_profile="known_private_recipient_v1";
inline constexpr const char* task_personal_message_command="task_personal_message_deliver";
inline constexpr const char* task_personal_message_source_kind="personal_message_delivery";
struct TaskPersonalMessagePolicy {StableId id,offer_id,personal_template_id,recipient_say_node_id;};
struct TaskPersonalMessageCatalog {Integer version=1;std::vector<TaskPersonalMessagePolicy> policies;};
const TaskPersonalMessagePolicy* task_personal_message_policy(const TaskPersonalMessageCatalog&,const StableId& offer_id);
// 两个完整原目录是前置；本门额外要求原私人message、唯一静态真人收件者及其无岗位provider SAY。
Status validate_task_personal_message_catalog(const TaskPersonalMessageCatalog&,const TaskCatalog&,const KnownPersonalDialogueCatalog&,const InteractionCatalog&);
Outcome<TaskPersonalMessageCatalog> decode_task_personal_message_catalog(const BoundaryValue&,const TaskCatalog&,const KnownPersonalDialogueCatalog&,const InteractionCatalog&);
BoundaryValue encode_task_personal_message_catalog(const TaskPersonalMessageCatalog&);
}
