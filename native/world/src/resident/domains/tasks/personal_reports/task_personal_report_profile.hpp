// 新返报独立段只随显式handoff4成对选择；旧三版不获得空role返报许可。
#pragma once
#include "task_personal_report_contract.hpp"
namespace hanmo::resident {
Outcome<bool> decode_task_personal_report_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Outcome<Integer> task_personal_report_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Status validate_task_personal_report_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const TaskPersonalReportState*);
Outcome<BoundaryValue> task_personal_report_fact_payloads(const CommonStore&,const TaskPersonalReportState*);
Status validate_task_personal_report_json_payloads(const CommonStore&,const TaskPersonalReportState*,const BoundaryValue&);
}
