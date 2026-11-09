// 静态成功奖励的剩余预算和另付报酬分腿保存；失败/取消只按原规则退库，货权仍由旧规则处理。
#include "task_static_public_payment.hpp"
#include "task_public_account_phase.hpp"
#include <limits>
#include <map>
namespace hanmo::resident {
Outcome<bool> task_static_payment_responsibility_supported(const TaskState& tasks,const TaskCatalog& catalog,const StableId& id){
    const auto* task=tasks.tasks.find(id);if(!task)return {false,{},common_failure("UnknownTaskResponsibility",id).error};
    if(task->contract_id)return {true,false,{}};
    if(!tasks.takeover_execution||tasks.takeover_execution->version!=2)return {true,false,{}};
    const auto* definition=catalog.offers.find(task->offer_id);
    if(!definition)return {false,{},common_failure("UnknownOffer",task->offer_id).error};
    if(definition->template_id!="procurement"&&definition->template_id!="message"&&definition->template_id!="escort")return {true,false,{}};
    if(task->id!=id||definition->id!=task->offer_id||definition->budget<0||definition->reward<0||definition->deadline_ticks<0||definition->funding_wallet.empty())return {false,{},common_failure("InvalidTaskStaticPaymentSource",id).error};
    return {true,true,{}};
}
Outcome<TaskStaticPaymentPreparation> prepare_task_static_payment(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const TaskPublicAccountExecutionState& state,const PoliticalTakeoverRead& authority,const StableId& id,const StableId& command,const ActionCoreView* action,Integer tick,const DynamicStepInput* phase){
    const auto fail=[](const StableId& code,const StableId& id=StableId{})->Outcome<TaskStaticPaymentPreparation>{return {false,{},common_failure(code,id).error};};
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};if(tick!=observed.value)return fail("TaskStaticPaymentTickMismatch",id);
    auto supported=task_static_payment_responsibility_supported(tasks,catalog,id);if(!supported.ok)return {false,{},supported.error};
    if(!supported.value||&state!=&*tasks.takeover_execution)return fail("TaskStaticPaymentProfileRequired",id);
    const auto& task=*tasks.tasks.find(id);const auto& definition=*catalog.offers.find(task.offer_id);const auto* actor=common.actor(task.assignee);const auto* escrow=common.wallet(task.budget_wallet_id);
    if(!actor||!escrow||escrow->owner.kind!=Owner::Kind::task||escrow->owner.id!=id||escrow->balance<0||escrow->balance>definition.budget)return fail("InvalidTaskStaticPaymentSource",id);
    auto permission=read_task_public_account_execution(common,tasks,catalog,&authority,id,phase);if(!permission.ok)return {false,{},permission.error};
    if(command=="cancel_task"?!permission.value.may_settle:!permission.value.may_progress)return fail(permission.value.code.empty()?"TaskPoliticalExecutionSuspended":permission.value.code,id);
    TaskStaticPaymentPreparation out;auto& receipt=out.receipt;receipt.task_id=id;receipt.registration_fact_id=permission.value.registration_fact_id;receipt.decision_fact_id=permission.value.decision_fact_id;receipt.tick=tick;
    const TaskAction* params=nullptr;
    if(command=="accept_offer"||command=="report_offer"){
        params=action?tasks.actions.find(action->id):nullptr;const auto* live=action?common.action(action->id):nullptr;
        if(!action||!live||live->kind!=command||action->kind!=command||action->actor_id!=task.assignee||live->actor_id!=task.assignee||!params||params->task_id!=id||params->offer_id!=task.offer_id)return fail("InvalidTaskStaticPaymentAction",id);
        receipt.source_action_id=action->id;
    }else if(command!="cancel_task"||action)return fail("UnsupportedTaskStaticPaymentCommand",command);
    auto add=[&](TaskStaticPaymentPurpose purpose,const StableId& from,const StableId& to,Integer amount){if(amount)receipt.legs.push_back({purpose,from,to,amount});};
    if(command=="accept_offer"){
        if(task.status!="accepting"||task.accepted_tick!=0||task.deadline_tick!=0||escrow->balance!=0||!task.settlement_id.empty())return fail("TaskStaticPaymentLifecycleMismatch",id);
        receipt.stage=TaskStaticPaymentStage::acceptance;add(TaskStaticPaymentPurpose::funding,definition.funding_wallet,escrow->id,definition.budget);
    }else{
        if(task.status!="active"&&task.status!="ready")return fail("TaskStaticPaymentLifecycleMismatch",id);
        if(command=="cancel_task"){
            receipt.stage=TaskStaticPaymentStage::cancellation;add(TaskStaticPaymentPurpose::budget_return,escrow->id,definition.funding_wallet,escrow->balance);
        }else{
            const bool failed=definition.template_id=="escort"&&task.escort&&!task.escort->failure.empty();const auto reward=params->reward.value_or(0);
            const auto expected=failed?0:(tick>task.deadline_tick?definition.reward/2:definition.reward);
            if(!params->reward||reward!=expected)return fail("TaskStaticPaymentRewardMismatch",id);
            receipt.stage=failed?TaskStaticPaymentStage::report_failure:TaskStaticPaymentStage::report_success;
            add(failed?TaskStaticPaymentPurpose::budget_return:TaskStaticPaymentPurpose::unused_budget_award,escrow->id,failed?definition.funding_wallet:actor->wallet_id,escrow->balance);
            add(TaskStaticPaymentPurpose::reward_payment,definition.funding_wallet,actor->wallet_id,reward);
        }
    }
    for(const auto& row:state.static_payments.rows())if(row.second.task_id==id&&(receipt.stage==TaskStaticPaymentStage::acceptance?row.second.stage==receipt.stage:row.second.stage!=TaskStaticPaymentStage::acceptance))return fail("DuplicateTaskStaticPayment",id);
    // 先合并同钱包各腿，再核上下界；一份付款计划不允许自付、负额或预算/报酬整型溢出。
    std::map<StableId,TaskStaticWalletResult> expected;
    for(const auto& leg:receipt.legs){
        if(leg.amount<=0||leg.from_wallet_id==leg.to_wallet_id)return fail("InvalidTaskStaticPaymentLeg",id);
        for(const auto& side:{std::make_pair(leg.from_wallet_id,-leg.amount),std::make_pair(leg.to_wallet_id,leg.amount)}){
            const auto* wallet=common.wallet(side.first);if(!wallet)return fail("UnknownWallet",side.first);
            auto [at,inserted]=expected.emplace(side.first,TaskStaticWalletResult{side.first,wallet->balance,wallet->balance});auto& amount=at->second.after;
            if(side.second<0?amount< -side.second:amount>std::numeric_limits<Integer>::max()-side.second)return fail("TaskStaticPaymentBalanceBounds",side.first);
            amount+=side.second;out.cash_recipe.operations.push_back(WalletDelta{side.first,side.second,false});
        }
    }
    for(auto& row:expected)out.wallets.push_back(std::move(row.second));return {true,std::move(out),{}};
}
Status record_task_static_payment_after_resources(CommonStore& common,TaskPublicAccountExecutionState& state,const TaskStaticPaymentPreparation& prepared){
    if(state.version!=2||!prepared.receipt.fact_id.empty())return common_failure("TaskStaticPaymentProfileRequired");
    for(const auto& row:prepared.wallets){const auto* actual=common.wallet(row.wallet_id);if(!actual||actual->balance!=row.after)return common_failure("TaskStaticPaymentResourceMismatch",row.wallet_id);}
    for(const auto& row:state.static_payments.rows())if(row.second.task_id==prepared.receipt.task_id&&(prepared.receipt.stage==TaskStaticPaymentStage::acceptance?row.second.stage==prepared.receipt.stage:row.second.stage!=TaskStaticPaymentStage::acceptance))return common_failure("DuplicateTaskStaticPayment",prepared.receipt.task_id);
    auto id=common.next_id("task_static_payment");if(!id.ok)return {false,{},id.error};auto receipt=prepared.receipt;receipt.fact_id=id.value;
    auto recorded=common.append_fact({id.value,task_static_payment_fact_kind,"tasks"});if(!recorded.ok)return recorded;
    if(!state.static_payments.insert(id.value,std::move(receipt)))return common_failure("DuplicateTaskStaticPayment",id.value);return common_success();
}
}
