// 只在已停止的原动态合同上执行唯一三腿清算；政治批准不替代金额、货权或完整候选事务。
#include "task_public_account_payment.hpp"
#include "task_public_account_phase.hpp"
#include "task_static_public_payment.hpp"
#include <limits>
#include <map>
namespace hanmo::resident {
namespace {
// 累计前核非负及上界，坏档不能靠有符号溢出隐藏重复清算。
bool add(Integer& total,Integer value){if(value<0||total<0||total>std::numeric_limits<Integer>::max()-value)return false;total+=value;return true;}
// 暂停保持原合同待清状态，由调用方展示原因；不得写收据或动钱包。
Outcome<TaskContractPaymentAttempt> pending(const StableId& code){return {true,{TaskContractPaymentKind::pending,code,{},{}},{}};}
}
Status validate_task_public_account_payments(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const TaskPublicAccountExecutionState& state,const PoliticalTakeoverRead& authority,const DynamicStepInput* phase){
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};
    if(state.version!=1&&state.version!=2)return common_failure("UnsupportedTaskTakeoverExecutionVersion");
    auto static_valid=validate_task_static_payments(common,tasks,catalog,state,authority,phase);if(!static_valid.ok)return static_valid;
    std::map<StableId,TaskContractPaymentAmounts> totals;
    for(const auto& row:state.payments.rows()){
        const auto& payment=row.second;
        if(row.first.empty()||row.first!=payment.fact_id)return common_failure("InvalidTaskContractPaymentSource",row.first);
        const auto* contract=tasks.contracts.find(payment.responsibility_id);
        const auto* task=tasks.tasks.find(payment.task_id);
        if(!contract||!contract->execution_stopped||!task||payment.tick<task->accepted_tick)return common_failure("TaskContractPaymentLifecycleMismatch",row.first);
        auto source=read_task_public_account_payment_authority(common,tasks,catalog,authority,payment,phase);if(!source.ok)return {false,{},source.error};
        auto& total=totals[contract->id];
        for(const auto& leg:payment.legs){Integer* target=nullptr;
            switch(leg.purpose){case TaskContractPaymentPurpose::budget_return:target=&total.budget_return;break;case TaskContractPaymentPurpose::reward_payment:target=&total.reward_payment;break;case TaskContractPaymentPurpose::reward_return:target=&total.reward_return;break;default:return common_failure("UnknownTaskContractPaymentPurpose",row.first);}
            if(!add(*target,leg.amount))return common_failure("TaskContractPaymentOverflow",row.first);
        }
    }
    for(const auto& row:common.state().facts.rows())if(row.second.kind==task_contract_payment_fact_kind&&(row.second.domain!="tasks"||!state.payments.find(row.first)))return common_failure("MissingTaskContractPayment",row.first);
    for(const auto& row:tasks.contracts.rows()){
        const auto& total=totals[row.first];const auto& contract=row.second;
        if(total.budget_return!=contract.refunded_budget||total.reward_payment!=contract.paid_reward||total.reward_return!=contract.refunded_reward)return common_failure("TaskContractPaymentTotalsMismatch",row.first);
    }
    return common_success();
}
Outcome<TaskContractPaymentAttempt> pay_task_contract_in_candidate(CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,TaskPublicAccountExecutionState& state,const PoliticalTakeoverRead& authority,const TaskPersonalAdvanceRead* advances,const StableId& id,const DynamicStepInput* phase){
    auto observed=task_public_account_observed_tick(common,phase);if(!observed.ok)return {false,{},observed.error};
    const auto fail=[](const StableId& code,const StableId& item=StableId{})->Outcome<TaskContractPaymentAttempt>{return {false,{},common_failure(code,item).error};};
    auto valid=validate_task_public_account_payments(common,tasks,catalog,state,authority,phase);if(!valid.ok)return {false,{},valid.error};
    const auto* contract=tasks.contracts.find(id);const auto* task=contract&&contract->current_task_id?tasks.tasks.find(*contract->current_task_id):nullptr;
    if(!contract||!task||task->contract_id!=id)return fail("InvalidTaskContract",id);
    if(contract->status==ContractStatus::settled)return {true,{}, {}};
    if(!contract->execution_stopped)return fail("TaskContractPaymentNotReady",id);
    auto authority_source=read_task_public_account_execution(common,tasks,catalog,&authority,id,phase);if(!authority_source.ok)return {false,{},authority_source.error};
    if(!authority_source.value.may_settle)return pending(authority_source.value.code.empty()?"TaskPoliticalSettlementSuspended":authority_source.value.code);
    const auto& terms=contract->frozen_terms;
    // 本金未归还前不放走原预算或报酬；即使政治批准也不自动改派私人债权。
    if(terms.personal_advance_allowed){
        if(!advances)return fail("MissingTaskAdvanceDomain",id);auto source=advances->task_personal_advance_source(id);if(!source.ok)return {false,{},source.error};
        if(source.value.contract_id!=id||source.value.task_id!=task->id||source.value.paid<source.value.repaid)return fail("TaskAdvanceSourceMismatch",id);
        if(source.value.paid!=source.value.repaid)return pending("TaskAdvanceRepaymentRequired");
    }
    const auto* escrow=common.wallet(task->budget_wallet_id);
    if(!escrow||terms.reward_total<0||terms.purchase_budget_cap<0||contract->paid_reward<0||contract->refunded_reward<0||contract->refunded_budget<0||contract->paid_reward>terms.reward_total||contract->refunded_reward>terms.reward_total-contract->paid_reward||contract->refunded_budget>terms.purchase_budget_cap)return fail("InvalidContractEscrow",id);
    const auto reward_remaining=terms.reward_total-contract->paid_reward-contract->refunded_reward;
    if(escrow->balance<reward_remaining)return fail("ContractEscrowShortfall",id);
    auto reward=task_prorata(terms.reward_total,contract->eligible_delivered_quantity,terms.agreed_quantity);if(!reward.ok)return {false,{},reward.error};
    if(reward.value<contract->paid_reward||reward.value-contract->paid_reward>reward_remaining)return fail("ContractRewardRegression",id);
    TaskContractPaymentAmounts amounts{escrow->balance-reward_remaining,reward.value-contract->paid_reward,0};amounts.reward_return=reward_remaining-amounts.reward_payment;
    if(amounts.budget_return>terms.purchase_budget_cap-contract->refunded_budget)return fail("InvalidContractEscrow",id);
    if(escrow->balance==0)return {true,{TaskContractPaymentKind::no_payment,{}, {},amounts},{}};
    TaskContractPaymentSource payment;payment.responsibility_id=id;payment.task_id=task->id;payment.registration_fact_id=authority_source.value.registration_fact_id;payment.decision_fact_id=authority_source.value.decision_fact_id;payment.tick=observed.value;
    ResourceRecipe recipe;
    // 相同资金/报酬钱包仍保不同角色的收据；实际资源变更由Common一次原子施行。
    const auto leg=[&](TaskContractPaymentPurpose purpose,const StableId& target,Integer amount){if(amount){payment.legs.push_back({purpose,escrow->id,target,amount});recipe.operations.push_back(WalletDelta{target,amount,false});}};
    if(reward_remaining)recipe.operations.push_back(SetReservation{escrow->id,{ReservationRef::Kind::contract,id},0,true});
    leg(TaskContractPaymentPurpose::budget_return,terms.source_wallet_id,amounts.budget_return);
    leg(TaskContractPaymentPurpose::reward_payment,terms.payee_wallet_id,amounts.reward_payment);
    leg(TaskContractPaymentPurpose::reward_return,terms.reward_wallet_id,amounts.reward_return);
    recipe.operations.push_back(WalletDelta{escrow->id,-escrow->balance,false});
    auto fact=common.next_id("task_payment");if(!fact.ok)return {false,{},fact.error};payment.fact_id=fact.value;
    auto changed=common.apply_resources(recipe);if(!changed.ok)return {false,{},changed.error};
    auto recorded=common.append_fact({fact.value,task_contract_payment_fact_kind,"tasks"});if(!recorded.ok)return {false,{},recorded.error};
    if(!state.payments.insert(fact.value,std::move(payment)))return fail("DuplicateTaskContractPayment",fact.value);
    return {true,{TaskContractPaymentKind::paid,{},fact.value,amounts},{}};
}
Status validate_task_public_account_execution_layout(const CommonStore& common,bool enabled,const std::vector<std::pair<StableId,StableId>>& modules,const TaskPublicAccountExecutionState* state){
    const StableId* marker=nullptr;for(const auto& row:modules)if(row.first=="task_takeover_execution"){if(marker)return common_failure("DuplicateTaskTakeoverExecutionMarker");marker=&row.second;}
    if(!enabled){
        if(marker||state)return common_failure("UnexpectedTaskTakeoverExecutionDomain");
        for(const auto& row:common.state().facts.rows())if(row.second.kind==task_contract_payment_fact_kind||row.second.kind==task_static_payment_fact_kind)return common_failure("TaskTakeoverExecutionProfileRequired",row.first);
        return common_success();
    }
    if(!marker)return common_failure("MissingTaskTakeoverExecutionMarker");if(*marker!="1"&&*marker!="2")return common_failure("UnsupportedTaskTakeoverExecutionVersion");
    if(!state)return common_failure("MissingTaskTakeoverExecutionDomain");if(std::to_string(state->version)!=*marker)return common_failure("TaskTakeoverExecutionVersionMismatch");
    if(state->version==1){if(!state->static_payments.empty())return common_failure("UnexpectedTaskStaticPaymentDomain");for(const auto& row:common.state().facts.rows())if(row.second.kind==task_static_payment_fact_kind)return common_failure("TaskStaticPaymentProfileRequired",row.first);}return common_success();
}
}
