// 代理签字借原控制/占领纯史，不反调Task、赈济或政治完整账。
#pragma once
#include "political_governance_agency_contract.hpp"
#include "resident/domains/politics/succession/political_succession_control.hpp"
namespace hanmo::resident {
Outcome<StableId> read_political_governance_agency_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& actual_fact);
// phase仅容后发原占领来源到to；本业务签字和实际before边界始终在已提交Common钟内。
struct PoliticalGovernanceAgencyTerritoryContext {const CommonStore& common;const PoliticsState& politics;const PoliticsCatalog& catalog;const PoliticalSuccessionOccupationContext* occupations=nullptr;const DynamicStepInput* phase=nullptr;};
Status validate_political_governance_agency_seat(const PoliticalGovernanceAgencyTerritoryContext&,const PoliticalGovernanceAgencyPolicy&,Integer tick,const StableId& actual_fact={});
}
