// v2当面返报沿原report_offer结酬完成点写入，不提供脱离任务的远程通知或任意事实抄送。
#pragma once
#include "task_message_handoff.hpp"
namespace hanmo::resident {
// 准备仅持原交接来源与真实现场；结酬ID由原Task规则成功后补齐。
struct TaskMessageReportPreparation {TaskMessageReport receipt;};
// 返回空表示原公约实际办理人缺可复核历史指派来源；旧结酬仍继续，但不授新报告知情。
Outcome<std::optional<TaskMessageReportPreparation>> prepare_task_message_report(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageHandoffState&,const ActionCoreView&,Integer,const DynamicStepInput* phase=nullptr);
Status record_task_message_report_after_settlement(CommonStore&,const TaskState&,TaskMessageHandoffState&,const TaskMessageReportPreparation&);
Status validate_task_message_reports(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageHandoffState&,const DynamicStepInput* phase=nullptr);
// 只有亲自返报与实际听取者可知本次报告；原收信人不会反向自动知道已向谁返报。
Outcome<std::optional<TaskMessageReport>> read_known_task_message_report(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const TaskMessageHandoffState&,const StableId& observer,const StableId& report_fact_id,const DynamicStepInput* phase=nullptr);
}
