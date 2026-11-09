// 返报沿统一actions_step；提议后续过期不重置已递交责任，也不另开任务时间。
#pragma once
#include "envoy_task_report_rules.hpp"
#include "envoy_task_step.hpp"
namespace hanmo::resident {
// 政治域只核返报责任连续性，不能复用会把提议过期判为失效的未交付使命入口。
using EnvoyReportContinuityRead=std::function<Outcome<DiplomaticMissionContinuity>(const EnvoyTaskAcceptance&,const EnvoyTaskDelivery&)>;
Outcome<EnvoyTaskStep> step_envoy_report_action(CommonStore&,TaskState&,EnvoyTaskState&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const DiplomaticEnvoyReportRead&,const StableId& action_id,const DynamicStepInput&,const EnvoyReportContinuityRead&,const EnvoyReportCommit&);
// clock_commit后维护未开工的真实返报责任；不移动人物、不猜行程、不重验提议有效期。
Status reconcile_envoy_reports_boundary(CommonStore&,TaskState&,EnvoyTaskState&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const DynamicStepInput&,const EnvoyReportContinuityRead&);
}
