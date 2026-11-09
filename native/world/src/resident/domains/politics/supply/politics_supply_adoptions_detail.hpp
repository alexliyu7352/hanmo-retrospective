// 仅纯条款读参与接管验证；经济完整签权冷验不从这里反向调用。
#pragma once
#include "politics_supply_adoptions.hpp"
#include "politics_supply_authority_detail.hpp"
#include "resident/domains/local_economy/supply/supply_policy_contract.hpp"
namespace hanmo::resident::supply_adoption_detail {
Outcome<EconomicSupplyPolicyTermsSource> terms(const PoliticalTakeoverContext&,const PoliticalTakeoverRegistration&,
    const PoliticalOriginalResponsibility&,const StableId&,const std::optional<StableId>& before_approval);
Outcome<bool> active(const PoliticalTakeoverContext&,const PoliticalTakeoverRegistration&,const EconomicSupplyPolicyTermsSource&,
    Integer tick,Integer sequence,const StableId& boundary);
}
