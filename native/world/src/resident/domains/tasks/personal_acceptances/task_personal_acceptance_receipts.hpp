// 本人受领开工与真实推进完成的永久提交绑定；原Q01交付result不能代替本次受领。
#pragma once
#include "task_personal_acceptance_contract.hpp"
namespace hanmo::resident {
struct ReceiptDetails;struct CommitResult;
Status validate_task_personal_acceptance_results(const TaskPersonalAcceptanceSources&);
Outcome<std::vector<StableId>> task_personal_acceptance_new_result_sources(const TaskPersonalAcceptanceState&,const TaskPersonalAcceptanceState&,const StableId& actor,const StableId& kind,const ReceiptDetails&,Integer tick,Integer before_tick);
Status bind_task_personal_acceptance_results(CommonStore&,TaskPersonalAcceptanceState&,const TaskPersonalAcceptanceSources&,const std::vector<StableId>& facts,const StableId& actor,Integer request,const CommitResult&,Integer before_tick);
}
