#include "resident/kernel/catalog_section_observation.hpp"
#include "resident/domains/military/military_model.hpp"
// 目录只保存作者合同，不生成已应用占领或已批准责任。
#include "politics_takeover_catalog.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
Status validate_political_takeover_catalog(const CommonCatalog& common,const OrganizationCatalog& organizations,const PoliticsCatalog& politics,const MilitaryCatalog& military,const PoliticalTakeoverCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 111, "PoliticalTakeoverCatalog", "validate_political_takeover_catalog", &catalog);

    if((catalog.version!=1&&catalog.version!=2)||(catalog.task_execution_version!=1&&catalog.task_execution_version!=2)||(!catalog.enabled&&catalog.task_execution_version!=1))return common_failure("UnsupportedPoliticalTakeoverVersion");
    if(!catalog.enabled)return catalog.rules.empty()?common_success():common_failure("UnexpectedPoliticalTakeoverCatalog");
    if(catalog.version==2&&(!organizations.authority_lifecycle_enabled||!organizations.exact_fact_clock_enabled))return common_failure("PoliticalTakeoverTime2AuthorityRequired");
    if(!organizations.enabled||!politics.enabled||!military.military||catalog.rules.empty()||catalog.rules.size()>4096)return common_failure("InvalidPoliticalTakeoverCatalog");
    std::set<std::pair<StableId,StableId>> covered;std::map<StableId,StableId> district_holds;
    for(const auto& row:catalog.rules.rows()){
        const auto& rule=row.second;const auto* district=politics.jurisdictions.find(rule.jurisdiction_id);const auto* site=common.places.find(rule.site_id);const auto* office=organizations.offices.find(rule.successor_office_id);
        const auto stronghold=std::find_if(military.strongholds.begin(),military.strongholds.end(),[&](const auto& value){return value.id==rule.stronghold_id;});
        const auto garrison=std::find_if(military.garrisons.begin(),military.garrisons.end(),[&](const auto& value){return value.id==rule.garrison_id;});
        if(row.first!=rule.id||rule.id.empty()||rule.id.size()>4096||!district||!site||district->seat_place_id!=rule.site_id||!office||!organizations.organizations.find(office->organization_id)||stronghold==military.strongholds.end()||garrison==military.garrisons.end()||stronghold->garrison_id!=rule.garrison_id||stronghold->settlement_id!=site->settlement_id||rule.occupation_rule.rule_id!=rule.id||rule.occupation_rule.rule_version!=1||rule.occupation_rule.integration_ticks<=0||rule.occupation_rule.income_basis_points<0||rule.occupation_rule.income_basis_points>10000||!covered.insert({rule.stronghold_id,office->organization_id}).second)return common_failure("InvalidPoliticalTakeoverRule",row.first);
        if(garrison->organization_id!=district->controlling_organization_id||std::find(stronghold->public_wallet_ids.begin(),stronghold->public_wallet_ids.end(),district->public_wallet_id)==stronghold->public_wallet_ids.end()||!std::all_of(district->public_inventory_ids.begin(),district->public_inventory_ids.end(),[&](const auto& id){return std::find(stronghold->public_inventory_ids.begin(),stronghold->public_inventory_ids.end(),id)!=stronghold->public_inventory_ids.end();}))return common_failure("PoliticalTakeoverPublicScopeMismatch",rule.id);
        // 作者必须提供原专项Grant；到实际决定时仍复核当前持有人、时窗和全清单。
        bool authorized=false;
        for(const auto& id:office->grant_ids){const auto* grant=organizations.grants.find(id);if(!grant||grant->office_id!=office->id||grant->amount_limit<0||std::find(grant->allowed_command_kinds.begin(),grant->allowed_command_kinds.end(),political_takeover_command_kind)==grant->allowed_command_kinds.end())continue;
            if(std::none_of(grant->scopes.begin(),grant->scopes.end(),[&](const auto& scope){return scope.kind==ScopeRef::Kind::site&&scope.id==rule.site_id;}))continue;
            const auto contains=[](const auto& available,const auto& required){return std::all_of(required.begin(),required.end(),[&](const auto& value){return std::find(available.begin(),available.end(),value)!=available.end();});};
            if(contains(grant->wallet_ids,stronghold->public_wallet_ids)&&contains(grant->inventory_ids,stronghold->public_inventory_ids)){authorized=true;break;}
        }
        if(!authorized)return common_failure("MissingPoliticalTakeoverGrant",rule.id);
        auto district_hold=district_holds.emplace(rule.jurisdiction_id,rule.stronghold_id);if(!district_hold.second&&district_hold.first->second!=rule.stronghold_id)return common_failure("AmbiguousPoliticalTakeoverJurisdiction",rule.jurisdiction_id);
        if(rule.occupation_rule.integration_ticks>2147483647)return common_failure("InvalidPoliticalTakeoverRule",rule.id);
    }
    return common_success();
}
Outcome<std::optional<StableId>> political_takeover_rule_for_occupation(const OrganizationCatalog& organizations,const PoliticalTakeoverCatalog& catalog,const MilitaryOccupationFact& occupation){
    if((catalog.version!=1&&catalog.version!=2)||(catalog.task_execution_version!=1&&catalog.task_execution_version!=2)||(!catalog.enabled&&catalog.task_execution_version!=1))return {false,{},common_failure("UnsupportedPoliticalTakeoverVersion").error};
    if(!catalog.enabled)return {true,std::nullopt,{}};
    std::optional<StableId> selected;bool configured=false;
    for(const auto& row:catalog.rules.rows()){const auto& rule=row.second;if(rule.stronghold_id!=occupation.stronghold_id||rule.garrison_id!=occupation.garrison_id)continue;configured=true;const auto* office=organizations.offices.find(rule.successor_office_id);if(!office)return {false,{},common_failure("UnknownOffice",rule.successor_office_id).error};if(office->organization_id!=occupation.to_organization_id)continue;if(selected)return {false,{},common_failure("AmbiguousPoliticalTakeoverRule",occupation.id).error};selected=rule.id;}
    if(configured&&!selected)return {false,{},common_failure("PoliticalTakeoverSuccessorNotConfigured",occupation.id).error};
    return {true,std::move(selected),{}};
}
}
