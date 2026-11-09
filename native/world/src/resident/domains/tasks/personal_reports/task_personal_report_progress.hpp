// 计划只继续本人已经实际开工的返报Action；不能把尚未实际开工的返报猜成普通旅行。
#pragma once
#include "task_personal_report_contract.hpp"
#include "resident/domains/tasks/personal_messages/task_personal_message_progress.hpp"
namespace hanmo::resident {
// 字段同原半日Action只读合同；业务status和实际来源仍分域解释。
using TaskPersonalReportActionSource=TaskPersonalMessageActionSource;
// 调用前同World完整信件、人物与结果先验；未知行动空，已识别而错本人/坏源报错。
Outcome<std::optional<TaskPersonalReportActionSource>> query_task_personal_report_action(const CommonStore&,const TaskState&,const TaskPersonalReportState&,const StableId& actor,const StableId& action);
bool task_personal_report_continuation_meeting_matches(const TaskPersonalReportActionSource&,const InteractionSession&);
struct ReceiptDetails;
Outcome<std::optional<TaskPersonalReportActionSource>> query_task_personal_report_completion(const CommonStore&,const TaskState&,const TaskPersonalReportState&,const StableId& actor,const StableId& action,const ReceiptDetails&,Integer tick,const StableId& actual_result_id);
struct CommitResult;
// 显式真实提交重载覆盖原六种外层；隐藏推进只认当前缓存中的实际对象和永久完成绑定。
Outcome<std::optional<TaskPersonalReportActionSource>> query_task_personal_report_completion(const CommonStore&,const TaskState&,const TaskPersonalReportState&,const StableId& actor,const StableId& action,const CommitResult& actual_result);

}
