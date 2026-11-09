// S6唯一政治呈递与S3唯一任务完成在同一候选成立；不持嵌套保存点或重放新命令。
#include "envoy_task_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_missions.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_reports.hpp"
namespace hanmo::resident {
namespace {
// gate由唯一显式外交作者块派生；目录丢失是坏候选，不静默丢弃已受责任。
Status available(const WorldBundle& w){
    if(!w.envoy_tasks_enabled()||!w.diplomacy_enabled())return common_failure("EnvoyTasksNotEnabled",{},ErrorKind::unsupported_capability);
    if(!w.catalog->diplomacy||!w.catalog->diplomacy->enabled||!w.catalog->organization||!w.catalog->interaction)return common_failure("MissingEnvoyPhaseCatalog",{},ErrorKind::internal);
    return common_success();
}
// 两域都必须已明确进入v2；独立的通行模块或仅一域升级不能开放返报。
Status report_available(const WorldBundle& w){auto ready=available(w);if(!ready.ok)return ready;if(w.envoy_tasks.version!=2||w.diplomacy.version!=2||w.catalog->diplomacy->version!=2)return common_failure("EnvoyReportVersionRequired",{},ErrorKind::unsupported_capability);return common_success();}
}
Outcome<EnvoyTaskStep> step_envoy_task_world(WorldBundle& w,const StableId& action_id,const DynamicStepInput& step){
    auto enabled=available(w);if(!enabled.ok)return {false,{},enabled.error};
    auto interaction=w.services.interaction_access();
    DiplomaticPresentRead read{w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,w.interaction,*w.catalog->interaction,interaction};
    // 完成源只能在当前捕获的actions_step读取到to，普通公开/冷读取仍不开放未来。
    DiplomaticMissionAccess missions(w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,step);
    EnvoyPresentationAttempt present=[&](const TaskEnvoyContactRead& contact,const StableId& task,const StableId& action,const DynamicStepInput& phase){return attempt_diplomatic_envoy_task_in_transaction(w.common,w.diplomacy,read,contact,task,action,phase);};
    return step_envoy_delivery_action(w.common,w.tasks,w.envoy_tasks,w.interaction,*w.catalog->interaction,interaction,missions,missions,action_id,step,present);
}
Status reconcile_envoy_task_world(WorldBundle& w,const DynamicStepInput& step){
    if(!w.envoy_tasks_enabled()){if(!w.envoy_tasks.envoy_contracts.rows().empty())return common_failure("UnexpectedEnvoyTasksState",{},ErrorKind::internal);return common_success();}
    auto enabled=available(w);if(!enabled.ok)return enabled;
    DiplomaticMissionAccess missions(w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy);
    return reconcile_envoy_tasks_boundary(w.common,w.tasks,w.envoy_tasks,missions,missions,step);
}
Outcome<EnvoyTaskStep> step_envoy_task_world_with_reports(WorldBundle& w,DiplomacyV2State& report_state,const StableId& action_id,const DynamicStepInput& step){
    auto enabled=report_available(w);if(!enabled.ok)return {false,{},enabled.error};if(report_state.version!=2)return {false,{},common_failure("EnvoyReportVersionRequired").error};if(!w.diplomacy.v2||&report_state!=&*w.diplomacy.v2)return {false,{},common_failure("EnvoyReportCandidateMismatch",{},ErrorKind::internal).error};const auto* action=w.common.action(action_id);if(!action)return {false,{},common_failure("UnknownEnvoyAction",action_id,ErrorKind::internal).error};
    if(action->kind=="envoy_deliver")return step_envoy_task_world(w,action_id,step);if(action->kind!="envoy_report")return {false,{},common_failure("InvalidEnvoyActionKind",action_id,ErrorKind::internal).error};
    auto interaction=w.services.interaction_access();DiplomaticPresentRead political{w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,w.interaction,*w.catalog->interaction,interaction};DiplomaticMissionAccess missions(w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,step);
    DiplomaticReportAccess reports(political,report_state,step);
    EnvoyReportContinuityRead continuity=[&](const EnvoyTaskAcceptance& accepted,const EnvoyTaskDelivery& delivered){return diplomatic_report_responsibility(political,accepted.mission_authority_fact_id,accepted.envoy_actor_id,delivered.diplomatic_presentation_fact_id);};
    EnvoyReportCommit receive=[&](const TaskEnvoyReportContactRead& contact,const StableId& task,const StableId& action,Integer to)->Outcome<DiplomaticEnvoyReportAttempt>{if(to!=step.to_tick)return {false,{},common_failure("InvalidEnvoyReportStep",action,ErrorKind::internal).error};return attempt_diplomatic_envoy_report_in_transaction(w.common,report_state,political,contact,task,action,step);};
    return step_envoy_report_action(w.common,w.tasks,w.envoy_tasks,w.interaction,*w.catalog->interaction,interaction,missions,missions,reports,action_id,step,continuity,receive);
}
Status reconcile_envoy_task_world_with_reports(WorldBundle& w,const DynamicStepInput& step){
    auto enabled=report_available(w);if(!enabled.ok)return enabled;auto original=reconcile_envoy_task_world(w,step);if(!original.ok)return original;
    auto interaction=w.services.interaction_access();DiplomaticPresentRead political{w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,w.interaction,*w.catalog->interaction,interaction};DiplomaticMissionAccess missions(w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy);
    EnvoyReportContinuityRead continuity=[&](const EnvoyTaskAcceptance& accepted,const EnvoyTaskDelivery& delivered){return diplomatic_report_responsibility(political,accepted.mission_authority_fact_id,accepted.envoy_actor_id,delivered.diplomatic_presentation_fact_id);};
    return reconcile_envoy_reports_boundary(w.common,w.tasks,w.envoy_tasks,missions,missions,step,continuity);
}
}
