// 只有原Task的明确作者政策可以产生垫款资格；拒绝或待清算都不另造时间与债务。
#include "political_takeover_execution_world.hpp"
#include "task_advance_integration.hpp"
#include "world_bundle.hpp"
#include "resident/domains/tasks/task_personal_advance_admission.hpp"
#include "resident/domains/tasks/task_personal_advance_rules.hpp"
#include "resident/domains/tasks/task_personal_advance_view.hpp"
namespace hanmo::resident {
namespace {
Status enabled(const WorldBundle& world){
    if(!world.dynamic_enabled()||!world.catalog->task_personal_advances)return common_failure("TaskAdvanceProfileRequired");
    if(!world.tasks.personal_advances)return common_failure("MissingTaskAdvanceDomain");return common_success();
}
}
Outcome<TaskPersonalAdvanceQuote> preview_task_advance_world(const WorldBundle& world,const StableId& actor,const TaskPersonalAdvanceCommand& command){
    auto takeover=PoliticalTakeoverExecutionWorldAccess::create(world);if(!takeover.ok)return {false,{},takeover.error};auto permission=read_task_execution_context(world.common,world.tasks,*world.catalog->tasks,takeover.value->tasks(),command.task_id);if(!permission.ok)return {false,{},permission.error};if(!permission.value.may_progress)return {false,{},common_failure(permission.value.code.empty()?"TaskPoliticalExecutionSuspended":permission.value.code,command.task_id).error};
    auto gate=enabled(world);if(!gate.ok)return {false,{},gate.error};
    return preview_task_personal_advance(world.common,world.tasks,*world.catalog->tasks,*world.tasks.personal_advances,actor,command.task_id,command.amount);
}
Status apply_task_advance_world(WorldBundle& world,const StableId& actor,const TaskPersonalAdvanceCommand& command,const TaskPersonalAdvanceQuote& quote){
    auto takeover=PoliticalTakeoverExecutionWorldAccess::create(world);if(!takeover.ok)return {false,{},takeover.error};auto permission=read_task_execution_context(world.common,world.tasks,*world.catalog->tasks,takeover.value->tasks(),command.task_id);if(!permission.ok)return {false,{},permission.error};if(!permission.value.may_progress)return {false,{},common_failure(permission.value.code.empty()?"TaskPoliticalExecutionSuspended":permission.value.code,command.task_id).error};
    auto gate=enabled(world);if(!gate.ok)return gate;
    auto paid=pay_task_personal_advance_in_candidate(world.common,world.tasks,*world.catalog->tasks,*world.tasks.personal_advances,actor,command.task_id,command.amount,quote);
    return {paid.ok,{},paid.error};
}
Status authorize_task_advance_after_accept(WorldBundle& world,const StableId& action_id){
    if(!world.catalog->task_personal_advances)return common_success();auto gate=enabled(world);if(!gate.ok)return gate;
    const auto* action=world.common.action(action_id);const auto* parameters=world.tasks.actions.find(action_id);
    if(!action||action->kind!="accept_offer"||!parameters)return common_success();
    const auto* task=world.tasks.tasks.find(parameters->task_id);if(!task||!task->contract_id)return common_success();
    auto authorized=authorize_task_personal_advance_in_candidate(world.common,world.tasks,*world.catalog->tasks,*world.catalog->task_personal_advances,*world.tasks.personal_advances,action_id);
    return {authorized.ok,{},authorized.error};
}
Status settle_task_advances_after_clock(WorldBundle& world,const TaskExecutionServices* execution){
    if(!world.catalog->task_personal_advances)return common_success();auto gate=enabled(world);if(!gate.ok)return gate;
    for(const auto& row:world.tasks.personal_advances->terms.rows()){
        const auto* contract=world.tasks.contracts.find(row.first);
        if(!contract)return common_failure("TaskAdvanceSourceMismatch",row.first);
        if(!contract->execution_stopped||contract->status==ContractStatus::settled)continue;
        // 先还本金，再调用原唯一结酬/退预算；晚ID或其它后继失败由整候选原子撤回。
        auto repayment=repay_task_personal_advance_in_candidate(world.common,world.tasks,*world.tasks.personal_advances,&world.organization,row.first);
        if(!repayment.ok)return {false,{},repayment.error};if(repayment.value.kind==TaskAdvanceRepaymentKind::pending)continue;
        auto settled=settle_task_contract(world.common,world.tasks,row.first,world.common.tick(),execution);if(!settled.ok)return settled;
    }
    return common_success();
}
Outcome<BoundaryValue> query_task_advances_world(const WorldBundle& world,Integer cursor){
    auto gate=enabled(world);if(!gate.ok)return {false,{},gate.error};
    return query_task_personal_advances(world.common,world.tasks,*world.catalog->tasks,*world.tasks.personal_advances,cursor);
}
}
