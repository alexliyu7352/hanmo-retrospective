// 历史工资事实按共同事实顺序一次折叠；后来的付款和人数不改写旧应付。
#include "cohort_wage_source.hpp"
#include "../economic_phase_clock.hpp"
#include "../takeover/execution_receipts.hpp"
#include <limits>
#include <map>
namespace hanmo::resident {
namespace {
// 所有主体比较同时核种类与稳定身份。
bool same_subject(const SubjectRef& a,const SubjectRef& b){return a.kind==b.kind&&a.id==b.id;}
// 临时验证状态不进入银行，原ArrearRecord仍是唯一欠额权威。
struct DebtFold { CohortWageObservation due;const ArrearRecord* original=nullptr;Integer paid=0,last_tick=0;std::size_t expected_payments=0,seen_payments=0;bool due_seen=false; };
// 付款来源只允许原机构钱包到原家庭钱包，无法从亲缘/公职猜出资产主权。
Status validate_source(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const StableId& id,const EconomicTakeoverReadContext* takeover){
    const auto* obligation=state.obligations.find(id);const auto* declared=catalog.obligations.find(id);
    if(!obligation||!declared||obligation->id!=id||declared->id!=id||obligation->kind!=ObligationKind::wage||obligation->principal.kind!=SubjectKind::organization||obligation->beneficiary.kind!=SubjectKind::cohort||!same_subject(obligation->principal,declared->principal)||!same_subject(obligation->beneficiary,declared->beneficiary)||declared->kind!=ObligationKind::wage||obligation->source_wallet_or_inventory!=declared->source_wallet_or_inventory||obligation->destination!=declared->destination||obligation->authority_ref!=declared->authority_ref)return common_failure("InvalidCohortWageObligation",id);
    const auto* cohort=state.cohorts.find(obligation->beneficiary.id);const auto* household=cohort?state.households.find(cohort->household_group_id):nullptr;const auto* payer=common.wallet(obligation->source_wallet_or_inventory);const auto* payee=common.wallet(obligation->destination);
    bool payer_owned=payer&&payer->owner.kind==Owner::Kind::organization&&payer->owner.id==obligation->principal.id;if(payer&&!payer_owned&&takeover){auto history=validate_economic_taken_container_history(*takeover,{EconomicResponsibilityKind::obligation,id,{}},payer->id,false,{Owner::Kind::organization,obligation->principal.id});if(!history.ok)return history;payer_owned=true;}
    if(!cohort||!household||cohort->id!=obligation->beneficiary.id||household->id!=cohort->household_group_id||cohort->private_wallet_id!=household->wallet_id||household->wallet_id!=obligation->destination||!payer_owned||!payee||payee->owner.kind!=Owner::Kind::household||payee->owner.id!=household->id)return common_failure("InvalidCohortWageHousehold",id);
    return common_success();
}
}
// 义务、欠据、付款和共同事实各遍历一次；历史长度增加不再产生平方次重复扫描。
Outcome<std::vector<CohortWageObservation>> observe_cohort_wage_history(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const std::vector<StableId>& ids,const CurrentMomentRead* actual_clock){return observe_cohort_wage_history(common,state,catalog,ids,nullptr,nullptr,actual_clock);}
Outcome<std::vector<CohortWageObservation>> observe_cohort_wage_history(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const std::vector<StableId>& ids,const EconomicTakeoverReadContext* takeover,const EconomicTakeoverExecutionState* execution,const CurrentMomentRead* actual_clock){
    auto actual_tick=read_economic_calendar_tick(common,actual_clock);if(!actual_tick.ok)return {false,{},actual_tick.error};const Integer observed_tick=actual_tick.value;
    if(bool(takeover)!=bool(execution)||(takeover&&(&takeover->common!=&common||&takeover->economy!=&state||&takeover->catalog!=&catalog)))return {false,{},common_failure("EconomicCohortWageContextMismatch").error};
    if(takeover){auto valid=validate_economic_takeover_execution(*takeover,*execution);if(!valid.ok)return {false,{},valid.error};}
    if(!catalog.enabled||state.last_step_tick!=observed_tick)return {false,{},common_failure("CohortWageClockMismatch").error};
    std::map<StableId,const RecurringObligation*> selected;
    for(const auto& id:ids){auto checked=validate_source(common,state,catalog,id,takeover);if(!checked.ok)return {false,{},checked.error};if(!selected.emplace(id,state.obligations.find(id)).second)return {false,{},common_failure("DuplicateCohortWageSource",id).error};}
    std::map<StableId,DebtFold> debts;
    for(const auto& row:state.arrears.rows()){
        const auto& debt=row.second;const auto source=selected.find(debt.obligation_id);if(source==selected.end())continue;
        const auto& obligation=*source->second;
        if(debt.id!=row.first||debt.due_tick<0||debt.due_tick>observed_tick||debt.original_due<0||debt.paid_amount<0||debt.paid_amount>debt.original_due)return {false,{},common_failure("InvalidCohortWageDebt",row.first).error};
        DebtFold fold;fold.original=&debt;fold.last_tick=debt.due_tick;auto& value=fold.due;value.source_fact_id=debt.id;value.obligation_id=obligation.id;value.arrear_id=debt.id;value.authority_ref=obligation.authority_ref;value.from_wallet_id=obligation.source_wallet_or_inventory;value.to_wallet_id=obligation.destination;value.household_group_id=state.cohorts.find(obligation.beneficiary.id)->household_group_id;value.principal=obligation.principal;value.beneficiary=obligation.beneficiary;value.fact_tick=debt.due_tick;value.due_tick=debt.due_tick;value.due_amount=debt.original_due;value.outstanding_amount=debt.original_due;debts.emplace(row.first,std::move(fold));
    }
    for(const auto& row:state.payments.rows()){
        const auto found=debts.find(row.second.arrear_id);
        if(found!=debts.end())++found->second.expected_payments;
        else if(selected.count(row.second.obligation_id))return {false,{},common_failure("InvalidCohortWagePayment",row.first).error};
    }
    std::vector<CohortWageObservation> out;
    for(const auto& row:common.state().facts.rows()){
        const auto due=debts.find(row.first);const auto* receipt=state.payments.find(row.first);
        if(due!=debts.end()){
            if(receipt||row.second.kind!="wage_due"||row.second.domain!="local_economy")return {false,{},common_failure("InvalidCohortWageDebt",row.first).error};
            due->second.due_seen=true;out.push_back(due->second.due);
        }
        if(!receipt)continue;const auto found=debts.find(receipt->arrear_id);if(found==debts.end())continue;
        auto& fold=found->second;const auto& value=fold.due;
        if(!fold.due_seen||receipt->id!=row.first||row.second.kind!="obligation_payment"||row.second.domain!="local_economy"||receipt->obligation_id!=value.obligation_id||receipt->from_wallet_id!=value.from_wallet_id||receipt->to_wallet_id!=value.to_wallet_id||(receipt->authority_ref!=value.authority_ref&&(!execution||!execution->payments.find(receipt->id)))||receipt->due_tick!=value.due_tick||receipt->tick<fold.last_tick||receipt->tick>observed_tick||receipt->amount<=0||fold.paid>std::numeric_limits<Integer>::max()-receipt->amount)return {false,{},common_failure("InvalidCohortWagePayment",row.first).error};
        fold.paid+=receipt->amount;fold.last_tick=receipt->tick;++fold.seen_payments;if(fold.paid>value.due_amount)return {false,{},common_failure("CohortWagePaymentCoverage",row.first).error};
        auto paid=value;paid.source_fact_id=row.first;paid.fact_tick=receipt->tick;paid.paid_amount=fold.paid;paid.outstanding_amount=paid.due_amount-paid.paid_amount;out.push_back(std::move(paid));
    }
    for(const auto& row:debts){const auto& fold=row.second;if(!fold.due_seen||fold.seen_payments!=fold.expected_payments||fold.paid!=fold.original->paid_amount)return {false,{},common_failure("CohortWagePaymentCoverage",row.first).error};}
    return {true,std::move(out),{}};
}
// 单事实读仍明确报未知来源；批量适配调用上面的原历史读，避免逐条重扫。
Outcome<CohortWageObservation> observe_cohort_wage_fact(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const StableId& id,const CurrentMomentRead* actual_clock){return observe_cohort_wage_fact(common,state,catalog,id,nullptr,nullptr,actual_clock);}
Outcome<CohortWageObservation> observe_cohort_wage_fact(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const StableId& id,const EconomicTakeoverReadContext* takeover,const EconomicTakeoverExecutionState* execution,const CurrentMomentRead* actual_clock){
    const auto* header=common.state().facts.find(id);const auto* payment=state.payments.find(id);const auto* debt=payment?state.arrears.find(payment->arrear_id):state.arrears.find(id);
    if(!header||header->domain!="local_economy"||!debt||header->kind!=(payment?"obligation_payment":"wage_due"))return {false,{},common_failure("UnsupportedCohortWageSource",id).error};
    auto history=observe_cohort_wage_history(common,state,catalog,{debt->obligation_id},takeover,execution,actual_clock);if(!history.ok)return {false,{},history.error};for(auto& value:history.value)if(value.source_fact_id==id)return {true,std::move(value),{}};
    return {false,{},common_failure("UnsupportedCohortWageSource",id).error};
}
}
