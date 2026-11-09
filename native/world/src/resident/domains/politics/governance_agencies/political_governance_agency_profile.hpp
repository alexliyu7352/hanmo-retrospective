// 显式独立尾选择治理代理，不占满32位，不改旧Authority或政治effects字节形状。
#pragma once
#include "political_governance_agency_business.hpp"
namespace hanmo::resident {
Outcome<Integer> political_governance_agency_selected_version(const std::vector<std::pair<StableId,StableId>>&);
Outcome<std::optional<PoliticalGovernanceAgencyCatalog>> decode_political_governance_agency_profile(const BoundaryValue&,const std::vector<StableId>&);
Status validate_political_governance_agency_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,const PoliticalGovernanceAgencyState*);
bool is_political_governance_agency_fact_kind(const StableId&);
Status validate_political_governance_agency_fact_header(const CommonStore&,const PoliticalGovernanceAgencyState&,const FactRecord&);
// 只辨独占新来源，拒把其fact/批准ID藏进旧政治结果kind。
bool political_governance_agency_effect_uses_sources(const PoliticsEffects&);
bool valid_political_governance_agency_effect(const StableId&,const PoliticsEffects&);
}
