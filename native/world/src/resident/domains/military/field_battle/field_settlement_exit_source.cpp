#include "field_settlement_exit_source.hpp"
#include "field_author_data.hpp"
#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "field_exit_policy.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
namespace {
struct PreparedSettlementExit {
  FieldDepartureSourceRead departure;
  SettlementLocation location;
  SourceRef origin;
};
Outcome<PreparedSettlementExit>
prepare(const FieldExactContactReadContext &x,
        const PoliticalRelationsReadContext &relations,
        const PoliticalTakeoverTerritoryAccess *territory,
        const FieldBattleCatalog &catalog, const StableId &battle,
        const StableId &formation, ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<PreparedSettlementExit> {
    return {false, {}, common_failure(code, formation).error};
  };
  auto departed =
      read_field_actual_departure_source(x.common, x.field, battle, formation);
  if (!departed.ok)
    return {false, {}, departed.error};
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : x.field.contacts)
    if (row.battle_id == battle) {
      if (contact)
        return fail("FieldContactSourceMismatch");
      contact = &row;
    }
  if (!contact)
    return fail("FieldContactSourceRequired");
  SourceRef origin{"military", "field_battle_contact", contact->contact_id};
  auto allowed = read_field_exits_at_original_contact(
      x, relations, territory, catalog, origin, departed.value.formation->side,
      budget);
  if (!allowed.ok)
    return {false, {}, allowed.error};
  if (!allowed.value.settlement_id || !allowed.value.settlement_cell)
    return fail("FieldSettlementExitOriginalPermissionRequired");
  auto field = regenerate_field_contact_battle(x.travel, catalog,
                                               contact->source, &x, budget);
  if (!field.ok)
    return {false, {}, field.error};
  const auto &p = field.value.parameters;
  const auto at = departed.value.formation->position_dm;
  if (p.cell_m <= 0 || at.x < -p.length_m * 5 || at.x >= p.length_m * 5 ||
      at.y < -p.width_m * 5 || at.y >= p.width_m * 5)
    return fail("FieldSettlementExitOriginalCellRequired");
  const auto cell = std::size_t((at.y + p.width_m * 5) / (p.cell_m * 10) *
                                    field.value.columns +
                                (at.x + p.length_m * 5) / (p.cell_m * 10));
  if (cell != *allowed.value.settlement_cell ||
      departed.value.session->sides[departed.value.formation->side].exits !=
          allowed.value.cells)
    return fail("FieldSettlementExitOriginalCellRequired");
  const auto town = x.travel.settlements.find(*allowed.value.settlement_id);
  if (!town || town->entry_place.empty())
    return fail("FieldSettlementExitOriginalCatalogRequired");
  return {true,
          {std::move(departed.value),
           {town->id, town->entry_place},
           std::move(origin)},
          {}};
}
bool same_time(FieldBattleTime a, FieldBattleTime b) {
  return a.tick == b.tick && a.t == b.t;
}
} // namespace
Outcome<FieldSettlementExitSourceRead> read_field_settlement_exit_source(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess *territory,
    const FieldBattleCatalog &catalog, const SourceRef &source,
    ExactArithmeticBudget budget) {
  auto fail = [&]() -> Outcome<FieldSettlementExitSourceRead> {
    return {
        false,
        {},
        common_failure("FieldSettlementExitOriginalSourceMismatch", source.id)
            .error};
  };
  if (!x.clock.bound_to(x.common, x.military, x.field))
    return fail();
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  const auto row = x.field.exits.find(source.id);
  const auto head = x.common.state().facts.find(source.id);
  if (source.domain != "military" || source.kind != "field_battle_exit" ||
      !head || head->domain != source.domain || head->kind != source.kind ||
      row == x.field.exits.end() || row->second.fact_id != source.id)
    return fail();
  const auto &r = row->second;
  auto no_future = compare_world_moments(r.applied_at, now.value, budget);
  if (!no_future.ok)
    return {false, {}, no_future.error};
  if (no_future.value > 0 || r.recorded_tick != r.applied_at.half_day_tick ||
      r.exact_route_progress)
    return fail();
  auto expected = prepare(x, relations, territory, catalog, r.battle_id,
                          r.formation_id, budget);
  if (!expected.ok)
    return {false, {}, expected.error};
  const auto &e = expected.value;
  const auto location = std::get_if<SettlementLocation>(&r.location);
  if (!location || location->settlement_id != e.location.settlement_id ||
      location->place_id != e.location.place_id ||
      r.army_id != e.departure.formation->army_id ||
      !same_time(r.at, *e.departure.formation->withdrawn_at) ||
      r.field_point_dm.x != e.departure.formation->position_dm.x ||
      r.field_point_dm.y != e.departure.formation->position_dm.y ||
      r.source_action_id != e.departure.binding->action_id ||
      !same_source_ref(r.origin, e.origin) ||
      r.named_actor_ids.size() != e.departure.surviving_actor_ids.size())
    return fail();
  for (std::size_t i = 0; i < r.named_actor_ids.size(); ++i)
    if (r.named_actor_ids[i] != *e.departure.surviving_actor_ids[i])
      return fail();
  for (const auto &[id, other] : x.field.exits)
    if (id != source.id && other.battle_id == r.battle_id &&
        other.formation_id == r.formation_id)
      return fail();
  auto from = read_actual_common_fact_position(x.common, r.origin);
  auto to = read_actual_common_fact_position(x.common, source);
  if (!from.ok)
    return {false, {}, from.error};
  if (!to.ok)
    return {false, {}, to.error};
  if (from.value >= to.value)
    return fail();
  return {true, {&r, std::move(expected.value.departure)}, {}};
}
Outcome<SourceRef> record_field_settlement_exit_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess *territory,
    const FieldBattleCatalog &catalog, const StableId &battle,
    const StableId &formation, CheckpointMemoryCharge &owner,
    ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<SourceRef> {
    return {false, {}, common_failure(code, formation).error};
  };
  if (&common != &x.common || &world != &x.field ||
      !x.clock.bound_to(common, x.military, world))
    return fail("CrossCandidateFieldSettlementExit");
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto canonical = compare_world_moments(now.value, now.value, budget);
  if (!canonical.ok)
    return {false, {}, canonical.error};
  for (const auto &[id, row] : world.exits)
    if (row.battle_id == battle && row.formation_id == formation) {
      SourceRef existing{"military", "field_battle_exit", id};
      auto checked = read_field_settlement_exit_source(
          x, relations, territory, catalog, existing, budget);
      if (!checked.ok)
        return {false, {}, checked.error};
      return {true, std::move(existing), {}};
    }
  auto ready =
      prepare(x, relations, territory, catalog, battle, formation, budget);
  if (!ready.ok)
    return {false, {}, ready.error};
  const auto &p = ready.value;
  if (!same_time(p.departure.session->now,
                 *p.departure.formation->withdrawn_at))
    return fail("FieldExitActualApplicationBoundaryRequired");
  if (common.state().next_instance_id <= 0 ||
      common.state().next_instance_id == std::numeric_limits<Integer>::max())
    return fail("FieldExitSequenceExhausted");
  char digits[32];
  const auto serial = std::to_chars(digits, digits + sizeof(digits),
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
  FieldBattleExitRecord r;
  r.fact_id = id.value;
  r.battle_id = battle;
  r.formation_id = formation;
  r.army_id = p.departure.formation->army_id;
  r.at = *p.departure.formation->withdrawn_at;
  r.recorded_tick = now.value.half_day_tick;
  r.applied_at = now.value;
  r.field_point_dm = p.departure.formation->position_dm;
  r.location = p.location;
  r.source_action_id = p.departure.binding->action_id;
  r.origin = p.origin;
  for (const auto actor : p.departure.surviving_actor_ids)
    r.named_actor_ids.push_back(*actor);
  auto added = common.append_fact({id.value, "field_battle_exit", "military"});
  if (!added.ok)
    return {false, {}, added.error};
  if (!world.exits.emplace(id.value, std::move(r)).second)
    return fail("FieldExitIdRepeated");
  SourceRef result{"military", "field_battle_exit", id.value};
  auto checked = read_field_settlement_exit_source(x, relations, territory,
                                                   catalog, result, budget);
  if (!checked.ok)
    return {false, {}, checked.error};
  return {true, std::move(result), {}};
}
Outcome<const FieldBattleExitRecord *> read_field_actual_exit_application(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const TravelCatalog &travel,
    const FieldBattleCatalog &catalog, const SourceRef &source,
    const FieldSegmentApplicationRead &clock, ExactArithmeticBudget budget,
    const FieldExactContactReadContext *exact,
    const PoliticalRelationsReadContext *relations,
    const PoliticalTakeoverTerritoryAccess *territory) {
  auto row = world.exits.find(source.id);
  if (row == world.exits.end())
    return {false, nullptr,
            common_failure("FieldExitOriginalSourceMismatch", source.id).error};
  if (std::holds_alternative<OnRouteLocation>(row->second.location)) {
    auto read = read_field_road_exit_source(
        common, military, world, travel, catalog, source, clock, budget, exact);
    if (!read.ok)
      return {false, nullptr, read.error};
    return {true, read.value.application, {}};
  }
  if (!exact || !relations || &exact->common != &common ||
      &exact->military != &military || &exact->field != &world ||
      &exact->travel != &travel || &exact->clock != &clock)
    return {
        false, nullptr,
        common_failure("FieldSettlementExitOriginalOwnerRequired", source.id)
            .error};
  auto read = read_field_settlement_exit_source(*exact, *relations, territory,
                                                catalog, source, budget);
  if (!read.ok)
    return {false, nullptr, read.error};
  return {true, read.value.application, {}};
}
Outcome<SourceRef> record_field_exit_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess *territory,
    const FieldBattleCatalog &catalog, const StableId &battle,
    const StableId &formation, CheckpointMemoryCharge &owner,
    ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<SourceRef> {
    return {false, {}, common_failure(code, formation).error};
  };
  if (&common != &x.common || &world != &x.field ||
      !x.clock.bound_to(common, x.military, world))
    return fail("CrossCandidateFieldExit");
  auto departure =
      read_field_actual_departure_source(common, world, battle, formation);
  if (!departure.ok)
    return {false, {}, departure.error};
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : world.contacts)
    if (row.battle_id == battle) {
      if (contact)
        return fail("FieldContactSourceMismatch");
      contact = &row;
    }
  if (!contact)
    return fail("FieldContactSourceRequired");
  auto allowed = read_field_exits_at_original_contact(
      x, relations, territory, catalog,
      {"military", "field_battle_contact", contact->contact_id},
      departure.value.formation->side, budget);
  if (!allowed.ok)
    return {false, {}, allowed.error};
  auto field = regenerate_field_contact_battle(x.travel, catalog,
                                               contact->source, &x, budget);
  if (!field.ok)
    return {false, {}, field.error};
  const auto &p = field.value.parameters;
  const auto at = departure.value.formation->position_dm;
  if (p.cell_m <= 0 || at.x < -p.length_m * 5 || at.x >= p.length_m * 5 ||
      at.y < -p.width_m * 5 || at.y >= p.width_m * 5)
    return fail("FieldExitGeometryMismatch");
  const auto cell = std::size_t((at.y + p.width_m * 5) / (p.cell_m * 10) *
                                    field.value.columns +
                                (at.x + p.length_m * 5) / (p.cell_m * 10));
  // 總設計師 2026-10-29 Slack 批准，1793335838.005349：
  // only an actual terminal Routing departure may leave from its present cell.
  // The existing road/settlement/off-road projection below remains unchanged.
  if (std::find(allowed.value.cells.begin(), allowed.value.cells.end(), cell) ==
          allowed.value.cells.end() &&
      !is_field_terminal_routing_departure(departure.value))
    return fail("FieldExitActualQualifiedCellRequired");
  if (allowed.value.settlement_cell && *allowed.value.settlement_cell == cell) {
    return record_field_settlement_exit_in_candidate(
        common, world, x, relations, territory, catalog, battle, formation,
        owner, budget);
  }
  return record_field_road_exit_in_candidate(
      common, x.military, world, x.travel, catalog, battle, formation, x.clock,
      owner, budget, &x);
}
Outcome<SourceRef> record_field_exit_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess &territory,
    const FieldBattleCatalog &catalog, const StableId &battle,
    const StableId &formation, CheckpointMemoryCharge &owner,
    ExactArithmeticBudget budget) {
  return record_field_exit_in_candidate(
      common, world, x, relations, &territory, catalog, battle, formation, owner,
      budget);
}
// 旧 takeover 调用仍经同一原完整分支，不改变历史ABI。
Outcome<FieldSettlementExitSourceRead> read_field_settlement_exit_source(
    const FieldExactContactReadContext &x, const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess &territory, const FieldBattleCatalog &catalog,
    const SourceRef &source, ExactArithmeticBudget budget) {
  return read_field_settlement_exit_source(x, relations, &territory, catalog, source, budget);
}
// 旧 writer 引用入口只转发，不产生第二次离场或伪位置。
Outcome<SourceRef> record_field_settlement_exit_in_candidate(
    CommonStore &common, FieldBattleWorldState &world,
    const FieldExactContactReadContext &x, const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess &territory, const FieldBattleCatalog &catalog,
    const StableId &battle, const StableId &formation, CheckpointMemoryCharge &owner,
    ExactArithmeticBudget budget) {
  return record_field_settlement_exit_in_candidate(
      common, world, x, relations, &territory, catalog, battle, formation, owner, budget);
}
} // namespace hanmo::resident
