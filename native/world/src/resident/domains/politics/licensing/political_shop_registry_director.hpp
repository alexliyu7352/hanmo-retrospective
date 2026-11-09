// 有界提案沿原Director轮次；远方只用本人原职权及作者原院地址，不读取当地接管结果。
#pragma once
#include "political_shop_registry_intents.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_shop_registry_acceptance(
 const PoliticalShopRegistryContext&,const PoliticalTakeoverRead&,const TravelCatalog&,const KnownSubjectView&,
 const DirectorPolicy&,const PoliticalShopRegistryIntent&,const DecisionProposal&,Integer candidate_limit);
}
