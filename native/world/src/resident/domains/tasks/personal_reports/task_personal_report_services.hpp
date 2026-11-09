// 原Task唯一writer只借本次同候选返报服务；缺段或借错状态必须在任何钱款写入前拒绝。
#pragma once
#include "task_personal_report.hpp"
#include "resident/domains/tasks/extensions/task_extension.hpp"
namespace hanmo::resident {
Status validate_task_personal_report_services(const CommonStore&,const TaskState&,const TaskCatalog&,const TaskPersonalReportState*,const TaskPersonalReportServices*);
Status validate_task_personal_report_extension_pair(const TaskPersonalReportCatalog&,const TaskExtensionCatalog*);
}
