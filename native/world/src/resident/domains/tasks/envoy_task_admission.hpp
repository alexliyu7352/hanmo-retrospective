// 正式使者受领只消费政治域当前具名批准；接受不自动旅行、呈递或替目标组织同意。
#pragma once
#include "envoy_task_state.hpp"
#include "task_model.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_mission_contract.hpp"
namespace hanmo::resident {
// 报价不分配实例或占用人物；实际执行再次读取同候选的当前政治批准。
Outcome<DiplomaticMissionApproval> preview_envoy_task_acceptance(const CommonStore&,const EnvoyTaskState&,const DiplomaticMissionRead&,const StableId& envoy_actor_id,const StableId& authority_fact_id,Integer expected_world_revision);
// 调用者持有整个World候选及各域备份；失败必须撤回Common、Task及sidecar，不能只回滚公共存储。
Outcome<EnvoyTaskAcceptance> accept_envoy_task_in_candidate(CommonStore&,TaskState&,EnvoyTaskState&,const CommonCatalog&,const DiplomaticMissionRead&,const StableId& envoy_actor_id,const StableId& authority_fact_id,Integer expected_world_revision);
}
