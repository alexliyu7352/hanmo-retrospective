// 使者阶段只组合当前唯一World候选；释放Action与完成回执由中央原阶段处理。
#pragma once
#include "resident/domains/tasks/envoy_task_step.hpp"
#include "resident/domains/tasks/envoy_task_boundary.hpp"
#include "resident/domains/tasks/envoy_task_report_step.hpp"
namespace hanmo::resident {
struct WorldBundle;
struct DiplomacyV2State;
Outcome<EnvoyTaskStep> step_envoy_task_world(WorldBundle&,const StableId& action_id,const DynamicStepInput&);
Status reconcile_envoy_task_world(WorldBundle&,const DynamicStepInput&);
// v2显式借同一DiplomacyState的报告子对象，内部绑定真实政治读写服务和原DynamicStepInput。
Outcome<EnvoyTaskStep> step_envoy_task_world_with_reports(WorldBundle&,DiplomacyV2State&,const StableId& action_id,const DynamicStepInput&);
// 原递交责任维护之后再维护已递交待返责任，仍只调用一次公共clock边界。
Status reconcile_envoy_task_world_with_reports(WorldBundle&,const DynamicStepInput&);
}
