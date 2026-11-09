// 历史公权与来源先后只按真实共同实例序核验，不凭今日Owner补旧授权。
#pragma once
#include "politics_occupation_fiscal.hpp"
#include "politics_takeover_detail.hpp"
namespace hanmo::resident::occupation_fiscal_detail {
inline bool valid_mode(FiscalTaxMode mode){return mode==FiscalTaxMode::lenient||mode==FiscalTaxMode::normal||mode==FiscalTaxMode::surcharge;}
inline Integer revision(const OccupationFiscalState& state,const StableId& jurisdiction){Integer result=0;for(const auto& row:state.policies.rows())if(row.second.policy.jurisdiction_id==jurisdiction)result=std::max(result,row.second.policy.policy_revision);return result;}
inline bool grant_covers(const Grant& grant,const PoliticalTakeoverRule& rule,const PoliticalTakeoverRegistration& registration,const FiscalPolicySnapshot& p,Integer tick){
    using takeover_detail::has;
    if(grant.office_id!=rule.successor_office_id||grant.amount_limit<0||tick<grant.valid_from||(grant.valid_until&&tick>*grant.valid_until)||!has(grant.allowed_command_kinds,political_occupation_fiscal_command_kind)||!has(grant.wallet_ids,p.public_wallet_id)||!has(grant.inventory_ids,p.public_inventory_id))return false;
    if(std::none_of(grant.scopes.begin(),grant.scopes.end(),[&](const auto& scope){return scope.kind==ScopeRef::Kind::site&&scope.id==rule.site_id;}))return false;
    // 每个额外Grant资源也须在这份真实公库清单内；不以当前Owner替代历史证明。
    for(const auto& id:grant.wallet_ids)if(!has(registration.public_wallet_ids,id))return false;
    for(const auto& id:grant.inventory_ids)if(!has(registration.public_inventory_ids,id))return false;
    return true;
}
inline const PoliticalTakeoverRegistration* last_before(const PoliticalTakeoverContext& x,const StableId& jurisdiction,Integer sequence){
    const PoliticalTakeoverRegistration* result=nullptr;Integer latest=0;
    for(const auto& row:x.state.registrations.rows()){
        const auto* rule=x.catalog.rules.find(row.second.rule_id);if(!rule||rule->jurisdiction_id!=jurisdiction)continue;
        auto n=organization_instance_sequence(row.second.occupation_fact_id,"military");if(n&&*n<sequence&&*n>latest){latest=*n;result=&row.second;}
    }
    return result;
}
inline const Grant* current_grant(const OccupationFiscalContext& x,const PoliticalSetOccupationTaxCommand& command,const PoliticalTakeoverRegistration& registration,const PoliticalTakeoverRule& rule,const Jurisdiction& jurisdiction){
    const auto& t=x.takeover;const auto* office=t.organizations.offices.find(command.authority.office_id);if(!office)return nullptr;
    FiscalPolicySnapshot p;p.public_wallet_id=jurisdiction.public_wallet_id;p.public_inventory_id=command.public_inventory_id;
    for(const auto& id:office->grant_ids){const auto* grant=t.organization_catalog.grants.find(id);if(grant&&grant_covers(*grant,rule,registration,p,t.common.tick()))return grant;}
    return nullptr;
}
}
