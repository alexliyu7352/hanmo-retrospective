// 原Task只借同一候选的新受领服务；结构门不反调人物或Q01全史。
#pragma once
#include "task_personal_acceptance_contract.hpp"
namespace hanmo::resident {
Status validate_task_personal_acceptance_services(const CommonStore&,const TaskState&,const TaskCatalog&,const TaskPersonalAcceptanceState*,const TaskPersonalAcceptanceServices*);
Status validate_task_personal_acceptance_task_coverage(const TaskState&,const TaskPersonalAcceptanceCatalog*,const TaskPersonalAcceptanceState*);
Status validate_task_personal_acceptance_action_shape(const CommonStore&,const TaskState&,const TaskPersonalAcceptanceState&,const ActionCoreView&,const TaskAction&);
// 所选Offer的旧入口明确拒绝；不是所选Offer则保留原准入，不能用新source_kind绕选段。
Status task_personal_acceptance_original_preview_gate(const TaskPersonalAcceptanceCatalog*,const TaskCommand&);
Status task_personal_acceptance_original_start_gate(const CommonStore&,const TaskState&,const TaskCatalog&,const TaskPersonalAcceptanceCatalog*,const TaskPersonalAcceptanceServices*,const ActionCoreView&,const TaskCommand&,const TaskQuote&);
bool task_personal_acceptance_can_interrupt(const StableId&);
}
