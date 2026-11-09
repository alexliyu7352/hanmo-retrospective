// 私人发信人离开原岗位后的具名返报许可；只绑定已选Q01原委托，不能借此办理公约或回书。
#pragma once
#include "resident/domains/tasks/personal_messages/task_personal_message_catalog.hpp"
namespace hanmo::resident {
inline constexpr const char* task_personal_report_module="task_personal_message_reports";
inline constexpr const char* task_personal_report_capability="task_personal_message_reports_v1";
inline constexpr const char* task_personal_report_profile="known_private_sender_report_v1";
inline constexpr const char* task_personal_report_command="task_personal_message_report";
inline constexpr const char* task_personal_report_source_kind="personal_message_report";
struct TaskPersonalReportPolicy {StableId id,offer_id,personal_template_id,sender_say_node_id;};
struct TaskPersonalReportCatalog {Integer version=1;std::vector<TaskPersonalReportPolicy> policies;};
const TaskPersonalReportPolicy* task_personal_report_policy(const TaskPersonalReportCatalog&,const StableId& offer_id);
// 原Task、Q01和Known目录先完整通过；此门只将原issuer本人SAY绑定为返报接收者，不赋予岗位权限。
Status validate_task_personal_report_catalog(const TaskPersonalReportCatalog&,const TaskCatalog&,const TaskPersonalMessageCatalog&,const KnownPersonalDialogueCatalog&,const InteractionCatalog&);
Outcome<TaskPersonalReportCatalog> decode_task_personal_report_catalog(const BoundaryValue&,const TaskCatalog&,const TaskPersonalMessageCatalog&,const KnownPersonalDialogueCatalog&,const InteractionCatalog&);
BoundaryValue encode_task_personal_report_catalog(const TaskPersonalReportCatalog&);
}
