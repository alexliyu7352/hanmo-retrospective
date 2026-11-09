// 新返报来源全验在原Known/Q01/人物史之后；早Task结构只能调用精确link，不授会面或付款权。
#pragma once
#include "task_personal_report_contract.hpp"
namespace hanmo::resident {
Status validate_task_personal_report_business(const TaskPersonalReportSources&);
Status validate_task_personal_report_link(const CommonStore&,const TaskPersonalReportState&,const TaskMessageReport&);
Status validate_task_personal_report_action_shape(const CommonStore&,const TaskState&,const TaskPersonalReportState&,const ActionCoreView&,const TaskAction&);
bool task_personal_report_can_interrupt(const StableId& code);
}
