// 接管付款仍写原PaymentReceipt/ArrearRecord；单一ResourceRecipe保持钱的守恒。
#include "resident/storage/money_transfer_recipe.hpp"
#include "execution_receipts.hpp"
#include "execution_materials.hpp"
#include "execution_configurations.hpp"
#include "../local_economy_internal.hpp"
namespace hanmo::resident {
namespace {
using Result=Outcome<EconomicTakenPaymentResult>;
Result fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
bool subject(const SubjectRef& a,const SubjectRef& b){return a.kind==b.kind&&a.id==b.id;}
bool contains(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
bool fact_before(const CommonStore& c,const StableId& first,const StableId& second){bool seen=false;for(const auto& f:c.state().facts.rows()){if(f.first==second)return seen;if(f.first==first)seen=true;}return false;}
bool same_registration(const PoliticalTakeoverRegistrationSource& r,const PoliticalTakeoverSource& s){return r.occupation_fact_id==s.occupation_fact_id&&r.jurisdiction_id==s.jurisdiction_id&&r.site_id==s.site_id&&r.former_organization_id==s.former_organization_id&&r.successor_organization_id==s.successor_organization_id&&r.occupation_tick==s.occupation_tick&&r.public_wallet_ids==s.public_wallet_ids&&r.public_inventory_ids==s.public_inventory_ids;}
}
Status validate_economic_taken_payment_source(const EconomicTakeoverReadContext& x,const EconomicTakeoverExecutionState& state,const StableId& id){
    if(state.version!=1)return common_failure("EconomicTakeoverExecutionVersionMismatch");
    const auto* link=state.payments.find(id);const auto* payment=x.economy.payments.find(id);
    const auto* debt=payment?x.economy.arrears.find(payment->arrear_id):nullptr;const auto* obligation=debt?x.economy.obligations.find(debt->obligation_id):nullptr;const auto* fact=x.common.state().facts.find(id);
    if(!link||!payment||!debt||!obligation||link->receipt_id!=id||payment->id!=id||link->arrear_id!=debt->id||payment->obligation_id!=obligation->id||payment->authority_ref!=link->decision_fact_id||payment->authority_ref==obligation->authority_ref||payment->from_wallet_id!=obligation->source_wallet_or_inventory||payment->to_wallet_id!=obligation->destination||payment->due_tick!=debt->due_tick||payment->amount<=0||payment->amount>debt->original_due||payment->tick<debt->due_tick||payment->tick>std::max(x.common.tick(),x.economy.last_step_tick)||!fact||fact->kind!="obligation_payment"||fact->domain!="local_economy")return common_failure("InvalidEconomicTakenPayment",id);
    const bool ongoing=link->scope==EconomicTakenPaymentScope::subsequent_obligation_period;
    if(!ongoing&&link->scope!=EconomicTakenPaymentScope::captured_arrear)return common_failure("UnknownEconomicTakenPaymentScope",id);
    const PoliticalResponsibilityRef ref{ongoing?PoliticalResponsibilityKind::obligation:PoliticalResponsibilityKind::arrear,ongoing?obligation->id:debt->id,{},{}};
    auto boundary=x.politics.political_responsibility_before_fact({PoliticalResponsibilityKind::arrear,debt->id,{},{}},id,payment->tick,x.common.state().identity);if(!boundary.ok)return {false,{},boundary.error};
    if(ongoing){if(boundary.value)return common_failure("EconomicCapturedArrearDecisionRequired",id);boundary=x.politics.political_responsibility_before_fact(ref,id,payment->tick,x.common.state().identity);if(!boundary.ok)return {false,{},boundary.error};}
    if(!boundary.value||boundary.value->status!=PoliticalResponsibilityExecutionStatus::approved||!boundary.value->decision||boundary.value->registration_fact_id!=link->registration_fact_id||boundary.value->decision->takeover_fact_id!=link->approval_fact_id||boundary.value->decision->decision_fact_id!=link->decision_fact_id)return common_failure("EconomicTakenPaymentDecisionNotEffective",id);
    const auto& registration=boundary.value->registration;
    if(registration.registration_fact_id!=link->registration_fact_id||!contains(registration.public_wallet_ids,payment->from_wallet_id)||(ongoing?(!fact_before(x.common,registration.registration_fact_id,debt->id)||debt->due_tick<registration.occupation_tick):!fact_before(x.common,debt->id,registration.occupation_fact_id))||!fact_before(x.common,registration.occupation_fact_id,link->registration_fact_id)||!fact_before(x.common,link->registration_fact_id,link->approval_fact_id)||!fact_before(x.common,link->approval_fact_id,link->decision_fact_id)||!fact_before(x.common,link->decision_fact_id,id))return common_failure("EconomicTakenPaymentSourceOrder",id);
    auto approval=x.politics.political_takeover_source(link->approval_fact_id);if(!approval.ok)return {false,{},approval.error};
    if(approval.value.source.fact_id!=link->approval_fact_id||!same_registration(registration,approval.value.source)||approval.value.source.effective_tick>payment->tick)return common_failure("EconomicTakenPaymentApprovalMismatch",id);
    const PoliticalResponsibilityDecision* decision=nullptr;for(const auto& d:approval.value.decisions)if(d.decision_fact_id==link->decision_fact_id){if(decision)return common_failure("DuplicateEconomicTakenPaymentDecision",id);decision=&d;}
    if(!decision||!decision->execution_accounts.empty()||decision->takeover_fact_id!=link->approval_fact_id||decision->responsibility.kind!=ref.kind||decision->responsibility.responsibility_id!=ref.responsibility_id||decision->responsibility.unit_id||decision->responsibility.asset_id||!decision->original_principal||!subject(*decision->original_principal,obligation->principal)||decision->original_beneficiaries.size()!=1||!subject(decision->original_beneficiaries.front(),obligation->beneficiary)||decision->collection_beneficiaries.size()!=1||!subject(decision->collection_beneficiaries.front(),obligation->beneficiary)||decision->receipt_wallet_id||decision->receipt_inventory_id||decision->payment_inventory_id||!decision->production_asset_ids.empty())return common_failure("EconomicTakenPaymentChangedResponsibility",id);
    if(decision->disposition==PoliticalResponsibilityDisposition::assume_by_successor){if(decision->payment_wallet_id!=std::optional<StableId>{payment->from_wallet_id}||!decision->execution_principal||decision->execution_principal->kind!=SubjectKind::organization||decision->execution_principal->id!=registration.successor_organization_id)return common_failure("EconomicTakenPaymentSuccessorMismatch",id);}
    else if(decision->disposition==PoliticalResponsibilityDisposition::continue_under_original){if(decision->payment_wallet_id||!decision->execution_principal||!subject(*decision->execution_principal,obligation->principal))return common_failure("EconomicTakenPaymentOriginalMismatch",id);}
    else return common_failure("EconomicTakenPaymentNotApproved",id);
    return common_success();
}
Status validate_economic_takeover_execution(const EconomicTakeoverReadContext& x,const EconomicTakeoverExecutionState& state){
    if(state.version!=1)return common_failure("EconomicTakeoverExecutionVersionMismatch");
    std::size_t covered=0;for(const auto& row:x.economy.payments.rows()){
        const auto* obligation=x.economy.obligations.find(row.second.obligation_id);if(!obligation)return common_failure("UnknownEconomicTakenPaymentObligation",row.first);
        const bool declared=state.payments.find(row.first)!=nullptr;
        if(row.second.authority_ref==obligation->authority_ref){
            if(declared)return common_failure("UnexpectedEconomicTakenPaymentSource",row.first);
            auto history=x.politics.political_responsibility_before_fact({PoliticalResponsibilityKind::arrear,row.second.arrear_id,{},{}},row.first,row.second.tick,x.common.state().identity);if(!history.ok)return {false,{},history.error};
            if(!history.value){history=x.politics.political_responsibility_before_fact({PoliticalResponsibilityKind::obligation,obligation->id,{},{}},row.first,row.second.tick,x.common.state().identity);if(!history.ok)return {false,{},history.error};}
            if(history.value)return common_failure("MissingEconomicTakenPaymentSource",row.first);
            continue;
        }
        auto valid=validate_economic_taken_payment_source(x,state,row.first);if(!valid.ok)return valid;++covered;
    }
    if(covered!=state.payments.size())return common_failure("OrphanEconomicTakenPaymentSource");
    auto requests=validate_economic_configuration_command_sources(x,state);if(!requests.ok)return requests;
    auto relief=validate_economic_taken_relief_sources(x,state);if(!relief.ok)return relief;
    auto materials=validate_economic_taken_material_sources(x,state);if(!materials.ok)return materials;return validate_economic_taken_configuration_sources(x,state);
}
Result settle_taken_economic_arrear_in_candidate(CommonStore& c,LocalEconomyState& economy,EconomicTakeoverExecutionState& sources,const EconomicTakeoverReadContext& x,const StableId& id,Integer tick){
    if(&c!=&x.common||&economy!=&x.economy||sources.version!=1)return fail("EconomicTakeoverCandidateMismatch",id);
    if(tick<c.tick()||tick>c.tick()+Integer(c.tick()!=INT64_MAX))return fail("EconomicTakeoverPaymentTickMismatch",id);
    auto* debt=economy.arrears.find(id);const auto* obligation=debt?economy.obligations.find(debt->obligation_id):nullptr;
    if(!debt||!obligation||debt->id!=id||debt->original_due<0||debt->paid_amount<0||debt->paid_amount>debt->original_due||debt->due_tick>tick||obligation->kind==ObligationKind::ration)return fail("InvalidEconomicTakeoverArrear",id);
    auto plan=read_economic_takeover_execution(x,{EconomicResponsibilityKind::arrear,id,{}});if(!plan.ok)return {false,{},plan.error};
    auto scope=EconomicTakenPaymentScope::captured_arrear;
    if(!plan.value){
        plan=read_economic_takeover_execution(x,{EconomicResponsibilityKind::obligation,obligation->id,{}});if(!plan.ok)return {false,{},plan.error};if(!plan.value)return {true,{}, {}};
        if(!fact_before(c,plan.value->registration.occupation_fact_id,plan.value->registration.registration_fact_id)||!fact_before(c,plan.value->registration.registration_fact_id,id)||debt->due_tick<plan.value->registration.occupation_tick)return fail("EconomicArrearOutsideAssumptionBoundary",id);
        scope=EconomicTakenPaymentScope::subsequent_obligation_period;
    }
    EconomicTakenPaymentResult result;result.handled=true;
    if(plan.value->status!=EconomicTakeoverExecutionStatus::approved){result.pending_code=plan.value->code;return {true,std::move(result),{}};}
    if(debt->paid_amount==debt->original_due)return {true,std::move(result),{}};
    if(!plan.value->decision||!plan.value->executing_owner)return fail("MissingEconomicTakeoverPaymentPlan",id);
    const auto* from=c.wallet(obligation->source_wallet_or_inventory);const auto* to=c.wallet(obligation->destination);
    if(!from||!to||from->id==to->id||!owner_equal(from->owner,*plan.value->executing_owner))return fail("EconomicTakeoverPaymentAccountsChanged",id);
    // 原受领容器再核一次；军事公库名单不能让家庭私款变成可支配公款。
    if(obligation->beneficiary.kind==SubjectKind::cohort){const auto* cohort=economy.cohorts.find(obligation->beneficiary.id);if(!cohort||cohort->private_wallet_id!=to->id||to->owner.kind!=Owner::Kind::household||to->owner.id!=cohort->household_group_id)return fail("InvalidEconomicTakenPaymentBeneficiary",id);}
    else if(obligation->beneficiary.kind==SubjectKind::actor){const auto* actor=c.actor(obligation->beneficiary.id);if(!actor||actor->wallet_id!=to->id||to->owner.kind!=Owner::Kind::actor||to->owner.id!=actor->id)return fail("InvalidEconomicTakenPaymentBeneficiary",id);}
    else return fail("UnsupportedEconomicTakenPaymentBeneficiary",id);
    auto spendable=c.spendable(from->id);if(!spendable.ok)return {false,{},spendable.error};const Integer amount=std::min(spendable.value,debt->original_due-debt->paid_amount);
    if(amount<=0){result.pending_code="InsufficientEconomicTakeoverFunds";return {true,std::move(result),{}};}
    ResourceRecipe recipe;auto paid=resource_recipe::append_money_transfer(recipe,from->id,to->id,amount,resource_recipe::MoneyTransferPolicy::spendable);if(!paid.ok)return {false,{},paid.error};paid=c.apply_resources(recipe);if(!paid.ok)return {false,{},paid.error};
    // ID晚失败发生在真实资源转移之后，调用者的候选事务必须完整撤回。
    auto receipt=c.next_id("local_payment");if(!receipt.ok)return {false,{},receipt.error};auto appended=c.append_fact({receipt.value,"obligation_payment","local_economy"});if(!appended.ok)return {false,{},appended.error};
    const auto& decision=*plan.value->decision;
    PaymentReceipt record{receipt.value,id,obligation->id,from->id,to->id,tick,debt->due_tick,amount,decision.decision_fact_id};
    if(!economy.payments.insert(record.id,record)||!sources.payments.insert(record.id,{record.id,id,plan.value->registration.registration_fact_id,decision.takeover_fact_id,decision.decision_fact_id,scope}))return fail("DuplicateEconomicTakenPayment",record.id);
    debt->paid_amount+=amount;economy.obligations.find(obligation->id)->last_receipt_id=record.id;
    result.receipt_id=record.id;return {true,std::move(result),{}};
}
Result settle_taken_economic_arrear(CommonStore& c,LocalEconomyState& economy,EconomicTakeoverExecutionState& sources,const EconomicTakeoverReadContext& x,const StableId& id,Integer tick){
    if(&c!=&x.common||&economy!=&x.economy)return fail("EconomicTakeoverCandidateMismatch",id);
    local_economy_detail::PhaseSavepoint savepoint(c);if(!savepoint.token().ok)return {false,{},savepoint.token().error};local_economy_detail::CandidateStateGuard<LocalEconomyState> state_guard(economy);local_economy_detail::CandidateStateGuard<EconomicTakeoverExecutionState> source_guard(sources);
    auto result=settle_taken_economic_arrear_in_candidate(c,economy,sources,x,id,tick);if(!result.ok)return result;
    auto valid=validate_economic_takeover_execution(x,sources);if(!valid.ok)return {false,{},valid.error};
    auto committed=savepoint.commit();if(!committed.ok)return {false,{},committed.error};state_guard.commit();source_guard.commit();return result;
}
}
