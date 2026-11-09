// 原S5真实到期结果→既有期次引用→既有客观incident，同一候选采用。
#pragma once
#include "treaty_material_aid_incident.hpp"
namespace hanmo::resident {
struct TreatyAidDeadlineAdopted {SourceRef actual_deadline_result,incident_ref;};
// 只核援助late的双向覆盖；其它incident仍交其真实owner全域validator。
Status validate_treaty_aid_deadline_adoption(const MaterialAidReadContext&,
 const TreatyAidPerformanceReadContext&,const OrderedTable<TreatyIncident>&);
// 真实收齐返回null，不制造成功fact或新期钟。真实有界赔偿必须已由原owner成立。
// 旧late重放仅反验完整原闭包；不修复缺失引用/incident的冷文件。失败撤整个World候选。
Outcome<std::optional<TreatyAidDeadlineAdopted>> close_and_record_treaty_aid_deadline_in_candidate(
 CommonStore&,TreatyMaterialAidState&,OrderedTable<TreatyAidOccurrenceBinding>&,
 OrderedTable<TreatyIncident>&,const MaterialAidReadContext&,
 const TreatyAidPerformanceReadContext&,const SourceRef& actual_occurrence_binding,
 const std::optional<ObligationRef>& actual_compensation={});
}
