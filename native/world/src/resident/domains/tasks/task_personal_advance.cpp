// 实际私人支出和原任务托管入账是同一ResourceRecipe；没有凭空经费或隐式免债。
#include "task_personal_advance_rules.hpp"
#include "task_execution_gate.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
bool identity_equal(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
Status failure(const StableId& code,const StableId& id={}){return common_failure(code,id);}
Outcome<Integer> task_advance_calendar_tick(const CommonStore& common,const DynamicStepInput* phase){if(!phase)return {true,common.tick(),{}};if(common.tick()==std::numeric_limits<Integer>::max()||phase->from_tick!=common.tick()||phase->to_tick!=common.tick()+1)return {false,{},failure("TaskAdvanceCalendarPhaseMismatch").error};return {true,phase->to_tick,{}};}
// 全部来源都必须仍指向原冻结合同；Owner只用于当前实际付款许可，不改写原issuer。
Status terms_match(const CommonStore& common,const TaskState& tasks,const TaskPersonalAdvanceTerms& terms,Integer observed_tick){
    const auto* contract=tasks.contracts.find(terms.contract_id);const auto* task=tasks.tasks.find(terms.task_id);
    const auto* offer=task?tasks.offer_instances.find(task->offer_id):nullptr;
    if(!contract||!task||contract->id!=terms.contract_id||task->id!=terms.task_id||task->contract_id!=terms.contract_id||
       !offer||offer->template_id!=terms.policy.task_template_id||
       contract->current_task_id!=terms.task_id||contract->source_binding.claim_id!=terms.claim_id||
       contract->frozen_terms.terms_version!=terms.terms_version||!contract->frozen_terms.personal_advance_allowed||
       contract->frozen_terms.definition.template_id!="procurement"||terms.policy.template_id!="procurement"||
       !owner_equal(contract->issuer,terms.original_issuer)||contract->assignee.kind!=SubjectKind::actor||
       contract->assignee.id!=terms.lender_actor_id||contract->executor_actor_id!=terms.lender_actor_id||
       task->assignee!=terms.lender_actor_id||task->budget_wallet_id!=terms.escrow_wallet_id||
       contract->frozen_terms.source_wallet_id!=terms.repayment_wallet_id||terms.frozen_tick<0||terms.frozen_tick>observed_tick)return failure("TaskAdvanceSourceMismatch",terms.contract_id);
    const auto* actor=common.actor(terms.lender_actor_id);
    if(!actor||actor->wallet_id!=terms.personal_wallet_id||terms.personal_wallet_id.empty()||terms.escrow_wallet_id.empty()||
       terms.repayment_wallet_id.empty()||terms.personal_wallet_id==terms.escrow_wallet_id||
       terms.personal_wallet_id==terms.repayment_wallet_id||terms.escrow_wallet_id==terms.repayment_wallet_id)return failure("TaskAdvanceWalletBindingMismatch",terms.contract_id);
    return validate_task_personal_advance_policy(terms.policy);
}
}
Status validate_task_personal_advance_policy(const TaskPersonalAdvancePolicy& policy){
    if(policy.id.empty()||policy.task_template_id.empty()||policy.version!=1||policy.template_id!="procurement"||policy.maximum_cash<1||policy.maximum_cash>2147483647||policy.repayment_policy!="authorized_advance_first_v1")return failure("InvalidTaskAdvancePolicy",policy.id);
    return common_success();
}
// 整域核账必须双向覆盖Common事实，不能删掉整份垫款或插入无条款的还款行。
Status validate_task_personal_advance_state(const CommonStore& common,const TaskState& tasks,const TaskPersonalAdvanceState& state){
    if(state.version!=1)return failure("UnsupportedTaskAdvanceVersion");
    for(const auto& row:tasks.contracts.rows())if(row.second.frozen_terms.personal_advance_allowed&&!state.terms.find(row.first))return failure("MissingTaskAdvanceTerms",row.first);
    std::set<StableId> task_ids;
    for(const auto& row:state.terms.rows()){
        if(row.first!=row.second.contract_id||row.first.empty()||!task_ids.insert(row.second.task_id).second)return failure("InvalidTaskAdvanceTerms",row.first);
        auto summary=read_task_personal_advance_summary(common,tasks,state,row.first);if(!summary.ok)return {false,{},summary.error};
        const auto* contract=tasks.contracts.find(row.first);
        if(contract&&contract->status==ContractStatus::settled&&summary.value.paid!=summary.value.repaid)return failure("UnpaidTaskAdvanceSettlement",row.first);
    }
    for(const auto& row:state.payments.rows())if(row.first!=row.second.fact_id||!state.terms.find(row.second.contract_id))return failure("OrphanTaskAdvancePayment",row.first);
    for(const auto& row:state.repayments.rows())if(row.first!=row.second.fact_id||!state.terms.find(row.second.contract_id))return failure("OrphanTaskAdvanceRepayment",row.first);
    for(const auto& row:common.state().facts.rows()){
        if(row.second.kind==task_advance_fact_kind&&(row.second.domain!="tasks"||!state.payments.find(row.first)))return failure("OrphanTaskAdvanceFact",row.first);
        if(row.second.kind==task_advance_repayment_fact_kind&&(row.second.domain!="tasks"||!state.repayments.find(row.first)))return failure("OrphanTaskAdvanceRepaymentFact",row.first);
    }
    return common_success();
}
Outcome<TaskPersonalAdvanceTerms> freeze_task_personal_advance_terms(const CommonStore& common,const TaskState& tasks,const StableId& id,const TaskPersonalAdvancePolicy& policy){
    auto valid=validate_task_personal_advance_policy(policy);if(!valid.ok)return {false,{},valid.error};
    const auto* contract=tasks.contracts.find(id);const auto* task=contract&&contract->current_task_id?tasks.tasks.find(*contract->current_task_id):nullptr;
    if(!contract||!task||task->status!="accepting"||contract->status!=ContractStatus::accepted||contract->execution_stopped||contract->assignee.kind!=SubjectKind::actor)return {false,{},failure("TaskAdvanceAcceptanceRequired",id).error};
    const auto* actor=common.actor(task->assignee);if(!actor)return {false,{},failure("UnknownTaskAssignee",id).error};
    TaskPersonalAdvanceTerms terms;terms.contract_id=id;terms.task_id=task->id;terms.claim_id=contract->source_binding.claim_id;terms.terms_version=contract->frozen_terms.terms_version;terms.policy=policy;terms.lender_actor_id=task->assignee;terms.personal_wallet_id=actor->wallet_id;terms.escrow_wallet_id=task->budget_wallet_id;terms.repayment_wallet_id=contract->frozen_terms.source_wallet_id;terms.original_issuer=contract->issuer;terms.frozen_tick=common.tick();
    valid=terms_match(common,tasks,terms,common.tick());if(!valid.ok)return {false,{},valid.error};return {true,std::move(terms),{}};
}
Outcome<TaskPersonalAdvanceSummary> read_task_personal_advance_summary(const CommonStore& common,const TaskState& tasks,const TaskPersonalAdvanceState& state,const StableId& id,const DynamicStepInput* phase){
    auto clock=task_advance_calendar_tick(common,phase);if(!clock.ok)return {false,{},clock.error};const auto observed_tick=clock.value;
    if(state.version!=1)return {false,{},failure("UnsupportedTaskAdvanceVersion").error};
    const auto* terms=state.terms.find(id);if(!terms)return {false,{},failure("TaskAdvanceNotAuthorized",id).error};if(terms->contract_id!=id)return {false,{},failure("TaskAdvanceSourceMismatch",id).error};
    auto valid=terms_match(common,tasks,*terms,observed_tick);if(!valid.ok)return {false,{},valid.error};
    TaskPersonalAdvanceSummary out;out.contract_id=id;out.task_id=terms->task_id;out.lender_actor_id=terms->lender_actor_id;out.personal_wallet_id=terms->personal_wallet_id;
    Integer latest_payment_tick=terms->frozen_tick;std::set<StableId> seen;
    // 同半日也必须先有真实付款、后有偿还；tick相等不能抹去Common事实追加顺序。
    const auto& fact_order=common.state().facts.order();std::size_t latest_payment_sequence=0;
    for(const auto& row:state.payments.rows())if(row.second.contract_id==id){const auto& payment=row.second;const auto* fact=common.state().facts.find(row.first);
        if(row.first!=payment.fact_id||!fact||fact->id!=row.first||fact->kind!=task_advance_fact_kind||fact->domain!="tasks"||
           payment.task_id!=terms->task_id||payment.lender_actor_id!=terms->lender_actor_id||payment.from_wallet_id!=terms->personal_wallet_id||payment.to_wallet_id!=terms->escrow_wallet_id||
           payment.tick<terms->frozen_tick||payment.tick>observed_tick||payment.amount<1||payment.amount>terms->policy.maximum_cash-out.paid||!seen.insert(row.first).second)
            return {false,{},failure("InvalidTaskAdvancePayment",row.first).error};
        out.paid+=payment.amount;out.payment_fact_ids.push_back(row.first);latest_payment_tick=std::max(latest_payment_tick,payment.tick);
        const auto sequence=std::find(fact_order.begin(),fact_order.end(),row.first);
        if(sequence==fact_order.end())return {false,{},failure("InvalidTaskAdvancePayment",row.first).error};
        latest_payment_sequence=std::max(latest_payment_sequence,static_cast<std::size_t>(sequence-fact_order.begin()));
    }
    for(const auto& row:state.repayments.rows())if(row.second.contract_id==id){const auto& repayment=row.second;const auto* fact=common.state().facts.find(row.first);
        if(out.repayment_fact_id||row.first!=repayment.fact_id||!fact||fact->id!=row.first||fact->kind!=task_advance_repayment_fact_kind||fact->domain!="tasks"||
           repayment.task_id!=terms->task_id||repayment.lender_actor_id!=terms->lender_actor_id||repayment.to_wallet_id!=terms->personal_wallet_id||repayment.escrow_wallet_id!=terms->escrow_wallet_id||repayment.issuer_wallet_id!=terms->repayment_wallet_id||
           repayment.payment_fact_ids!=out.payment_fact_ids||repayment.amount!=out.paid||repayment.amount<1||repayment.from_escrow<0||repayment.from_issuer<0||repayment.from_escrow>repayment.amount||repayment.from_issuer!=repayment.amount-repayment.from_escrow||repayment.tick<latest_payment_tick||repayment.tick>observed_tick)return {false,{},failure("InvalidTaskAdvanceRepayment",row.first).error};
        const auto sequence=std::find(fact_order.begin(),fact_order.end(),row.first);
        if(sequence==fact_order.end()||static_cast<std::size_t>(sequence-fact_order.begin())<=latest_payment_sequence)return {false,{},failure("TaskAdvanceRepaymentPrecedesPayment",row.first).error};
        const auto* contract=tasks.contracts.find(id);if(!contract||!contract->execution_stopped)return {false,{},failure("PrematureTaskAdvanceRepayment",row.first).error};out.repayment_fact_id=row.first;out.repaid=repayment.amount;
    }
    return {true,std::move(out),{}};
}
Outcome<TaskPersonalAdvanceQuote> preview_task_personal_advance(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const TaskPersonalAdvanceState& state,const StableId& actor_id,const StableId& task_id,Integer amount){
    const auto* task=tasks.tasks.find(task_id);if(!task||task->assignee!=actor_id||!task->contract_id)return {false,{},failure("TaskNotKnown").error};
    const auto* contract=tasks.contracts.find(*task->contract_id);const auto* terms=state.terms.find(*task->contract_id);
    if(!contract||!terms)return {false,{},failure("TaskAdvanceNotAuthorized").error};
    auto summary=read_task_personal_advance_summary(common,tasks,state,contract->id);if(!summary.ok)return {false,{},summary.error};
    const auto* actor=common.actor(actor_id);if(!actor||!actor->alive||!actor->available||!actor->action_id.empty())return {false,{},failure("TaskAdvanceActorUnavailable").error};
    if(task->status!="active"||contract->status!=ContractStatus::active||contract->execution_stopped||summary.value.repayment_fact_id||!task_procurement_authorized_now(common,tasks,catalog,actor_id,task_id,contract->frozen_terms.item_id))return {false,{},failure("TaskAdvanceContractClosed",task_id).error};
    if(amount<1||amount>terms->policy.maximum_cash-summary.value.paid)return {false,{},failure("TaskAdvanceLimitExceeded",task_id).error};
    const auto* personal=common.wallet(terms->personal_wallet_id);const auto* escrow=common.wallet(terms->escrow_wallet_id);const auto* issuer=common.wallet(terms->repayment_wallet_id);
    if(!personal||personal->owner.kind!=Owner::Kind::actor||personal->owner.id!=actor_id||!escrow||escrow->owner.kind!=Owner::Kind::task||escrow->owner.id!=task_id||!issuer||!owner_equal(issuer->owner,terms->original_issuer))return {false,{},failure("TaskAdvanceWalletAuthorityChanged",task_id).error};
    if(escrow->balance>std::numeric_limits<Integer>::max()-amount)return {false,{},failure("TaskAdvanceOverflow",task_id).error};
    auto cash=common.spendable(personal->id);if(!cash.ok)return {false,{},cash.error};if(cash.value<amount)return {false,{},failure("InsufficientTaskAdvanceFunds",task_id).error};
    TaskPersonalAdvanceQuote quote;quote.source_identity=common.state().identity;quote.contract_id=contract->id;quote.task_id=task_id;quote.actor_id=actor_id;quote.from_wallet_id=personal->id;quote.to_wallet_id=escrow->id;quote.amount=amount;quote.previous_total=summary.value.paid;quote.maximum_cash=terms->policy.maximum_cash;
    quote.resource_versions={common.read_version({ResourceKey::Kind::wallet,personal->id}),common.read_version({ResourceKey::Kind::wallet,escrow->id}),common.read_version({ResourceKey::Kind::wallet,issuer->id})};return {true,std::move(quote),{}};
}
Outcome<StableId> pay_task_personal_advance_in_candidate(CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,TaskPersonalAdvanceState& state,const StableId& actor,const StableId& task_id,Integer amount,const TaskPersonalAdvanceQuote& quoted){
    if(!identity_equal(common.state().identity,quoted.source_identity))return {false,{},failure("StaleTaskAdvanceQuote").error};
    auto checked=preview_task_personal_advance(common,tasks,catalog,state,actor,task_id,amount);if(!checked.ok)return {false,{},checked.error};const auto& current=checked.value;
    if(quoted.contract_id!=current.contract_id||quoted.task_id!=current.task_id||quoted.actor_id!=current.actor_id||quoted.from_wallet_id!=current.from_wallet_id||quoted.to_wallet_id!=current.to_wallet_id||quoted.amount!=current.amount||quoted.previous_total!=current.previous_total||quoted.maximum_cash!=current.maximum_cash)return {false,{},failure("StaleTaskAdvanceQuote").error};
    if(quoted.resource_versions.size()!=current.resource_versions.size())return {false,{},failure("StaleTaskAdvanceQuote").error};
    for(std::size_t index=0;index<current.resource_versions.size();++index){const auto& a=quoted.resource_versions[index];const auto& b=current.resource_versions[index];if(a.resource.kind!=b.resource.kind||a.resource.id!=b.resource.id||a.version!=b.version)return {false,{},failure("StaleTaskAdvanceQuote").error};}
    const auto id=common.next_id("task_advance");if(!id.ok)return id;
    ResourceRecipe recipe;recipe.expected_versions=current.resource_versions;recipe.operations={WalletDelta{current.from_wallet_id,-amount,true},WalletDelta{current.to_wallet_id,amount,false}};
    auto paid=common.apply_resources(recipe);if(!paid.ok)return {false,{},paid.error};auto recorded=common.append_fact({id.value,task_advance_fact_kind,"tasks"});if(!recorded.ok)return {false,{},recorded.error};
    TaskPersonalAdvancePayment receipt{id.value,current.contract_id,task_id,actor,current.from_wallet_id,current.to_wallet_id,amount,common.tick()};state.payments.insert(id.value,std::move(receipt));return id;
}
Outcome<TaskAdvanceSettlementAmounts> calculate_task_advance_settlement(Integer balance,Integer reserved,Integer original,Integer principal){
    if(balance<0||reserved<0||reserved>balance||original<0||principal<0)return {false,{},failure("InvalidTaskAdvanceSettlement").error};
    const auto free=balance-reserved;const auto from_escrow=std::min(free,principal),unused=free-from_escrow;
    if(unused>original)return {false,{},failure("TaskAdvanceEscrowMismatch").error};
    return {true,{principal,from_escrow,principal-from_escrow,unused},{}};
}
}

namespace hanmo::resident {
// 只向原出资人的原钱包偿还已付本金；原组织的普通职权变化不改变这项历史责任。
Outcome<TaskAdvanceRepaymentAttempt> repay_task_personal_advance_in_candidate(CommonStore& common,const TaskState& tasks,TaskPersonalAdvanceState& state,const OrganizationState* organizations,const StableId& id,const DynamicStepInput* phase){
    auto clock=task_advance_calendar_tick(common,phase);if(!clock.ok)return {false,{},clock.error};
    using Result=Outcome<TaskAdvanceRepaymentAttempt>;
    const auto pending=[](const StableId& code)->Result{return {true,{TaskAdvanceRepaymentKind::pending,code,{}},{}};};
    const auto* terms=state.terms.find(id);if(!terms)return {false,{},common_failure("TaskAdvanceNotAuthorized",id).error};
    auto summary=read_task_personal_advance_summary(common,tasks,state,id,phase);if(!summary.ok)return {false,{},summary.error};
    if(summary.value.repayment_fact_id)return {true,{TaskAdvanceRepaymentKind::already_repaid,{},summary.value.repayment_fact_id},{}};
    if(summary.value.paid==0)return {true,{TaskAdvanceRepaymentKind::no_advance,{},{}},{}};
    const auto* contract=tasks.contracts.find(id);const auto* task=tasks.tasks.find(terms->task_id);
    if(!contract||!task||!contract->execution_stopped||contract->status==ContractStatus::settled)return {false,{},common_failure("TaskAdvanceSettlementNotReady",id).error};
    const auto* lender=common.actor(terms->lender_actor_id);if(!lender||!lender->alive)return pending("TaskAdvanceLenderSuccessionRequired");
    if(terms->original_issuer.kind==Owner::Kind::actor){const auto* issuer=common.actor(terms->original_issuer.id);if(!issuer)return {false,{},common_failure("UnknownTaskIssuer",id).error};if(!issuer->alive)return pending("TaskAdvanceIssuerSuccessionRequired");}
    else if(terms->original_issuer.kind==Owner::Kind::organization){const auto* issuer=organizations?organizations->organizations.find(terms->original_issuer.id):nullptr;if(!issuer)return {false,{},common_failure("TaskAdvanceOrganizationSourceRequired",id).error};if(issuer->status==OrganizationStatus::dissolved)return pending("TaskAdvanceIssuerSuccessionRequired");if(issuer->status!=OrganizationStatus::active&&issuer->status!=OrganizationStatus::vacant&&issuer->status!=OrganizationStatus::limited)return {false,{},common_failure("InvalidOrganizationStatus",issuer->id).error};}
    else return {false,{},common_failure("InvalidTaskAdvanceIssuer",id).error};
    const auto* escrow=common.wallet(terms->escrow_wallet_id);const auto* issuer=common.wallet(terms->repayment_wallet_id);const auto* personal=common.wallet(terms->personal_wallet_id);
    if(!escrow||!issuer||!personal)return {false,{},common_failure("MissingTaskAdvanceWallet",id).error};
    if(escrow->owner.kind!=Owner::Kind::task||escrow->owner.id!=task->id)return {false,{},common_failure("TaskEscrowOwnerMismatch",id).error};
    if(!owner_equal(issuer->owner,terms->original_issuer)||personal->owner.kind!=Owner::Kind::actor||personal->owner.id!=terms->lender_actor_id)return pending("TaskAdvanceWalletAuthorityChanged");
    const auto& frozen=contract->frozen_terms;
    if(contract->paid_reward<0||contract->refunded_reward<0||contract->paid_reward>frozen.reward_total||contract->refunded_reward>frozen.reward_total-contract->paid_reward||contract->refunded_budget<0||contract->refunded_budget>frozen.purchase_budget_cap)return {false,{},common_failure("InvalidTaskAdvanceSettlement",id).error};
    auto amounts=calculate_task_advance_settlement(escrow->balance,frozen.reward_total-contract->paid_reward-contract->refunded_reward,frozen.purchase_budget_cap-contract->refunded_budget,summary.value.paid);
    if(!amounts.ok)return {false,{},amounts.error};
    auto free_escrow=common.spendable(escrow->id),free_issuer=common.spendable(issuer->id);
    if(!free_escrow.ok)return {false,{},free_escrow.error};if(!free_issuer.ok)return {false,{},free_issuer.error};
    if(free_escrow.value<amounts.value.from_escrow||free_issuer.value<amounts.value.from_issuer)return pending("TaskAdvanceRepaymentFunds");
    if(personal->balance>std::numeric_limits<Integer>::max()-summary.value.paid)return pending("TaskAdvanceRecipientCapacity");
    ResourceRecipe recipe;recipe.expected_versions={common.read_version({ResourceKey::Kind::wallet,escrow->id}),common.read_version({ResourceKey::Kind::wallet,issuer->id}),common.read_version({ResourceKey::Kind::wallet,personal->id})};
    if(amounts.value.from_escrow)recipe.operations.push_back(WalletDelta{escrow->id,-amounts.value.from_escrow,true});
    if(amounts.value.from_issuer)recipe.operations.push_back(WalletDelta{issuer->id,-amounts.value.from_issuer,true});
    recipe.operations.push_back(WalletDelta{personal->id,summary.value.paid,false});
    auto fact=common.next_id("task_advance_repayment");if(!fact.ok)return {false,{},fact.error};auto transferred=common.apply_resources(recipe);if(!transferred.ok)return {false,{},transferred.error};
    auto recorded=common.append_fact({fact.value,task_advance_repayment_fact_kind,"tasks"});if(!recorded.ok)return {false,{},recorded.error};
    TaskPersonalAdvanceRepayment receipt;receipt.fact_id=fact.value;receipt.contract_id=id;receipt.task_id=task->id;receipt.lender_actor_id=terms->lender_actor_id;receipt.to_wallet_id=terms->personal_wallet_id;receipt.escrow_wallet_id=terms->escrow_wallet_id;receipt.issuer_wallet_id=terms->repayment_wallet_id;receipt.payment_fact_ids=summary.value.payment_fact_ids;receipt.amount=summary.value.paid;receipt.from_escrow=amounts.value.from_escrow;receipt.from_issuer=amounts.value.from_issuer;receipt.tick=clock.value;state.repayments.insert(fact.value,std::move(receipt));
    return {true,{TaskAdvanceRepaymentKind::repaid,{},fact.value},{}};
}
}
