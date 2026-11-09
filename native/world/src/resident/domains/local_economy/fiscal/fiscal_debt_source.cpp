// 金额按原不可变付款顺序截止到指定事实，后来的偿付不能改写旧消息中的欠额。
#include "fiscal_debt_source.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include <algorithm>
#include "../economic_phase_clock.hpp"
namespace hanmo::resident {
namespace {
// 来源校验失败保持显式错误，不降格成“没有欠款”。
Outcome<std::vector<FiscalDebtObservation>> fail(const StableId& code,const StableId& id){return {false,{},common_failure(code,id).error};}
// 群体仅引用真实家庭容器，机构仅引用自己的原容器；具名Actor不能冒税源。
bool taxpayer_matches(const LocalEconomyState& economy,const FiscalTaxSource& source,const FiscalSourceAssessment& assessment){
    if(source.taxpayer.kind==SubjectKind::organization)return assessment.owner.kind==Owner::Kind::organization&&assessment.owner.id==source.taxpayer.id;
    if(source.taxpayer.kind!=SubjectKind::cohort)return false;
    const auto* cohort=economy.cohorts.find(source.taxpayer.id);const auto* household=cohort?economy.households.find(cohort->household_group_id):nullptr;
    return household&&assessment.owner.kind==Owner::Kind::household&&assessment.owner.id==household->id&&cohort->private_wallet_id==source.wallet_id&&cohort->private_inventory_id==source.inventory_id&&household->wallet_id==source.wallet_id&&household->inventory_id==source.inventory_id;
}
}
// 此读口不构造Need；原ArrearRecord仍是当前税债唯一权威，所有返回值只是某张事实的快照。
Outcome<std::vector<FiscalDebtObservation>> observe_fiscal_debt_fact(const CommonStore& common,const LocalEconomyState& economy,const FiscalSettlementState& state,const FiscalSettlementCatalog& catalog,const StableId& id,const CurrentMomentRead* actual_clock){
 auto actual_tick=read_economic_calendar_tick(common,actual_clock);if(!actual_tick.ok)return {false,{},actual_tick.error};const Integer observed_tick=actual_tick.value;
    if(catalog.version!=1||state.version!=1)return fail("UnsupportedFiscalDebtVersion",id);
    if(state.initialized_tick<0||state.initialized_tick>observed_tick||state.last_step_tick!=observed_tick||economy.last_step_tick!=observed_tick)return fail("FiscalDebtClockMismatch",id);
    const auto* header=common.state().facts.find(id);if(!header)return fail("FiscalDebtSourceUnknown",id);
    if(header->domain!="local_economy")return fail("UnsupportedFiscalDebtSource",id);
    if(const auto* period=state.periods.find(id)){
        const auto* district=catalog.districts.find(period->district_id);const auto* registered=state.districts.find(period->district_id);const auto* registration=registered?common.state().facts.find(registered->registration_fact_id):nullptr;
        if(header->kind!="fiscal_period_assessed"||period->id!=id||!district||!registered||registered->id!=period->district_id||period->registration_fact_id!=registered->registration_fact_id||!registration||registration->kind!="fiscal_source_registered"||registration->domain!="local_economy"||period->due_tick<state.initialized_tick||period->due_tick>observed_tick||period->period_start_tick!=period->due_tick-district->period_ticks||(!period->policy&&!period->source_assessment_ids.empty()))return fail("InvalidFiscalDebtPeriod",id);
        return {true,{}, {}};
    }
    for(const auto& row:state.districts.rows())if(row.second.registration_fact_id==id){if(header->kind!="fiscal_source_registered"||row.second.id!=row.first||!catalog.districts.find(row.first))return fail("InvalidFiscalDebtRegistration",id);return {true,{}, {}};}
    const auto* payment=state.payments.find(id);const auto* assessment=payment?state.assessments.find(payment->assessment_id):state.assessments.find(id);
    if(!assessment||header->kind!=(payment?"fiscal_payment":"fiscal_source_assessed")||(payment&&payment->id!=id)||(!payment&&assessment->id!=id))return fail("UnsupportedFiscalDebtSource",id);
    const auto* assessed_header=common.state().facts.find(assessment->id);const auto* period=state.periods.find(assessment->period_id);const auto* period_header=period?common.state().facts.find(period->id):nullptr;const auto* district=period?catalog.districts.find(period->district_id):nullptr;const auto* source=district?district->sources.find(assessment->source_id):nullptr;
    if(!assessed_header||assessed_header->domain!="local_economy"||assessed_header->kind!="fiscal_source_assessed"||!period||!period->policy||!period_header||period_header->domain!="local_economy"||period_header->kind!="fiscal_period_assessed"||!source||source->id!=assessment->source_id||district->site_id!=period->policy->site_id||assessment->wallet_id!=source->wallet_id||assessment->inventory_id!=source->inventory_id||assessment->grain_item_id!=source->grain_item_id||!taxpayer_matches(economy,*source,*assessment)||std::count(period->source_assessment_ids.begin(),period->source_assessment_ids.end(),assessment->id)!=1)return fail("InvalidFiscalDebtAssessment",id);
    const auto& policy=*period->policy;const auto* policy_header=common.state().facts.find(policy.source_fact_id);
    const Integer tick=payment?payment->tick:period->due_tick;
    if(!policy_header||policy_header->kind!="political_fiscal_policy_changed"||policy_header->domain!="politics"||policy.jurisdiction_id.empty()||policy.organization_id.empty()||period->due_tick<0||tick<period->due_tick||tick>observed_tick||policy.effective_from_tick>period->due_tick)return fail("InvalidFiscalDebtPolicy",id);
    Integer cash=0,grain=0;
    if(payment){
        bool found=false;Integer previous_tick=period->due_tick;
        for(const auto& row:state.payments.rows()){
            const auto& before=row.second;
            if(before.assessment_id==assessment->id){
                const auto* fact=common.state().facts.find(row.first);
                if(before.id!=row.first||!fact||fact->domain!="local_economy"||fact->kind!="fiscal_payment"||before.tick<previous_tick||before.tick>tick||before.cash_paid<0||before.grain_paid<0||(!before.cash_paid&&!before.grain_paid)||!local_economy_detail::add(cash,before.cash_paid,cash)||!local_economy_detail::add(grain,before.grain_paid,grain))return fail("InvalidFiscalDebtPayment",row.first);
                previous_tick=before.tick;
            }
            if(row.first==id){found=true;break;}
        }
        if(!found)return fail("FiscalDebtPaymentMissing",id);
    }
    std::vector<FiscalDebtObservation> out;
    for(const auto unit:{FiscalDebtUnit::cash,FiscalDebtUnit::grain}){
        const bool money=unit==FiscalDebtUnit::cash;const auto& debt=money?assessment->cash:assessment->grain;const auto paid=money?cash:grain;const StableId suffix=money?".cash":".grain";
        if(debt.id!=assessment->id+suffix||debt.obligation_id!=source->id+suffix||debt.due_tick!=period->due_tick||debt.original_due<0||debt.paid_amount<paid||debt.paid_amount>debt.original_due||paid>debt.original_due)return fail("InvalidFiscalDebtAmount",id);
        FiscalDebtObservation value;value.source_fact_id=id;value.assessment_id=assessment->id;value.period_id=period->id;value.source_id=source->id;value.obligation_id=debt.obligation_id;value.arrear_id=debt.id;value.jurisdiction_id=policy.jurisdiction_id;value.site_id=policy.site_id;value.policy_fact_id=policy.source_fact_id;value.item_id=money?StableId{}:assessment->grain_item_id;value.taxpayer=source->taxpayer;value.beneficiary={SubjectKind::organization,policy.organization_id};value.unit=unit;value.fact_tick=tick;value.due_tick=debt.due_tick;value.due_amount=debt.original_due;value.paid_amount=paid;value.outstanding_amount=debt.original_due-paid;out.push_back(std::move(value));
    }
    return {true,std::move(out),{}};
}
}
