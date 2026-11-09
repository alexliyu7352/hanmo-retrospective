// 先核当前本人承诺再读typed来源；不存在和他人任务同样拒绝，不能探测远方私务。
#include "governance_task_view.hpp"
namespace hanmo::resident {
Outcome<GovernanceTaskProgress> query_governance_task_progress(const CommonStore& common,const TaskState& tasks,const GovernanceTaskState& governance,const StableId& task_id){
    const auto& observer=common.state().metadata.player_actor_id;const auto* task=tasks.tasks.find(task_id);if(!task||task->assignee!=observer)return {false,{},common_failure("TaskNotKnown").error};
    GovernanceTaskRead read(common,tasks,governance);auto source=read.governance_task_source(task_id);if(!source.ok)return {false,{},source.error};const auto* contract=governance.governance_contracts.find(task_id);const auto& accepted=source.value.acceptance;
    if(accepted.display_name.size()>4096||contract->pending_code.size()>4096||contract->execution_note_code.size()>4096)return {false,{},common_failure("GovernanceTaskTextLimit").error};
    GovernanceTaskProgress result;result.source_identity=common.state().identity;result.observer_actor_id=observer;result.task_id=task_id;result.display_name=accepted.display_name;result.status=task->status;result.accepted_tick=accepted.accepted_tick;result.deadline_tick=accepted.deadline_tick;
    if(task->status=="active"){const auto* action=common.action(contract->action_id);if(source.value.work||source.value.completion||source.value.interruption||!action||action->kind!="governance_appease"||action->actor_id!=observer||action->remaining_ticks<1)return {false,{},common_failure("InvalidGovernanceActiveTask",task_id,ErrorKind::internal).error};result.action_id=action->id;result.remaining_ticks=action->remaining_ticks;result.execution_note_code=contract->execution_note_code;}
    else if(task->status=="ready"){if(!source.value.work||source.value.completion||common.action(contract->action_id))return {false,{},common_failure("InvalidGovernanceReadyTask",task_id,ErrorKind::internal).error};result.waiting_code=contract->pending_code;}
    else if(task->status=="settling"){if(!source.value.interruption||source.value.work||source.value.completion||common.action(contract->action_id))return {false,{},common_failure("InvalidGovernanceInterruptedTask",task_id,ErrorKind::internal).error};result.waiting_code=contract->pending_code;result.interruption_fact_id=source.value.interruption->fact_id;result.completed_work_ticks=source.value.interruption->completed_work_ticks;}
    else{if(task->status!="success"&&task->status!="cancelled"&&task->status!="failed")return {false,{},common_failure("InvalidGovernanceTaskStatus",task_id,ErrorKind::internal).error};if(!source.value.completion||task->settlement_id!=source.value.completion->fact_id||common.action(contract->action_id))return {false,{},common_failure("MissingGovernanceTerminalSource",task_id,ErrorKind::internal).error};const auto outcome=source.value.completion->result;const auto expected=outcome==GovernanceTaskResult::completed?"success":outcome==GovernanceTaskResult::cancelled?"cancelled":"failed";if(task->status!=expected)return {false,{},common_failure("GovernanceTerminalSourceMismatch",task_id,ErrorKind::internal).error};}
    if(source.value.work){result.work_fact_id=source.value.work->fact_id;result.work_completed_tick=source.value.work->completed_tick;}
    if(source.value.completion){result.final_fact_id=source.value.completion->fact_id;result.final_completed_tick=source.value.completion->completed_tick;}
    return {true,std::move(result),{}};
}
// 冷DTO绑定完整世界来源，只有实际存在的行动/事实字段才输出。
BoundaryValue encode_governance_task_progress(const GovernanceTaskProgress& row){
    using V=BoundaryValue;StableId hex;const char* digits="0123456789abcdef";for(const auto byte:row.source_identity.world_id){hex.push_back(digits[byte>>4]);hex.push_back(digits[byte&15]);}
    V identity=boundary::obj({{"world_id",V{hex}},{"execution_epoch",V{row.source_identity.execution_epoch}},{"revision",V{row.source_identity.revision}},{"tick",V{row.source_identity.tick}}});
    V::Dict result{{"ok",V{true}},{"source_identity",std::move(identity)},{"observer_actor_id",V{row.observer_actor_id}},{"task_id",V{row.task_id}},{"display_name",V{row.display_name}},{"status",V{row.status}},{"accepted_tick",V{row.accepted_tick}},{"deadline_tick",V{row.deadline_tick}},{"waiting_code",V{row.waiting_code}},{"execution_note_code",V{row.execution_note_code}}};
    if(row.action_id)result.emplace_back("action_id",V{*row.action_id});if(row.remaining_ticks)result.emplace_back("remaining_ticks",V{*row.remaining_ticks});if(row.work_fact_id)result.emplace_back("work_fact_id",V{*row.work_fact_id});if(row.work_completed_tick)result.emplace_back("work_completed_tick",V{*row.work_completed_tick});if(row.final_fact_id)result.emplace_back("final_fact_id",V{*row.final_fact_id});if(row.final_completed_tick)result.emplace_back("final_completed_tick",V{*row.final_completed_tick});if(row.interruption_fact_id)result.emplace_back("interruption_fact_id",V{*row.interruption_fact_id});if(row.completed_work_ticks)result.emplace_back("completed_work_ticks",V{*row.completed_work_ticks});return V{std::move(result)};
}
// 手记按真实阶段叙述；等待码留给原错误翻译口，不把内部ID写进玩家正文。
std::vector<StableId> governance_task_progress_lines(const GovernanceTaskProgress& row){
    const auto status=row.status=="active"?(row.execution_note_code.empty()?"正在办理":"暂停办理，须待执行条件恢复"):row.status=="ready"?"事务已做完，尚待钱粮送达":row.status=="settling"?"办理已经中止，责任尚待清算或改派":row.status=="success"?"已办妥":row.status=="cancelled"?"已取消":"未能办妥";
    std::vector<StableId> lines{row.display_name+" · "+status,"第"+std::to_string(row.accepted_tick/2+1)+"日应承的治理事务。"};
    if(row.remaining_ticks)lines.push_back("还须办理 "+std::to_string(*row.remaining_ticks)+" 个半日。");
    if(row.completed_work_ticks)lines.push_back("中止前已实际办理 "+std::to_string(*row.completed_work_ticks)+" 个半日，未获完整治理成果。");
    if(row.status=="active"||row.status=="ready"){if(row.source_identity.tick>row.deadline_tick)lines.push_back("原定办结期限已过，仍须核对送达与清算。");else lines.push_back("距原定办结期限还有 "+std::to_string(row.deadline_tick-row.source_identity.tick)+" 个半日。");}
    if(row.status=="cancelled"&&row.work_fact_id)lines.push_back("已经做过的事务仍有记录，尚未兑现治理成果。");return lines;
}
}
