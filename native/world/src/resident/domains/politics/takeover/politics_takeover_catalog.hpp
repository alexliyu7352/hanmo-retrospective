// 作者显式指定可接管据点、接任官职和整合规则；不存在隐式全境接管许可。
#pragma once
#include "politics_takeover_state.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
struct MilitaryOccupationFact;
Status validate_political_takeover_catalog(const CommonCatalog&,const OrganizationCatalog&,
    const PoliticsCatalog&,const MilitaryCatalog&,const PoliticalTakeoverCatalog&);
Outcome<PoliticalTakeoverCatalog> decode_political_takeover_catalog(const BoundaryValue&);
BoundaryValue encode_political_takeover_catalog(const PoliticalTakeoverCatalog&);
// 同一据点可以为不同合法接任组织配置规则；一份实际接管最多命中一条。
Outcome<std::optional<StableId>> political_takeover_rule_for_occupation(
    const OrganizationCatalog&,const PoliticalTakeoverCatalog&,const MilitaryOccupationFact&);
}
