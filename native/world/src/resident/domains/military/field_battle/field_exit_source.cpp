#include "field_exit_source.hpp"
#include "field_contact_origin.hpp"
#include "field_segment_settlement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
namespace {
struct RoadExitValue {
  FieldRouteExitProjectionRead projection;
  OnRouteLocation location;
  CanonicalFraction exact_progress;
  SourceRef origin;
};
Outcome<RoadExitValue>
prepare_road_exit(const CommonStore &common, const FieldBattleWorldState &world,
                  const TravelCatalog &travel,
                  const FieldBattleCatalog &catalog, const StableId &battle,
                  const StableId &formation, ExactArithmeticBudget budget,
                  const FieldExactContactReadContext *exact_contacts) {
  auto fail = [&](const char *code) -> Outcome<RoadExitValue> {
    return {false, {}, common_failure(code, formation).error};
  };
  auto projection =
      read_field_route_exit_projection(common, world, travel, catalog, battle,
                                       formation, budget, exact_contacts);
  if (!projection.ok)
    return {false, {}, projection.error};
  const auto &binding = *projection.value.departure.binding;
  const auto *initial =
      std::get_if<FieldInitialContactBinding>(&binding.source);
  const auto *boundary =
      std::get_if<FieldBoundaryContactBinding>(&binding.source);
  const StableId &contact =
      initial ? initial->contact_id : boundary->contact_id;
  const auto *origin = common.state().facts.find(contact);
  if (!origin || origin->domain != "military" ||
      origin->kind != "field_battle_contact")
    return fail("FieldExitOriginalContactRequired");
  const FieldBattleContactRecord *initial_contact = nullptr;
  for (const auto &[id, row] : world.contacts)
    if (row.battle_id == battle) {
      if (initial_contact)
        return fail("FieldContactSourceMismatch");
      initial_contact = &row;
    }
  if (!initial_contact)
    return fail("FieldContactSourceRequired");
  auto generated = regenerate_field_contact_battle(
      travel, catalog, initial_contact->source, exact_contacts, budget);
  if (!generated.ok)
    return {false, {}, generated.error};
  // Road exits may not reinterpret a settlement cell as ordinary road. The
  // settlement branch needs its genuine P3-qualified Gate/centre source.
  const auto point = projection.value.departure.formation->position_dm;
  const auto &p = generated.value.parameters;
  if (p.cell_m <= 0 || point.x < -p.length_m * 5 || point.x >= p.length_m * 5 ||
      point.y < -p.width_m * 5 || point.y >= p.width_m * 5)
    return fail("FieldExitGeometryMismatch");
  const auto x = (point.x + p.length_m * 5) / (p.cell_m * 10);
  const auto y = (point.y + p.width_m * 5) / (p.cell_m * 10);
  const auto index = std::size_t(y * generated.value.columns + x);
  if (index >= generated.value.cells.size())
    return fail("FieldExitGeometryMismatch");
  const auto terrain = generated.value.cells[index].terrain;
  // The contract gives Settlement handoff to the actual Gate/village-centre
  // exit, not every traversable Village cell in its 300 m terrain area.
  // An ordinary boundary exit through that area remains the original off-road
  // projection; the actual settlement terminal cannot fall back to OnRoute.
  bool settlement_terminal = false;
  for (const auto &zone : generated.value.sides) {
    if (!zone.road_exit_settlement_id)
      continue;
    const auto e = zone.road_exit_dm;
    if (e.x < -p.length_m * 5 || e.x >= p.length_m * 5 ||
        e.y < -p.width_m * 5 || e.y >= p.width_m * 5)
      continue;
    const auto terminal_cell = std::size_t(
        (e.y + p.width_m * 5) / (p.cell_m * 10) * generated.value.columns +
        (e.x + p.length_m * 5) / (p.cell_m * 10));
    settlement_terminal = settlement_terminal || terminal_cell == index;
  }
  if (terrain == FieldTerrainKind::Gate || terrain == FieldTerrainKind::Wall ||
      settlement_terminal)
    return fail("FieldSettlementExitOriginalSourceRequired");
  const auto *road = travel.roads.find(projection.value.route_id);
  if (!road || road->duration_ticks <= 0 ||
      road->duration_ticks > std::numeric_limits<Integer>::max() / 100)
    return fail("InvalidFieldBattleRoute");
  auto limit = normalize_fraction(std::to_string(road->duration_ticks * 100),
                                  "1", budget);
  if (!limit.ok)
    return {false, {}, limit.error};
  auto progress =
      multiply_fractions(projection.value.route_fraction, limit.value, budget);
  if (!progress.ok)
    return {false, {}, progress.error};
  auto legacy = floor_fraction(progress.value, budget);
  if (!legacy.ok)
    return {false, {}, legacy.error};
  return {true,
          {std::move(projection.value),
           {road->id, road->origin, legacy.value},
           progress.value,
           {origin->domain, origin->kind, origin->id}},
          {}};
}
bool same_time(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick == b.tick && a.t == b.t;
}
} // namespace
Outcome<FieldRoadExitSourceRead> read_field_road_exit_source(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const SourceRef &source,
    const FieldSegmentApplicationRead &clock, ExactArithmeticBudget budget,
    const FieldExactContactReadContext *exact_contacts) {
  auto fail = [&]() -> Outcome<FieldRoadExitSourceRead> {
    return {false,
            {},
            common_failure("FieldExitOriginalSourceMismatch", source.id).error};
  };
  if (!clock.bound_to(common, military, world))
    return fail();
  auto actual_now = clock.read_current_moment();
  if (!actual_now.ok)
    return {false, {}, actual_now.error};
  const auto *head = common.state().facts.find(source.id);
  auto row = world.exits.find(source.id);
  if (source.domain != "military" || source.kind != "field_battle_exit" ||
      !head || head->domain != source.domain || head->kind != source.kind ||
      row == world.exits.end() || row->second.fact_id != source.id)
    return fail();
  const auto &r = row->second;
  if (!r.exact_route_progress || r.applied_at.half_day_tick != r.recorded_tick)
    return fail();
  auto canonical =
      compare_world_moments(r.applied_at, actual_now.value, budget);
  if (!canonical.ok)
    return {false, {}, canonical.error};
  if (canonical.value > 0)
    return fail();
  for (const auto &[id, other] : world.exits)
    if (id != source.id && other.battle_id == r.battle_id &&
        other.formation_id == r.formation_id)
      return fail();
  auto expected = prepare_road_exit(common, world, travel, catalog, r.battle_id,
                                    r.formation_id, budget, exact_contacts);
  if (!expected.ok)
    return {false, {}, expected.error};
  const auto &e = expected.value;
  const auto *location = std::get_if<OnRouteLocation>(&r.location);
  if (!location || location->edge_id != e.location.edge_id ||
      location->origin != e.location.origin ||
      location->progress != e.location.progress ||
      r.army_id != e.projection.departure.formation->army_id ||
      !same_time(r.at, *e.projection.departure.formation->withdrawn_at) ||
      r.field_point_dm.x != e.projection.departure.formation->position_dm.x ||
      r.field_point_dm.y != e.projection.departure.formation->position_dm.y ||
      r.source_action_id != e.projection.departure.binding->action_id ||
      r.origin.domain != e.origin.domain || r.origin.kind != e.origin.kind ||
      r.origin.id != e.origin.id ||
      r.named_actor_ids.size() !=
          e.projection.departure.surviving_actor_ids.size())
    return fail();
  auto same_progress =
      compare_fractions(*r.exact_route_progress, e.exact_progress, budget);
  if (!same_progress.ok)
    return {false, {}, same_progress.error};
  if (same_progress.value != 0)
    return fail();
  for (std::size_t i = 0; i < r.named_actor_ids.size(); ++i)
    if (r.named_actor_ids[i] != *e.projection.departure.surviving_actor_ids[i])
      return fail();
  auto from = read_actual_common_fact_position(common, r.origin);
  auto to = read_actual_common_fact_position(common, source);
  if (!from.ok || !to.ok)
    return {false, {}, !from.ok ? from.error : to.error};
  if (from.value >= to.value)
    return fail();
  return {true, {&r, std::move(expected.value.projection)}, {}};
}
Outcome<SourceRef> record_field_road_exit_in_candidate(
    CommonStore &common, const MilitaryState &military,
    FieldBattleWorldState &world, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const StableId &battle,
    const StableId &formation, const FieldSegmentApplicationRead &clock,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget budget,
    const FieldExactContactReadContext *exact_contacts) {
  auto fail = [&](const char *code) -> Outcome<SourceRef> {
    return {false, {}, common_failure(code, formation).error};
  };
  if (!clock.bound_to(common, military, world))
    return fail("FieldExitWorldMismatch");
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto canonical = compare_world_moments(now.value, now.value, budget);
  if (!canonical.ok)
    return {false, {}, canonical.error};
  for (const auto &[id, row] : world.exits)
    if (row.battle_id == battle && row.formation_id == formation) {
      SourceRef existing{"military", "field_battle_exit", id};
      auto checked =
          read_field_road_exit_source(common, military, world, travel, catalog,
                                      existing, clock, budget, exact_contacts);
      if (!checked.ok)
        return {false, {}, checked.error};
      return {true, std::move(existing), {}};
    }
  auto prepared = prepare_road_exit(common, world, travel, catalog, battle,
                                    formation, budget, exact_contacts);
  if (!prepared.ok)
    return {false, {}, prepared.error};
  const auto &p = prepared.value;
  if (!same_time(p.projection.departure.session->now,
                 *p.projection.departure.formation->withdrawn_at))
    return fail("FieldExitActualApplicationBoundaryRequired");
  if (common.state().next_instance_id <= 0 ||
      common.state().next_instance_id == std::numeric_limits<Integer>::max())
    return fail("FieldExitSequenceExhausted");
  char digits[32];
  auto serial = std::to_chars(digits, digits + sizeof(digits),
                              common.state().next_instance_id);
  if (serial.ec != std::errc{})
    return fail("FieldExitSequenceExhausted");
  ProducedStableIdBuildView produced{
      "field_battle_exit",
      std::string_view(digits, std::size_t(serial.ptr - digits))};


  (owner).grow(0);
  auto id = common.next_id("field_battle_exit");
  if (!id.ok)
    return {false, {}, id.error};
  FieldBattleExitRecord row;
  row.fact_id = id.value;
  row.battle_id = battle;
  row.formation_id = formation;
  row.army_id = p.projection.departure.formation->army_id;
  row.at = *p.projection.departure.formation->withdrawn_at;
  row.recorded_tick = now.value.half_day_tick;
  row.applied_at = now.value;
  row.field_point_dm = p.projection.departure.formation->position_dm;
  row.location = p.location;
  row.exact_route_progress = p.exact_progress;
  row.source_action_id = p.projection.departure.binding->action_id;
  row.origin = p.origin;
  for (const auto *actor : p.projection.departure.surviving_actor_ids)
    row.named_actor_ids.push_back(*actor);
  auto added = common.append_fact({id.value, "field_battle_exit", "military"});
  if (!added.ok)
    return {false, {}, added.error};
  if (!world.exits.emplace(id.value, std::move(row)).second)
    return fail("FieldExitIdRepeated");
  SourceRef result{"military", "field_battle_exit", id.value};
  auto checked =
      read_field_road_exit_source(common, military, world, travel, catalog,
                                  result, clock, budget, exact_contacts);
  if (!checked.ok)
    return {false, {}, checked.error};
  return {true, std::move(result), {}};
}
} // namespace hanmo::resident
