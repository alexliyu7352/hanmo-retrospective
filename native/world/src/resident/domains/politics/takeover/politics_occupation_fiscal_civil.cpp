#include "resident/domains/military/military_model.hpp"
// 新旧期来源须互斥；重放同一真实评税只返回原后果，不能重复改变民心。
#include "politics_occupation_fiscal_civil.hpp"
#include "politics_takeover_detail.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_settlement.hpp"
namespace hanmo::resident {
Outcome<PoliticalCivilSource> read_occupation_fiscal_civil_source(const CommonStore& c,const PoliticsState& politics,const PoliticsGovernanceState& state,const FiscalSettlementState* legacy,const OccupationFiscalCivilRead& read,const StableId& id){
    auto actual=read.occupation_fiscal_civil_period(id);if(!actual.ok)return {false,{},actual.error};
    const auto* head=c.state().facts.find(id);
    if(!actual.value||!head||head->id!=id||head->domain!="local_economy"||head->kind!="occupation_fiscal_period_assessed")return {false,{},common_failure("OccupationFiscalCivilSourceRequired",id).error};
    const auto& source=*actual.value;const auto& policy=source.policy;const auto* jurisdiction=politics.jurisdictions.find(policy.jurisdiction_id);
    if(!takeover_detail::same_identity(source.source_identity,c.state().identity)||source.period_fact_id!=id||source.due_tick<0||source.due_tick>c.tick()||policy.effective_from_tick>source.due_tick||!jurisdiction||jurisdiction->seat_place_id!=policy.site_id)return {false,{},common_failure("OccupationFiscalCivilSourceMismatch",id).error};
    for(const auto& row:state.consequences.rows())if(row.second.source_domain=="local_economy"&&row.second.source_fact_id!=id){
        auto old=legacy?fiscal_assessment_result(c,*legacy,row.second.source_fact_id):std::nullopt;
        if(old&&old->jurisdiction_id==policy.jurisdiction_id&&old->due_tick==source.due_tick)return {false,{},common_failure("FiscalCivilPeriodAlreadyConsumed",id).error};
        const auto* prior=c.state().facts.find(row.second.source_fact_id);if(!prior||prior->kind!="occupation_fiscal_period_assessed")continue;
        auto newer=read.occupation_fiscal_civil_period(row.second.source_fact_id);if(!newer.ok)return {false,{},newer.error};
        if(!newer.value)return {false,{},common_failure("OccupationFiscalCivilSourceRequired",row.second.source_fact_id).error};
        if(newer.value->policy.jurisdiction_id==policy.jurisdiction_id&&newer.value->due_tick==source.due_tick)return {false,{},common_failure("FiscalCivilPeriodAlreadyConsumed",id).error};
    }
    if(policy.mode!=FiscalTaxMode::normal&&policy.mode!=FiscalTaxMode::lenient&&policy.mode!=FiscalTaxMode::surcharge)return {false,{},common_failure("InvalidOccupationFiscalMode",id).error};
    const Integer delta=policy.mode==FiscalTaxMode::lenient?3:policy.mode==FiscalTaxMode::surcharge?-5:0;
    return {true,{policy.jurisdiction_id,"local_economy",id,{},source.due_tick,delta,100},{}};
}
}
