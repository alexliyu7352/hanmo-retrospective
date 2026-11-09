#include "field_army_exit_placement.hpp"
#include "field_ability_sources.hpp"
#include "field_author_data.hpp"
#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "field_exit_policy.hpp"
#include "field_exit_projection.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/travel.hpp"
#include <limits>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 真source调用者已经证明point；这里只选原几何分支，城村权限沿原contact归属源。
MovementAnchor original_point_anchor(const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a,
    const StableId &battle, Integer side, PointI point, const SourceRef &source,
    ExactArithmeticBudget b) {
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : x.field.contacts) if (row.battle_id == battle) {
    if (contact) fail("FieldArmyExitOriginalContactRequired", battle);
    contact = &row;
  }
  if (!contact || side < 0 || side > 1) fail("FieldArmyExitOriginalContactRequired", battle);
  const auto field = take(regenerate_field_contact_battle(x.travel, catalog, contact->source, &x, b));
  const auto &p = field.parameters;
  if (p.cell_m <= 0 || point.x < -p.length_m * 5 || point.x >= p.length_m * 5 ||
      point.y < -p.width_m * 5 || point.y >= p.width_m * 5)
    fail("FieldExitGeometryMismatch", source.id);
  const auto cell = std::size_t((point.y + p.width_m * 5) / (p.cell_m * 10) * field.columns +
                                 (point.x + p.length_m * 5) / (p.cell_m * 10));
  if (cell >= field.cells.size()) fail("FieldExitGeometryMismatch", source.id);
  bool settlement_terminal = false;
  for (const auto &zone : field.sides) {
    if (!zone.road_exit_settlement_id) continue;
    const auto end = zone.road_exit_dm;
    if (end.x < -p.length_m * 5 || end.x >= p.length_m * 5 ||
        end.y < -p.width_m * 5 || end.y >= p.width_m * 5) continue;
    const auto terminal_cell = std::size_t((end.y + p.width_m * 5) / (p.cell_m * 10) * field.columns +
                                           (end.x + p.length_m * 5) / (p.cell_m * 10));
    settlement_terminal = settlement_terminal || terminal_cell == cell;
  }
  if (field.cells[cell].terrain == FieldTerrainKind::Gate ||
      field.cells[cell].terrain == FieldTerrainKind::Wall || settlement_terminal) {
    if (!a.diplomacy) fail("FieldSettlementExitOriginalPermissionRequired", source.id);
    const auto allowed = take(read_field_exits_at_original_contact(x, *a.diplomacy,
        a.territory, catalog, {"military", "field_battle_contact", contact->contact_id}, side, b));
    if (!allowed.settlement_cell || *allowed.settlement_cell != cell || !allowed.settlement_id)
      fail("FieldSettlementExitOriginalPermissionRequired", source.id);
    const auto *town = x.travel.settlements.find(*allowed.settlement_id);
    if (!town || town->entry_place.empty()) fail("FieldSettlementExitOriginalCatalogRequired", source.id);
    return OriginalMovementAnchor{SettlementLocation{town->id, town->entry_place}, source};
  }
  const auto projection = take(read_field_point_route_projection(x.field, x.travel,
      catalog, battle, point, b, &x));
  const auto *road = x.travel.roads.find(projection.route_id);
  if (!road || road->duration_ticks <= 0 || road->duration_ticks > std::numeric_limits<Integer>::max() / 100)
    fail("InvalidFieldBattleRoute", source.id);
  const auto progress = mul(projection.route_fraction, integer(road->duration_ticks * 100, b), b);
  return RoadFractionalAnchor{road->id, road->origin, progress, source};
}
}
// 领域内共用原投影；真实原因/时刻/名单由调用它的完整原source reader证明。
Outcome<MovementAnchor> project_field_army_exit_actual_point(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const StableId &battle, Integer side,
    PointI point, const SourceRef &source, ExactArithmeticBudget b) {
  return guard<MovementAnchor>([&] { return original_point_anchor(x, catalog, a, battle, side, point, source, b); });
}
// 每个普通兵的唯一实际末点来自被证的那条motion，不从编队当前缓存或主将位置聚拢。
Outcome<MovementAnchor> read_field_army_exit_ordinary_anchor(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &a, const SourceRef &source,
    const StableId &actor, ExactArithmeticBudget b) {
  return guard<MovementAnchor>([&] {
    const auto actual = take(read_field_army_exit_ordinary_departure_source(x, catalog, a, source, actor));
    return original_point_anchor(x, catalog, a, actual.source.plan->battle_id,
        actual.formation->side, actual.motion->to_dm, source, b);
  });
}
} // namespace hanmo::resident
