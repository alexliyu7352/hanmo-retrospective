// 本人返报开工与真实推进完成的永久提交绑定；原Q01交付result不能代替本次报告。
#pragma once
#include "task_personal_report_contract.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Status validate_task_personal_report_results(const TaskPersonalReportSources&);
Outcome<std::vector<StableId>> task_personal_report_new_result_sources(const TaskPersonalReportState&,const TaskPersonalReportState&,const StableId& actor,const StableId& kind,const ReceiptDetails&,Integer tick,Integer before_tick);
Status bind_task_personal_report_results(CommonStore&,TaskPersonalReportState&,const TaskPersonalReportSources&,const std::vector<StableId>& facts,const StableId& actor,Integer request,const CommitResult&,Integer before_tick);
}
