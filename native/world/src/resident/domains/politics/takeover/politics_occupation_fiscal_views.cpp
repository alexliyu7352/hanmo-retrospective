#include "resident/domains/military/military_model.hpp"
// 当前接任官在原治所读取新税政，动作参数来自实际Office/Grant；预测系数不是已征收入。
#include "politics_occupation_fiscal_detail.hpp"
#include "resident/domains/politics/politics_internal.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> query_occupation_fiscal_entries(const OccupationFiscalContext& x,const ActivityRead& activity,const StableId& actor,const StableId& site){
    using V=BoundaryValue;const auto& t=x.takeover;
    if(!x.catalog.enabled)return {false,{},common_failure("OccupationFiscalNotEnabled",{},ErrorKind::unsupported_capability).error};
    if(actor!=t.common.state().metadata.player_actor_id)return {false,{},common_failure("PoliticalTakeoverObserverMismatch").error};
    auto location=politics_detail::place(t.common,actor);if(!location||location->place_id!=site)return {false,{},common_failure("PlaceNotObservable",site).error};
    auto valid=validate_occupation_fiscal(x);if(!valid.ok)return {false,{},valid.error};
    V::Array rows;Integer scanned=0;bool truncated=false;
    const auto mode_text=[](FiscalTaxMode mode)->StableId{return mode==FiscalTaxMode::lenient?"宽免":mode==FiscalTaxMode::normal?"常赋":"加征";};
    for(const auto& row:t.state.registrations.rows()){
        if(++scanned>256){truncated=true;break;}const auto& registration=row.second;const auto& rule=*t.catalog.rules.find(registration.rule_id);
        if(rule.site_id!=site||!takeover_detail::known_to_actor(t,actor,registration))continue;
        auto current=military_garrison_owner(t.common,t.military,t.military_catalog,t.military_access,rule.garrison_id);if(!current.ok)return {false,{},current.error};if(current.value.occupation_id!=registration.occupation_fact_id)continue;
        const auto* jurisdiction=x.politics.jurisdictions.find(rule.jurisdiction_id);const auto* office=t.organizations.offices.find(rule.successor_office_id);if(!jurisdiction||!office)return {false,{},common_failure("UnknownOccupationFiscalJurisdiction",rule.jurisdiction_id).error};
        const OccupationFiscalPolicyRecord* latest=nullptr;for(const auto& policy:x.state.policies.rows())if(policy.second.registration_fact_id==registration.id&&(!latest||policy.second.policy.policy_revision>latest->policy.policy_revision))latest=&policy.second;
        V::Array actions;for(const auto& inventory:jurisdiction->public_inventory_ids){if(truncated)break;for(auto mode:{FiscalTaxMode::lenient,FiscalTaxMode::normal,FiscalTaxMode::surcharge}){
            if(++scanned>256){truncated=true;break;}PoliticalSetOccupationTaxCommand command{{office->organization_id,office->id,office->epoch,actor},jurisdiction->id,registration.id,inventory,jurisdiction->revision,occupation_fiscal_detail::revision(x.state,jurisdiction->id),mode};
            auto allowed=preview_political_set_occupation_tax(x,activity,actor,command);actions.push_back(boundary::obj({{"text",V{"新签"+mode_text(mode)}},{"command",V{political_occupation_fiscal_command_kind}},{"params",encode_political_set_occupation_tax(command)},{"available",V{allowed.ok}},{"code",V{allowed.ok?StableId{}:allowed.error.original.code}}}));
        }}
        if(registration.tick>INT64_MAX-rule.occupation_rule.integration_ticks)return {false,{},common_failure("PoliticalOccupationWindowOverflow",registration.id).error};const auto until=registration.tick+rule.occupation_rule.integration_ticks;const auto bp=t.common.tick()<until?rule.occupation_rule.income_basis_points:10000;const auto* name=t.common_catalog.texts.find(jurisdiction->name_key);
        rows.push_back(boundary::obj({{"id",V{registration.id+":fiscal"}},{"kind",V{"occupation_fiscal_policy"}},{"display_name",V{name?*name:jurisdiction->id}},{"text",V{latest?"已新签"+mode_text(latest->policy.mode)+"，半日"+std::to_string(latest->policy.effective_from_tick)+"起生效":"接管后尚未新签税政"}},{"source_fact_id",V{latest?latest->id:StableId{}}},{"terms_text",V{"原税债及原受益方未满整单位的税额余数保持原账。新税政只作用于原周期的后续税期；整合截止半日"+std::to_string(until)+"，当前税基系数"+std::to_string(bp)+"/10000，不代表已收收入。"}},{"actions",V{std::move(actions)}}}));
        if(rows.size()>=16){truncated=true;break;}
    }
    return {true,boundary::obj({{"ok",V{true}},{"rows",V{std::move(rows)}},{"truncated",V{truncated}}}),{}};
}
}
