// 释放落点纯报价：全部真实 S1 据点共用同一控制截面与 S1 道路距离，不写人物。
#pragma once
#include "named_military_custody_terms.hpp"
#include "named_military_custody_territory.hpp"
#include "resident/domains/travel/road_geometry_distance.hpp"
namespace hanmo::resident {
class PoliticalTakeoverTerritoryAccess;
struct NamedMilitaryCustodyDestination {
  Integer priority = 0;
  StableId settlement_id, jurisdiction_id;
  CanonicalFraction road_distance;
  std::vector<StableId> road_ids;
  bool initial_returning = false;
  std::optional<SettlementLocation> destination;
};
// origin 必须由调用者的真实 S1 spatial source 核证，本纯报价本身不授予释放权限。
Outcome<NamedMilitaryCustodyDestination> quote_named_military_custody_destination(
    const NamedMilitaryCustodyContext &, const TravelCatalog &,
    const NamedMilitaryCustodyTerritorySources &, const NamedMilitaryCustodyTerm &,
    const MovementAnchor &origin, ExactArithmeticBudget = {});
// closure 必须为同 Military 根的真实行；所有候选均读该原头前历史控制，非今日控制。
Outcome<NamedMilitaryCustodyDestination> quote_named_military_custody_destination_before_release(
    const NamedMilitaryCustodyContext &, const TravelCatalog &,
    const NamedMilitaryCustodyTerritorySources &, const NamedMilitaryCustodyRelease &,
    ExactArithmeticBudget = {});
// 旧 concrete 入口保持原签名，只委托有限借用适配，不改变原接管读口。
Outcome<NamedMilitaryCustodyDestination> quote_named_military_custody_destination(
    const NamedMilitaryCustodyContext &, const TravelCatalog &,
    const PoliticalTakeoverTerritoryAccess &, const NamedMilitaryCustodyTerm &,
    const MovementAnchor &origin, ExactArithmeticBudget = {});
Outcome<NamedMilitaryCustodyDestination> quote_named_military_custody_destination_before_release(
    const NamedMilitaryCustodyContext &, const TravelCatalog &,
    const PoliticalTakeoverTerritoryAccess &, const NamedMilitaryCustodyRelease &,
    ExactArithmeticBudget = {});
} // namespace hanmo::resident
