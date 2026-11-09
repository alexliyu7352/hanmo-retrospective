// 本人已受领私人信件的返报责任，只展示原办理事实与现在实际Known会面的可执行报价。
#pragma once
#include "task_personal_report_contract.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_task_personal_reports(const TaskPersonalReportSources&,const InteractionState&,const InteractionAccess&,Integer cursor=0);
}
