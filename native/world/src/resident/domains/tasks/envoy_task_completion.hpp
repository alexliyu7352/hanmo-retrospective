// 真实政治呈递与使者Task交付在同一候选成立；任务结束不等于外交提议被接受。
#pragma once
#include "envoy_task_rules.hpp"
#include "envoy_task_contact.hpp"
namespace hanmo::resident {
// 组合层捕获原DynamicStepInput并调用S6具名呈递事务，不允许自报一个公共fact冒成功。
using EnvoyPresentationCommit=std::function<Outcome<StableId>(const TaskEnvoyContactRead&,const StableId& task_id,const StableId& action_id,Integer completed_tick)>;
// Action已减零且未释放时调用；两域任一错误都要求丢弃整个候选，Common释放仍由统一阶段负责。
Outcome<EnvoyTaskSource> finish_envoy_delivery_in_candidate(CommonStore&,TaskState&,EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const StableId& task_id,const StableId& action_id,Integer completed_tick,const EnvoyPresentationCommit&);
}
