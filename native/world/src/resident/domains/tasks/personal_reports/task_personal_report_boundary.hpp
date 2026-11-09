// 原任务ID是唯一可写参数；原发信者、地点、说话节点与原报酬都来自真实已受领来源。
#pragma once
#include "task_personal_report_contract.hpp"
namespace hanmo::resident {
Outcome<TaskPersonalReport> decode_task_personal_report(const StableId&,const BoundaryValue& params);
BoundaryValue encode_task_personal_report_quote(const TaskPersonalReportQuote&);
}
