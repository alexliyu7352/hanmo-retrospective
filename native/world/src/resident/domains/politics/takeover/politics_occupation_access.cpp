#include "resident/domains/military/military_model.hpp"
// 末次真实占领决定当前窗口；原政权收复也不能复活更早已结束的窗口。
#include "politics_occupation_access.hpp"
#include "politics_takeover_detail.hpp"
namespace hanmo::resident {
Outcome<std::optional<PoliticalRecruitmentOccupation>> PoliticalOccupationAccess::recruitment_occupation(const StableId& garrison)const{
    const auto& x=context_;
    auto tick=political_takeover_evaluation_tick(x.common,x.phase);if(!tick.ok)return {false,{},tick.error};
    auto valid=validate_political_takeover_state(x);if(!valid.ok)return {false,{},valid.error};
    auto current=military_garrison_owner(x.common,x.military,x.military_catalog,x.military_access,garrison,tick.value);if(!current.ok)return {false,{},current.error};
    if(!x.catalog.enabled||current.value.occupation_id.empty())return {true,std::nullopt,{}};
    const PoliticalTakeoverRegistration* selected=nullptr;
    for(const auto& row:x.state.registrations.rows())if(row.second.occupation_fact_id==current.value.occupation_id){
        if(selected)return {false,{},common_failure("AmbiguousPoliticalOccupationWindow",garrison).error};selected=&row.second;
    }
    // 没有匹配作者规则的占领保持旧规则；匹配但漏登记已由完整接管验证拒绝。
    if(!selected)return {true,std::nullopt,{}};
    const auto* rule=x.catalog.rules.find(selected->rule_id);
    if(!rule||rule->garrison_id!=garrison||selected->successor_organization_id!=current.value.organization_id)return {false,{},common_failure("PoliticalOccupationControlMismatch",garrison).error};
    if(selected->tick<0||rule->occupation_rule.integration_ticks<=0||selected->tick>INT64_MAX-rule->occupation_rule.integration_ticks)return {false,{},common_failure("PoliticalOccupationWindowOverflow",selected->id).error};
    PoliticalRecruitmentOccupation out;out.source_identity=x.common.state().identity;out.registration_fact_id=selected->id;out.occupation_fact_id=selected->occupation_fact_id;out.jurisdiction_id=rule->jurisdiction_id;out.site_id=rule->site_id;out.garrison_id=garrison;out.controlling_organization_id=selected->successor_organization_id;out.occupation_rule=rule->occupation_rule;out.occupation_tick=selected->tick;out.observed_tick=tick.value;out.integration_until_tick=selected->tick+rule->occupation_rule.integration_ticks;out.blocked=out.observed_tick<out.integration_until_tick;
    return {true,std::move(out),{}};
}
}
