// 使者手记只展示本人真实受领与原行动进度；显示递交不表示条约获对方认可。
#pragma once
#include "envoy_task_rules.hpp"
namespace hanmo::resident {
struct GovernanceTaskState;
struct EnvoyTaskProgress {
    WorldIdentity source_identity;
    StableId observer_actor_id,task_id,display_name,status,waiting_code;
    StableId mission_authority_fact_id,acceptance_fact_id,contract_id,final_reason;
    StableId delivery_site_id,report_site_id;
    Integer accepted_tick=0,delivery_expires_tick=0;
    bool return_report_required=false;
    std::optional<StableId> action_id,action_kind,delivery_fact_id,final_fact_id;
    std::optional<Integer> remaining_ticks,delivered_tick,completed_tick;
    std::optional<StableId> last_interruption_fact_id,last_interruption_reason,diplomatic_report_fact_id;
    std::optional<Integer> last_interruption_tick,last_completed_work_ticks;
};
// 先核本人归属再读政治历史，未知ID与别人任务统一拒绝，不透露远方任务状态。
Outcome<EnvoyTaskProgress> query_envoy_task_progress(const CommonStore&,const TaskState&,const EnvoyTaskState&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const StableId& task_id,const DiplomaticEnvoyReportRead* reports=nullptr);
BoundaryValue encode_envoy_task_progress(const EnvoyTaskProgress&);
std::vector<StableId> envoy_task_progress_lines(const EnvoyTaskProgress&);
// 原手记按明确登记来源分流本人使者任务，其余约定继续沿旧有限手记规则。
Outcome<BoundaryValue> query_task_journal_with_responsibilities(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const InteractionState&,const SocialState&,const GrowthState&,const GovernanceTaskState&,bool governance_registered,const EnvoyTaskState&,bool envoy_registered,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const DiplomaticEnvoyReportRead* reports=nullptr);
}
