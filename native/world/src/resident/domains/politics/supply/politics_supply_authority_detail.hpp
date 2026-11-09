// 只给本供给签发/观察边界使用的纯历史读，不对外开放任意fact授权。
#pragma once
#include "politics_supply_authority.hpp"
#include "resident/domains/politics/takeover/politics_takeover_detail.hpp"
namespace hanmo::resident::supply_authority_detail {
inline bool before(const CommonStore& c,const StableId& a,const StableId& b){bool seen=false;for(const auto& row:c.state().facts.rows()){if(row.first==b)return seen;if(row.first==a)seen=true;}return false;}
inline bool covers(const Grant& g,const PoliticalTakeoverRule& rule,const PoliticalSupplyPolicyMandate& m,Integer tick){
    using takeover_detail::has;
    return g.office_id==rule.successor_office_id&&g.amount_limit>=m.maximum_contract_commitment&&m.maximum_contract_commitment>=0&&tick>=g.valid_from&&(!g.valid_until||tick<=*g.valid_until)&&has(g.allowed_command_kinds,political_supply_policy_grant)&&has(g.wallet_ids,m.funding_wallet_id)&&has(g.wallet_ids,m.reward_wallet_id)&&has(g.inventory_ids,m.inventory_id)&&std::any_of(g.scopes.begin(),g.scopes.end(),[&](const auto& scope){return scope.kind==ScopeRef::Kind::site&&scope.id==m.site_id;});
}
// 全链到今日必须同实际军事清单一致；仅返回指定真实边界之前的Owner。
Outcome<Owner> asset_owner_before(const PoliticalTakeoverContext&,const StableId& asset,bool inventory,Integer tick,Integer sequence,const StableId& boundary);
// 最新实际占領与本登记须一致。不能因同tick晚发生的收复为早期草案补权。
Status control_before(const PoliticalTakeoverContext&,const PoliticalTakeoverRegistration&,const PoliticalTakeoverRule&,Integer tick,Integer sequence,const StableId& boundary);
}
