// 继位借原Family控制与原辖区控制纯来源，不回调完整任务/私铺/接管责任全验。
#pragma once
#include "political_succession_contract.hpp"
#include "resident/domains/politics/politics_model.hpp"
namespace hanmo::resident {
struct FamilyState;struct FamilyLegacyState;struct PoliticalTakeoverState;struct PoliticalTakeoverCatalog;
struct MilitaryState;struct MilitaryCatalog;struct MilitaryAccess;
// 完整Family及Legacy/弟子控制史先验后才调用，精确五业务头拒任意事实边界。
Outcome<StableId> read_political_succession_player_before_fact(const CommonStore&,const FamilyState&,const FamilyLegacyState*,Integer tick,const StableId& boundary);
struct PoliticalSuccessionOccupationContext {const PoliticalTakeoverState& state;const PoliticalTakeoverCatalog& catalog;const MilitaryState& military;const MilitaryCatalog& military_catalog;const MilitaryAccess& military_access;};
struct PoliticalSuccessionTerritoryContext {const CommonStore& common;const PoliticsState& politics;const PoliticsCatalog& catalog;const PoliticalSuccessionOccupationContext* occupations=nullptr;};
// 原首职政策尚无接管后续签语义；任何该截面前占领均明确要求独立连续性权源。
Status validate_political_succession_seat(const PoliticalSuccessionTerritoryContext&,const PoliticalSuccessionPolicy&,Integer tick,const StableId& boundary={});
}
