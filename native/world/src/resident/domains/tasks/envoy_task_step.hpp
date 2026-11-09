// 使者只沿原Common Action推进；正常暂停/重新接触是业务结果，损坏来源仍拒绝整个候选。
#pragma once
#include "envoy_task_completion.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct EnvoyTaskStep {
    bool suspended=false,action_finished=false,success=true;
    StableId task_id,code;
    std::optional<StableId> fact_id;
};
// 递交/返报共用原行动中止事实；仅供已校验的本域阶段候选，不释放Common参与者。
Outcome<EnvoyTaskStep> interrupt_envoy_handover(CommonStore&,TaskState&,EnvoyTaskContract&,const ActionCoreView&,Integer work_ticks,Integer stopped_tick,const StableId& reason,const StableId& note,bool terminate,const std::optional<StableId>& cause);
using EnvoyPresentationAttempt=std::function<Outcome<DiplomaticTaskPresentationAttempt>(const TaskEnvoyContactRead&,const StableId&,const StableId&,const DynamicStepInput&)>;
// 只处理阶段开始即存在的递交行动；调用方仍统一写ActionCompletion/释放原参与者。
Outcome<EnvoyTaskStep> step_envoy_delivery_action(CommonStore&,TaskState&,EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const StableId& action_id,const DynamicStepInput&,const EnvoyPresentationAttempt&);
}
