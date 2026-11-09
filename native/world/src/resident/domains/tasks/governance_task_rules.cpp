// 接受、劳动与最终事实交叉核真；不得借实际劳动完成直接发放民心或凭金额伪造资源收据。
#include "governance_task_rules.hpp"
#include "resident/domains/local_economy/relief/public_relief.hpp"
#include <set>
#include <limits>
namespace hanmo::resident {
namespace {
// 公共索引只是fact头，必须同时命中本域typed记录及其真实task/action链接。
bool fact_is(const CommonStore& common,const StableId& id,const StableId& kind,const StableId& domain="tasks"){
    const auto* fact=common.state().facts.find(id);return fact&&fact->id==id&&fact->kind==kind&&fact->domain==domain;
}
// 不同终态登记不同公共kind，取消不得被下游当作成功完成源消费。
StableId completion_kind(GovernanceTaskResult result){return result==GovernanceTaskResult::completed?"governance_task_completed":result==GovernanceTaskResult::cancelled?"governance_task_cancelled":"governance_task_failed";}
// 接受和任务核心必须指向同一个真人、工期和期限，不允许伪造第二个终态账。
Status source_matches(const CommonStore& common,const TaskRecord* task,const GovernanceTaskContract& contract){
    const auto& a=contract.acceptance;
    if(!task||task->id!=a.task_id||task->offer_id!=a.authority_source_fact_id||task->serial<1||task->assignee!=a.executor_actor_id||task->accepted_tick!=a.accepted_tick||task->deadline_tick!=a.deadline_tick||!fact_is(common,a.fact_id,"governance_task_accepted")||!fact_is(common,a.authority_source_fact_id,"governance_authority_issued","politics"))return common_failure("InvalidGovernanceTaskSource",a.task_id,ErrorKind::internal);
    if(contract.work){const auto& w=*contract.work;if(w.task_id!=a.task_id||w.contract_id!=a.contract_id||w.action_id!=contract.action_id||w.acceptance_fact_id!=a.fact_id||!fact_is(common,w.fact_id,"governance_work_completed"))return common_failure("InvalidGovernanceWorkSource",a.task_id,ErrorKind::internal);}
    if(contract.completion){const auto& f=*contract.completion;if(f.task_id!=a.task_id||f.contract_id!=a.contract_id||f.action_id!=contract.action_id||f.acceptance_fact_id!=a.fact_id||!fact_is(common,f.fact_id,completion_kind(f.result)))return common_failure("InvalidGovernanceCompletionSource",a.task_id,ErrorKind::internal);}
    if(contract.interruption){
        const auto& f=*contract.interruption;if(f.task_id!=a.task_id||f.contract_id!=a.contract_id||f.action_id!=contract.action_id||f.acceptance_fact_id!=a.fact_id||!fact_is(common,f.fact_id,"governance_work_interrupted"))return common_failure("InvalidGovernanceInterruptionSource",a.task_id,ErrorKind::internal);
        // 原因可无可靠具名源，但一旦保存就必须在冷恢复与下游读取时仍能核到原公共事实。
        if(f.cause_fact_id){const auto* cause=common.state().facts.find(*f.cause_fact_id);if(f.cause_fact_id->empty()||*f.cause_fact_id==f.fact_id||!cause||cause->id!=*f.cause_fact_id)return common_failure("MissingGovernanceInterruptionCause",a.task_id,ErrorKind::internal);}
    }
    return common_success();
}
}
// 旧测试或第三域适配器若没有实现枚举，明确不可证，禁止默认为未受领。
Outcome<std::optional<GovernanceTaskSource>> TaskGovernanceRead::governance_task_for_authority(const StableId& id)const{return {false,{},common_failure("GovernanceAuthorityTaskReadUnavailable",id).error};}
// 所有原契约仍走同一完整来源读；取消或完成不抹去曾经接受这份邀请的事实。
Outcome<std::optional<GovernanceTaskSource>> GovernanceTaskRead::governance_task_for_authority(const StableId& id)const{
    const auto* h=common_.state().facts.find(id);if(id.empty()||!h||h->id!=id||h->domain!="politics"||h->kind!="governance_authority_issued")return {false,{},common_failure("UnknownGovernanceAuthority",id).error};
    std::optional<GovernanceTaskSource> found;
    for(const auto& row:governance_.governance_contracts.rows()){auto source=governance_task_source(row.first);if(!source.ok)return {false,{},source.error};if(source.value.acceptance.authority_source_fact_id!=id)continue;if(found)return {false,{},common_failure("DuplicateGovernanceAuthorityTask",id).error};found=std::move(source.value);}
    return {true,std::move(found),{}};
}
GovernanceTaskRead::GovernanceTaskRead(const CommonStore& c,const TaskState& t,const GovernanceTaskState& g):common_(c),tasks_(t),governance_(g){}
// 未知记录保留明确失败，不能以空successful状态证明已受领或完成。
Outcome<GovernanceTaskSource> GovernanceTaskRead::governance_task_source(const StableId& id)const{
    const auto* contract=governance_.governance_contracts.find(id);if(!contract)return {false,{},common_failure("UnknownGovernanceTask",id).error};
    const auto* task=tasks_.tasks.find(id);auto valid=source_matches(common_,task,*contract);if(!valid.ok)return {false,{},valid.error};
    return {true,{contract->acceptance,task->status,contract->work,contract->completion,contract->interruption},{}};
}
// 只扫描本域工作事实，不枚举世界人物/资产；命中后仍反查接受源与唯一任务。
Outcome<GovernanceWorkCompletion> GovernanceTaskRead::governance_work_completion(const StableId& id)const{
    for(const auto& row:governance_.governance_contracts.rows())if(row.second.work&&row.second.work->fact_id==id){auto source=governance_task_source(row.first);if(!source.ok)return {false,{},source.error};return {true,*source.value.work,{}};}return {false,{},common_failure("UnknownGovernanceWorkFact",id).error};
}
// 最终源与工作源是两个不同ID，不能把仅完成劳动作为资源已经送达的证明。
Outcome<GovernanceTaskCompletion> GovernanceTaskRead::governance_task_completion(const StableId& id)const{
    for(const auto& row:governance_.governance_contracts.rows())if(row.second.completion&&row.second.completion->fact_id==id){auto source=governance_task_source(row.first);if(!source.ok)return {false,{},source.error};return {true,*source.value.completion,{}};}return {false,{},common_failure("UnknownGovernanceCompletionFact",id).error};
}
Outcome<GovernanceWorkCompletion> finish_governance_work(CommonStore& common,TaskState& tasks,GovernanceTaskState& governance,const StableId& action_id,Integer to_tick,const GovernanceOutcomeFreeze& freeze){
    GovernanceTaskContract* contract=nullptr;for(auto& row:governance.governance_contracts.rows())if(row.second.action_id==action_id){if(contract)return {false,{},common_failure("DuplicateGovernanceAction",action_id,ErrorKind::internal).error};contract=&row.second;}
    if(!contract)return {false,{},common_failure("UnknownGovernanceAction",action_id).error};
    auto* task=tasks.tasks.find(contract->acceptance.task_id);auto valid=source_matches(common,task,*contract);if(!valid.ok)return {false,{},valid.error};
    if(contract->work)return {true,*contract->work,{}};
    const auto* action=common.action(action_id);const auto& a=contract->acceptance;
    if(!freeze||task->status!="active"||contract->completion||contract->interruption||!action||action->kind!="governance_appease"||action->actor_id!=a.executor_actor_id||action->participants!=std::vector<StableId>{a.executor_actor_id}||action->remaining_ticks!=0||action->start_tick<a.accepted_tick||to_tick<=action->start_tick||a.work_ticks<=0||to_tick-action->start_tick<a.work_ticks||common.tick()==std::numeric_limits<Integer>::max()||to_tick!=common.tick()+1)return {false,{},common_failure("GovernanceWorkNotCompleted",action_id,ErrorKind::internal).error};
    auto id=common.next_id("governance_work");if(!id.ok)return {false,{},id.error};
    GovernanceWorkCompletion work{id.value,a.task_id,a.contract_id,action_id,a.fact_id,{},action->start_tick,to_tick};
    valid=common.append_fact({work.fact_id,"governance_work_completed","tasks"});if(!valid.ok)return {false,{},valid.error};contract->work=work;
    GovernanceTaskRead read(common,tasks,governance);auto plan=freeze(read,work);if(!plan.ok)return {false,{},plan.error};if(plan.value.empty()||!fact_is(common,plan.value,"governance_outcome_plan","politics"))return {false,{},common_failure("MissingGovernanceOutcomePlan",a.task_id,ErrorKind::internal).error};
    contract->work->outcome_plan_id=plan.value;contract->execution_note_code.clear();task->objective_completed_tick=to_tick;task->status="ready";return {true,*contract->work,{}};
}
bool governance_reservation_exists(const TaskState& tasks,const GovernanceTaskState& governance,const StableId& id){
    for(const auto& row:governance.governance_contracts.rows())if(row.second.acceptance.contract_id==id){const auto* task=tasks.tasks.find(row.first);return task&&!row.second.completion&&!row.second.interruption&&(task->status=="active"||task->status=="ready");}return false;
}
Status settle_ready_governance_tasks(CommonStore& common,TaskState& tasks,GovernanceTaskState& governance,const GovernanceSettlementServices& services){
    if(!services.distribute||!services.receipts||!services.apply_outcome)return common_failure("MissingGovernanceSettlementServices",{},ErrorKind::internal);
    GovernanceTaskRead read(common,tasks,governance);
    for(auto& row:governance.governance_contracts.rows()){
        auto* task=tasks.tasks.find(row.first);auto& contract=row.second;if(!task)return common_failure("InvalidGovernanceTaskSource",row.first,ErrorKind::internal);if(task->status!="ready"||contract.completion)continue;
        auto source=read.governance_task_source(row.first);if(!source.ok)return {false,{},source.error};
        if(!contract.work||contract.work->outcome_plan_id.empty()||contract.work->completed_tick>common.tick()||common.action(contract.action_id))return common_failure("GovernanceSettlementBeforeWorkRelease",row.first,ErrorKind::internal);
        if(contract.last_settlement_attempt_tick==common.tick())continue;
        auto result=services.distribute(read,source.value);if(!result.ok)return {false,{},result.error};
        contract.last_settlement_attempt_tick=common.tick();
        if(result.value.kind==GovernanceSettlementAttempt::Kind::pending){if(!result.value.resource_receipt_ids.empty()||result.value.code.empty())return common_failure("InvalidGovernancePendingResult",row.first,ErrorKind::internal);contract.pending_code=result.value.code;continue;}
        if(result.value.resource_receipt_ids.size()!=1||!result.value.code.empty())return common_failure("InvalidGovernanceSettlementResult",row.first,ErrorKind::internal);
        const auto& receipt_id=result.value.resource_receipt_ids.front();auto receipt=services.receipts->public_relief_receipt(receipt_id);if(!receipt.ok)return {false,{},receipt.error};if(!receipt.value)return common_failure("MissingGovernanceResourceReceipt",receipt_id,ErrorKind::internal);
        const auto& r=*receipt.value;const auto& a=contract.acceptance;const auto& w=*contract.work;
        if(r.id!=receipt_id||r.terms_id!=a.relief_terms_id||r.task_id!=a.task_id||r.contract_id!=a.contract_id||r.action_id!=w.action_id||r.acceptance_fact_id!=a.fact_id||r.authority_source_fact_id!=a.authority_source_fact_id||r.work_fact_id!=w.fact_id||r.tick!=common.tick())return common_failure("GovernanceResourceSourceMismatch",receipt_id,ErrorKind::internal);
        auto id=common.next_id("governance_complete");if(!id.ok)return {false,{},id.error};GovernanceTaskCompletion final;final.fact_id=id.value;final.task_id=a.task_id;final.contract_id=a.contract_id;final.action_id=w.action_id;final.acceptance_fact_id=a.fact_id;final.work_completion_fact_id=w.fact_id;final.outcome_plan_id=w.outcome_plan_id;final.resource_receipt_ids=result.value.resource_receipt_ids;final.completed_tick=common.tick();
        auto appended=common.append_fact({final.fact_id,"governance_task_completed","tasks"});if(!appended.ok)return appended;contract.completion=final;contract.pending_code.clear();task->status="success";task->settlement_id=final.fact_id;task->reported_tick=common.tick();task->late=common.tick()>task->deadline_tick;
        auto effect=services.apply_outcome(read,final);if(!effect.ok)return effect;
    }
    return common_success();
}
// 报价和执行共用原本人/终态资格；读口不产生取消事实，保原错误先后。
static Outcome<GovernanceTaskSource> cancellation_source(const CommonStore& common,const TaskState& tasks,const GovernanceTaskState& governance,const StableId& actor_id,const StableId& task_id){
    GovernanceTaskRead read(common,tasks,governance);auto source=read.governance_task_source(task_id);if(!source.ok)return source;
    if(source.value.acceptance.executor_actor_id!=actor_id)return {false,{},common_failure("TaskNotAssignedToActor",task_id).error};
    if(source.value.completion){if(source.value.completion->result==GovernanceTaskResult::cancelled)return source;return {false,{},common_failure("TaskNotActive",task_id).error};}
    if(source.value.task_status!="active"&&source.value.task_status!="ready")return {false,{},common_failure("TaskNotActive",task_id).error};
    const auto* actor=common.actor(actor_id);const auto* contract=governance.governance_contracts.find(task_id);
    if(actor&&!actor->action_id.empty()&&actor->action_id!=contract->action_id)return {false,{},common_failure("ActorBusy",actor_id).error};
    return source;
}
bool governance_cancellation_busy_allowed(const CommonStore& common,const TaskState& tasks,const GovernanceTaskState& governance,const StableId& actor_id){
    const auto* actor=common.actor(actor_id);if(!actor||!actor->alive||!actor->available||actor->action_id.empty())return false;
    const auto* action=common.action(actor->action_id);if(!action||action->kind!="governance_appease"||action->actor_id!=actor_id||action->participants!=std::vector<StableId>{actor_id}||action->remaining_ticks<1)return false;
    const GovernanceTaskContract* contract=nullptr;for(const auto& row:governance.governance_contracts.rows())if(row.second.action_id==action->id){if(contract)return false;contract=&row.second;}
    if(!contract||contract->work||contract->completion||contract->interruption||action->start_tick<contract->acceptance.accepted_tick||action->remaining_ticks>contract->acceptance.work_ticks)return false;
    auto source=cancellation_source(common,tasks,governance,actor_id,contract->acceptance.task_id);return source.ok&&source.value.task_status=="active";
}
Outcome<GovernanceTaskSource> preview_governance_cancellation(const CommonStore& common,const TaskState& tasks,const GovernanceTaskState& governance,const StableId& actor_id,const StableId& task_id){
    auto source=cancellation_source(common,tasks,governance,actor_id,task_id);if(!source.ok)return source;
    if(!source.value.completion&&source.value.work&&source.value.work->completed_tick>common.tick())return {false,{},common_failure("GovernanceCancellationBeforeClock",task_id,ErrorKind::internal).error};
    return source;
}
Outcome<GovernanceTaskCompletion> cancel_governance_task(CommonStore& common,TaskState& tasks,GovernanceTaskState& governance,const StableId& actor_id,const StableId& task_id,const GovernanceCancellationServices& services){
    GovernanceTaskRead read(common,tasks,governance);auto source=cancellation_source(common,tasks,governance,actor_id,task_id);if(!source.ok)return {false,{},source.error};
    if(source.value.completion)return {true,*source.value.completion,{}};
    if(!services.release_resources||!services.release_action)return {false,{},common_failure("MissingGovernanceCancellationServices",task_id,ErrorKind::internal).error};
    auto* contract=governance.governance_contracts.find(task_id);auto* task=tasks.tasks.find(task_id);const auto& a=contract->acceptance;
    if(contract->work&&contract->work->completed_tick>common.tick())return {false,{},common_failure("GovernanceCancellationBeforeClock",task_id,ErrorKind::internal).error};
    auto released=services.release_resources(read,source.value);if(!released.ok)return {false,{},released.error};
    if(common.action(contract->action_id)){released=services.release_action(contract->action_id);if(!released.ok)return {false,{},released.error};}
    const auto* actor=common.actor(actor_id);if(common.action(contract->action_id)||(actor&&actor->action_id==contract->action_id))return {false,{},common_failure("GovernanceActionNotReleased",task_id,ErrorKind::internal).error};
    auto id=common.next_id("governance_cancel");if(!id.ok)return {false,{},id.error};GovernanceTaskCompletion final;final.fact_id=id.value;final.task_id=a.task_id;final.contract_id=a.contract_id;final.action_id=contract->action_id;final.acceptance_fact_id=a.fact_id;final.result=GovernanceTaskResult::cancelled;final.completed_tick=common.tick();final.reason="CancelledByAssignee";if(contract->work){final.work_completion_fact_id=contract->work->fact_id;final.outcome_plan_id=contract->work->outcome_plan_id;}
    auto appended=common.append_fact({final.fact_id,completion_kind(final.result),"tasks"});if(!appended.ok)return {false,{},appended.error};contract->completion=final;contract->pending_code.clear();contract->execution_note_code.clear();task->status="cancelled";task->settlement_id=final.fact_id;task->reported_tick=common.tick();task->late=common.tick()>task->deadline_tick;return {true,std::move(final),{}};
}
// 本域校验不复制钱粮/民心计算；各owner另核权源、资源及计划的严格链接。
Status validate_governance_tasks(const CommonStore& common,const TaskState& tasks,const GovernanceTaskState& governance){
    if(governance.version!=1)return common_failure("UnsupportedGovernanceTaskVersion");std::set<StableId> contract_ids,action_ids,authority_ids,facts;
    for(const auto& row:governance.governance_contracts.rows()){
        const auto& g=row.second;const auto& a=g.acceptance;const auto* task=tasks.tasks.find(row.first);auto valid=source_matches(common,task,g);if(!valid.ok)return valid;
        if(row.first!=a.task_id||a.kind!=GovernanceTaskKind::appease||a.contract_id.empty()||!contract_ids.insert(a.contract_id).second||g.action_id.empty()||!action_ids.insert(g.action_id).second||!facts.insert(a.fact_id).second||a.authority_source_fact_id.empty()||!authority_ids.insert(a.authority_source_fact_id).second||a.organization_id.empty()||a.jurisdiction_id.empty()||a.site_id.empty()||a.rule_id.empty()||a.rule_version.empty()||a.relief_terms_id.empty()||a.reservation_id!=a.contract_id||a.display_name.empty()||a.work_ticks<1||a.accepted_tick<0||a.accepted_tick>common.tick()||a.accepted_revision<0||a.accepted_revision>common.revision()||a.deadline_tick<a.accepted_tick||a.work_ticks>a.deadline_tick-a.accepted_tick||g.last_settlement_attempt_tick< -1||g.last_settlement_attempt_tick>common.tick())return common_failure("InvalidGovernanceContract",row.first);
        if(!task->budget_wallet_id.empty()||task->contract_id||task->offer_ref||task->escort)return common_failure("GovernanceProcurementStateMixed",row.first);
        if(g.work){const auto& w=*g.work;if(w.outcome_plan_id.empty()||!fact_is(common,w.outcome_plan_id,"governance_outcome_plan","politics")||!facts.insert(w.fact_id).second||w.started_tick<a.accepted_tick||w.completed_tick<w.started_tick||w.completed_tick-w.started_tick<a.work_ticks||w.completed_tick>common.tick()||task->objective_completed_tick!=w.completed_tick)return common_failure("InvalidGovernanceWorkFact",row.first);}
        if(g.interruption){const auto& f=*g.interruption;if(g.work||g.completion||!facts.insert(f.fact_id).second||f.stopped_tick<a.accepted_tick||f.stopped_tick>common.tick()||f.completed_work_ticks<0||f.completed_work_ticks>=a.work_ticks||(f.reason!="ExecutorDead"&&f.reason!="PrincipalDissolved")||task->status!="settling"||common.action(g.action_id)||!task->settlement_id.empty()||task->objective_completed_tick||task->reported_tick||task->late||g.pending_code!=f.reason||!g.execution_note_code.empty())return common_failure("InvalidGovernanceInterruptionFact",row.first);continue;}
        if(g.completion){const auto& f=*g.completion;if(!facts.insert(f.fact_id).second||f.completed_tick<a.accepted_tick||f.completed_tick>common.tick()||task->settlement_id!=f.fact_id||task->reported_tick!=f.completed_tick||task->late!=(f.completed_tick>a.deadline_tick)||!g.pending_code.empty()||common.action(g.action_id))return common_failure("InvalidGovernanceCompletionFact",row.first);
            if(g.work){if(f.work_completion_fact_id!=g.work->fact_id||f.outcome_plan_id!=g.work->outcome_plan_id||f.completed_tick<g.work->completed_tick)return common_failure("InvalidGovernanceFinalWorkLink",row.first);}else if(!f.work_completion_fact_id.empty()||!f.outcome_plan_id.empty())return common_failure("PrematureGovernanceFinalWork",row.first);
            if(f.result==GovernanceTaskResult::completed){if(!g.work||f.resource_receipt_ids.size()!=1||f.resource_receipt_ids.front().empty()||task->status!="success"||!f.reason.empty())return common_failure("InvalidGovernanceSuccess",row.first);}
            else if(f.result==GovernanceTaskResult::cancelled){if(task->status!="cancelled"||!f.resource_receipt_ids.empty()||f.reason!="CancelledByAssignee")return common_failure("InvalidGovernanceCancellation",row.first);}
            else return common_failure("UnsupportedGovernanceFailurePolicy",row.first);
        }
        else if(g.work){if(task->status!="ready"||common.action(g.action_id)||!task->settlement_id.empty()||!g.execution_note_code.empty())return common_failure("InvalidGovernanceReadyTask",row.first);}
        else{const auto* action=common.action(g.action_id);const auto* actor=common.actor(a.executor_actor_id);if(task->status!="active"||!action||action->kind!="governance_appease"||action->actor_id!=a.executor_actor_id||action->participants!=std::vector<StableId>{a.executor_actor_id}||action->remaining_ticks<1||action->remaining_ticks>a.work_ticks||!actor||actor->action_id!=action->id||!task->settlement_id.empty()||task->objective_completed_tick||!g.pending_code.empty()||g.last_settlement_attempt_tick!=-1)return common_failure("InvalidGovernanceActiveTask",row.first);}
    }
    return common_success();
}
}
