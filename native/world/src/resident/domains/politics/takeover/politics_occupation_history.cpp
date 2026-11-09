#include "resident/domains/military/military_model.hpp"
// 成功征募只按原完成事实之前的真实占领核窗口，后续收复或失守不倒改旧兵员来源。
#include "politics_occupation_access.hpp"
#include "politics_takeover_detail.hpp"
namespace hanmo::resident {
Outcome<std::optional<PoliticalRecruitmentOccupation>> PoliticalOccupationAccess::recruitment_occupation_before(const StableId& garrison,Integer completed,const StableId& id)const{
    const auto& x=context_;const auto fail=[](const StableId& code,const StableId& source=StableId{})->Outcome<std::optional<PoliticalRecruitmentOccupation>>{return {false,{},common_failure(code,source).error};};
    auto evaluation=political_takeover_evaluation_tick(x.common,x.phase);if(!evaluation.ok)return {false,{},evaluation.error};const auto* fact=x.common.state().facts.find(id);const bool started=fact&&fact->kind=="population_military_action";const auto sequence=organization_instance_sequence(id,started?"action":"population_military");
    if(!fact||fact->id!=id||fact->domain!="population_military"||(fact->kind!="population_enrolled"&&fact->kind!="population_military_action")||!sequence||*sequence>=x.common.state().next_instance_id||completed<0||completed>evaluation.value)return fail("InvalidPoliticalRecruitmentFactBoundary",id);
    auto valid=validate_political_takeover_state(x);if(!valid.ok)return {false,{},valid.error};
    auto current=military_garrison_owner(x.common,x.military,x.military_catalog,x.military_access,garrison,evaluation.value);if(!current.ok)return {false,{},current.error};
    auto before=[&](const StableId& first,const StableId& last){bool seen=false;for(const auto& row:x.common.state().facts.rows()){if(row.first==last)return seen;if(row.first==first)seen=true;}return false;};
    // 军事链按末次到最早排列；显式比较共同序号，不让末尾的旧占领覆盖新窗口。
    const MilitaryOccupationFact* selected=nullptr;Integer selected_sequence=0;
    for(const auto& source:current.value.chain){const auto* occupation=takeover_detail::occupation(x.military,source);const auto number=organization_instance_sequence(source,"military");if(!occupation||!number||((*number<*sequence)!=before(source,id)))return fail("PoliticalRecruitmentCausalityMismatch",id);if(*number>=*sequence)continue;if(occupation->tick>completed)return fail("PoliticalRecruitmentCausalityMismatch",id);if(*number>selected_sequence){selected_sequence=*number;selected=occupation;}}
    if(!selected||!x.catalog.enabled)return {true,std::nullopt,{}};
    const PoliticalTakeoverRegistration* registration=nullptr;for(const auto& row:x.state.registrations.rows())if(row.second.occupation_fact_id==selected->id){if(registration)return fail("AmbiguousPoliticalOccupationWindow",garrison);registration=&row.second;}
    if(!registration)return {true,std::nullopt,{}};
    if(!before(registration->id,id))return fail("PoliticalRecruitmentBeforeRegistration",id);
    const auto* rule=x.catalog.rules.find(registration->rule_id);if(!rule||rule->garrison_id!=garrison||registration->successor_organization_id!=selected->to_organization_id)return fail("PoliticalOccupationControlMismatch",garrison);
    if(registration->tick<0||rule->occupation_rule.integration_ticks<=0||registration->tick>INT64_MAX-rule->occupation_rule.integration_ticks)return fail("PoliticalOccupationWindowOverflow",registration->id);
    PoliticalRecruitmentOccupation result;result.source_identity=x.common.state().identity;result.registration_fact_id=registration->id;result.occupation_fact_id=selected->id;result.jurisdiction_id=rule->jurisdiction_id;result.site_id=rule->site_id;result.garrison_id=garrison;result.controlling_organization_id=registration->successor_organization_id;result.occupation_rule=rule->occupation_rule;result.occupation_tick=registration->tick;result.observed_tick=completed;result.integration_until_tick=registration->tick+rule->occupation_rule.integration_ticks;result.blocked=completed<result.integration_until_tick;return {true,std::move(result),{}};
}
}
