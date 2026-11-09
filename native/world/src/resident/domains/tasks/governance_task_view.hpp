// 本人已接受治理任务的有限只读投影；任务手记消费它，政治栏不另造倒计时。
#pragma once
#include "governance_task_rules.hpp"
namespace hanmo::resident {
// work与final明确分开，ready不再携带remaining_ticks，也不会推断钱粮或民心已经兑现。
struct GovernanceTaskProgress {
    WorldIdentity source_identity;
    StableId observer_actor_id,task_id,display_name,status,waiting_code,execution_note_code;
    Integer accepted_tick=0,deadline_tick=0;
    std::optional<StableId> action_id,work_fact_id,final_fact_id;
    std::optional<Integer> remaining_ticks,work_completed_tick,final_completed_tick;
    std::optional<StableId> interruption_fact_id;
    std::optional<Integer> completed_work_ticks;
};
Outcome<GovernanceTaskProgress> query_governance_task_progress(const CommonStore&,const TaskState&,const GovernanceTaskState&,const StableId& task_id);
BoundaryValue encode_governance_task_progress(const GovernanceTaskProgress&);
std::vector<StableId> governance_task_progress_lines(const GovernanceTaskProgress&);
// 原手记只为已登记治理模块委派本人sidecar；普通合同、行囊、见闻继续原有限来源规则。
Outcome<BoundaryValue> query_task_journal_with_governance(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const InteractionState&,const SocialState&,const GrowthState&,const GovernanceTaskState&,bool registered);
}
