#include "field_army_exit_route.hpp"
#include "field_author_data.hpp"
#include "field_binding_source.hpp"
#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_field_command_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/domains/interaction/immutable_authored_flags.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
using namespace movement_detail;
// 从原Field分米到同作者地图连续点仅作尺度转换；不舍入人物或替换世界位置。
Outcome<Integer> read_field_army_return_route_cost(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &abilities, const StableId &battle,
    const StableId &army, const StableId &destination,
    const FieldControlAcceptanceBoundary &cut, const FieldBattleTime &at,
    std::optional<std::size_t> candidate_exit, ExactArithmeticBudget b) {
  return guard<Integer>([&] {
    if (!x.travel_services || !x.clock.bound_to(x.common, x.military, x.field))
      fail("FieldArmyExitTravelOwnerRequired", army);
    auto weights = take(read_field_army_exit_weight_prefix(x, catalog, abilities,
                                                          battle, army, cut, at));
    const auto &session = x.field.sessions.at(battle);
    const auto now = take(x.clock.read_current_moment());
    const auto command = take(read_military_army_command_source_at_actual_prefix(
        x.common, x.military, army, cut.common_prefix, cut.observed_at, now,
        &x.formations, b));
    const FieldFormation *commander = nullptr;
    for (const auto &[id, row] : session.formations)
      if (row.army_id == army &&
          (row.commander_actor_id == command.commander_actor_id ||
           row.deputy_actor_id == command.commander_actor_id)) {
        if (commander) fail("FieldArmyExitCommanderAmbiguous", army);
        commander = &row;
      }
    if (!commander) fail("FieldArmyExitActualCommanderRequired", army);
    auto weight = std::find_if(weights.begin(), weights.end(), [&](const auto &w) {
      return w.formation_id == commander->formation_id;
    });
    if (weight == weights.end()) fail("FieldArmyExitCommanderOriginRequired", army);
    const FieldBattleContactRecord *contact = nullptr;
    for (const auto &[id, row] : x.field.contacts)
      if (row.battle_id == battle) {
        if (contact) fail("FieldArmyExitOriginalContactRequired", battle);
        contact = &row;
      }
    if (!contact) fail("FieldArmyExitOriginalContactRequired", battle);
    const auto generated = take(regenerate_field_contact_battle(
        x.travel, catalog, contact->source, &x, b));
    const auto *terrain = take(read_field_road_terrain(
        x.travel, catalog, generated.anchor.route_id));
    const auto &scale = terrain->metadata.km_per_unit;
    const auto *map = x.travel.maps.find(terrain->metadata.map_id);
    const auto *stop = x.travel.settlements.find(destination);
    if (!map || !stop || terrain->metadata.map_id != x.travel.world_map_id ||
        (!stop->world_map_id.empty() && stop->world_map_id != map->id) ||
        scale.num <= 0 || scale.den <= 0)
      fail("FieldArmyExitOriginalMapRequired", destination);
    PointI local = weight->position_dm;
    if (candidate_exit) {
      if (commander->side < 0 || commander->side > 1 ||
          *candidate_exit >= generated.cells.size() ||
          std::find(session.sides[commander->side].exits.begin(),
                    session.sides[commander->side].exits.end(), *candidate_exit) ==
              session.sides[commander->side].exits.end())
        fail("FieldArmyExitOriginalExitRequired", army);
      local = field_cell_center_dm(generated, *candidate_exit);
    }
    const auto origin = field_point_to_map_dm(generated.anchor, local);
    if (origin.x < 0 || origin.y < 0)
      fail("FieldArmyExitOriginalMapPointRequired", army);
    const auto divisor = mul(integer(scale.num, b), integer(10000, b), b);
    const auto coordinate = [&](Integer v) {
      return take(fraction_to_binary64(
          div(mul(integer(v, b), integer(scale.den, b), b), divisor, b), b));
    };
    const terrain::PointD point{coordinate(origin.x), coordinate(origin.y)};
    if (point.x < 0 || point.y < 0 || point.x >= double(map->width) ||
        point.y >= double(map->height) || stop->map_xy.x < 0 ||
        stop->map_xy.y < 0 || stop->map_xy.x >= map->width ||
        stop->map_xy.y >= map->height)
      fail("FieldArmyExitOriginalMapPointRequired", army);
    // 当前显式无flag-writer选择沿不可变作者源证明任一历史cut；绝非今日空表推断。
    if (!x.original_flags || !x.original_interaction)
      fail("FieldArmyExitOriginalFlagsRequired", battle);
    take(validate_immutable_interaction_flags(*x.original_flags, catalog.identity,
                                              *x.original_interaction));
    const auto &flags = x.travel_services->terrain_flags();
    if (flags.size() != x.original_flags->rows().size())
      fail("FieldArmyExitOriginalFlagsMismatch", battle);
    for (const auto &flag : x.original_flags->rows()) {
      const auto actual = flags.find(flag.id);
      if (actual == flags.end() || actual->second != flag.value)
        fail("FieldArmyExitOriginalFlagsMismatch", flag.id);
    }
    for (const auto &cell : map->cells)
      if (cell.blocked_flag)
        take(read_immutable_interaction_flag(*x.original_flags, catalog.identity,
                                            *x.original_interaction, *cell.blocked_flag));
    // Field是实际陆战；不借旧embarked位切船模式，也不把不可达当零成本。
    const auto route = take(terrain::plan_continuous_prepared(
        *map, point, stop->map_xy, terrain::Mode::army,
        flags, b));
    return route.cost;
  });
}
} // namespace hanmo::resident
