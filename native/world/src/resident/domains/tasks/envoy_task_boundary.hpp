// 在途/尚未开始交接的责任仍受原期限约束；本叶不推进旅行、代选受任人或生成劳动。
#pragma once
#include "envoy_task_rules.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
// 原clock_commit之后调用一次；已递交/已终态及有交接Action的责任交原对应机制处理。
Status reconcile_envoy_tasks_boundary(CommonStore&,TaskState&,EnvoyTaskState&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const DynamicStepInput&);
}
