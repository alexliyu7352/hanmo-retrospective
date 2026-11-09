// 经营续签只补原Director已到期主体的无命令提案，不另建轮询器或突破责任优先序。
#pragma once
#include "politics_supply_renewal_catalog.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_supply_renewal(
    const PoliticalTakeoverContext&,const EconomicSupplyPolicyContext&,const KnownSubjectView&,
    const DirectorPolicy&,const PoliticalSupplyRenewalIntent&,const DecisionProposal&,Integer candidate_limit);
}
