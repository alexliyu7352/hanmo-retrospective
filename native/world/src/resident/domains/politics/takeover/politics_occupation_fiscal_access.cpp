#include "resident/domains/military/military_model.hpp"
// 新税期按原due选择当时有效签发；后续占领截断执行但不抹去已成立税期来源。
#include "politics_occupation_fiscal_detail.hpp"
namespace hanmo::resident {
Outcome<std::optional<OccupationFiscalPolicySource>> OccupationFiscalAccess::occupation_fiscal_policy_for_due_tick(const StableId& site,Integer due)const{return select(site,due,nullptr);}
Outcome<std::optional<OccupationFiscalPolicySource>> OccupationFiscalAccess::occupation_fiscal_policy_for_assessment_source(const StableId& site,Integer due,const StableId& source)const{return select(site,due,&source);}
Outcome<std::optional<OccupationFiscalPolicySource>> OccupationFiscalAccess::select(const StableId& site,Integer due,const StableId* historical)const{
    const auto& x=context_;const auto& t=x.takeover;const auto fail=[](const StableId& code,const StableId& id){return Outcome<std::optional<OccupationFiscalPolicySource>>{false,{},common_failure(code,id).error};};
    auto evaluation=political_takeover_evaluation_tick(t.common,t.phase);if(!evaluation.ok)return {false,{},evaluation.error};
    if(due<0||due>evaluation.value)return fail("FutureOrInvalidFiscalAssessment",site);
    if(!t.common_catalog.places.find(site))return fail("UnknownFiscalSite",site);
    auto valid=validate_occupation_fiscal(x);if(!valid.ok)return {false,{},valid.error};
    if(!x.catalog.enabled)return historical?fail("OccupationFiscalAssessmentPolicyMismatch",*historical):Outcome<std::optional<OccupationFiscalPolicySource>>{true,std::nullopt,{}};
    const Jurisdiction* jurisdiction=nullptr;for(const auto& row:t.politics_catalog.jurisdictions.rows())if(row.second.seat_place_id==site){if(jurisdiction)return fail("AmbiguousFiscalJurisdiction",site);jurisdiction=&row.second;}
    const OccupationFiscalPolicyRecord* selected=nullptr;
    if(jurisdiction)for(const auto& row:x.state.policies.rows()){const auto& p=row.second.policy;if(p.jurisdiction_id==jurisdiction->id&&p.effective_from_tick<=due&&(!selected||p.policy_revision>selected->policy.policy_revision))selected=&row.second;}
    if(historical&&(!selected||selected->id!=*historical))return fail("OccupationFiscalAssessmentPolicyMismatch",*historical);
    if(!selected)return {true,std::nullopt,{}};
    const auto& r=*t.state.registrations.find(selected->registration_fact_id);const auto& rule=*t.catalog.rules.find(r.rule_id);const auto sequence=*organization_instance_sequence(selected->id,political_occupation_fiscal_id_prefix);
    if(r.tick>INT64_MAX-rule.occupation_rule.integration_ticks)return fail("PoliticalOccupationWindowOverflow",r.id);
    OccupationFiscalPolicySource out;out.source_identity=t.common.state().identity;out.policy=selected->policy;out.registration_fact_id=r.id;out.occupation_fact_id=r.occupation_fact_id;out.occupation_rule=rule.occupation_rule;out.occupation_tick=r.tick;out.integration_until_tick=r.tick+rule.occupation_rule.integration_ticks;out.income_basis_points=due<out.integration_until_tick?rule.occupation_rule.income_basis_points:10000;
    Integer first_loss=INT64_MAX;
    for(const auto& row:t.state.registrations.rows()){
        const auto* next_rule=t.catalog.rules.find(row.second.rule_id);if(!next_rule||next_rule->jurisdiction_id!=selected->policy.jurisdiction_id)continue;
        const auto number=*organization_instance_sequence(row.second.occupation_fact_id,"military");
        if(number>sequence&&number<first_loss){first_loss=number;out.control_loss=FiscalPolicyOccupationBoundary{t.common.state().identity,row.second.occupation_fact_id,row.second.tick};}
    }
    if(out.control_loss&&(historical?due>out.control_loss->occupation_tick:due>=out.control_loss->occupation_tick))return historical?fail("OccupationFiscalAssessmentAfterControlLoss",*historical):Outcome<std::optional<OccupationFiscalPolicySource>>{true,std::nullopt,{}};
    return {true,std::move(out),{}};
}
}
