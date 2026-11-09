// 共享原Task只借同一候选的接触源；私人新路径不能拿别的Task或World服务替代原声明。
#pragma once
#include "task_personal_message.hpp"
#include "resident/domains/tasks/extensions/task_extension.hpp"
namespace hanmo::resident {
Status validate_task_personal_message_services(const CommonStore&,const TaskState&,const TaskCatalog&,const TaskPersonalMessageState*,const TaskPersonalMessageServices*);
Status validate_task_personal_message_extension_pair(const TaskPersonalMessageCatalog&,const TaskExtensionCatalog*);
// 原Task早结构门只核同一Action/开工行，完整Known与历史控制仍由晚位全验负责。
Status validate_task_personal_message_action_shape(const CommonStore&,const TaskState&,const TaskPersonalMessageState&,const ActionCoreView&,const TaskAction&);
bool task_personal_message_can_interrupt(const StableId& code);
}
