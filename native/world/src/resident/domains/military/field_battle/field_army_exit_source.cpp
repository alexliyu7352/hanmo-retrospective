#include "field_event_legacy_kinds.hpp"
#include "field_event_motion_application.hpp"
#include "field_army_exit_source.hpp"
#include "field_binding_source.hpp"
#include "field_contact_origin.hpp"
#include "field_exact_contact_source.hpp"
#include "field_initial_placement_source.hpp"
#include "field_integer_geometry.hpp"
#include "field_roster_origin.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 历史边界比较只比较原局部时间，不制造S0时刻。
bool no_later(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) <= std::tie(b.tick, b.t);
}
// 当前正文与原头分离验证，不拿指针非空或任意同名表当同World来源。
bool same_point(PointI a, PointI b) { return a.x == b.x && a.y == b.y; }
}
// 选择权重沿原入场/布阵、MotionApplied和原五类名册cut还原，不借今日位置。
Outcome<std::vector<FieldArmyExitWeight>> read_field_army_exit_weight_prefix(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const FieldBattleAbilityReadContext &abilities, const StableId &battle,
    const StableId &army, const FieldControlAcceptanceBoundary &cut,
    const FieldBattleTime &selected_at) {
  auto fail = [&](const char *code) -> Outcome<std::vector<FieldArmyExitWeight>> {
    return {false, {}, common_failure(code, army).error};
  };
  if (!x.clock.bound_to(x.common, x.military, x.field) ||
      abilities.exact_contacts != &x || !abilities.original_rosters ||
      &abilities.original_rosters->common != &x.common ||
      &abilities.original_rosters->military != &x.military)
    return fail("FieldArmyExitSameWorldSourcesRequired");
  const auto session = x.field.sessions.find(battle);
  if (session == x.field.sessions.end() ||
      !no_later(selected_at, session->second.now))
    return fail("FieldArmyExitActualChoiceBoundaryRequired");
  const auto &s = session->second;
  auto valid = validate_field_control_acceptance(x.common, s, cut);
  if (!valid.ok) return {false, {}, valid.error};
  auto binding = read_field_original_battle_binding(x.common, x.field, battle, army);
  if (!binding.ok) return {false, {}, binding.error};
  const FieldBattleContactRecord *contact = nullptr;
  for (const auto &[id, row] : x.field.contacts)
    if (row.battle_id == battle) {
      if (contact || id != row.contact_id)
        return fail("FieldArmyExitOriginalContactRequired");
      contact = &row;
    }
  if (!contact) return fail("FieldArmyExitOriginalContactRequired");
  auto generated = regenerate_field_contact_battle(x.travel, catalog,
                                                   contact->source, &x);
  if (!generated.ok) return {false, {}, generated.error};
  if (generated.value.field_hash != s.field_hash)
    return fail("FieldArmyExitOriginalGeometryRequired");
  std::vector<FieldArmyExitWeight> result;
  for (const auto &[id, formation] : s.formations) {
    if (formation.army_id != army || !no_later(formation.joined_at, selected_at))
      continue;
    auto initial = read_field_original_initial_placement(
        x.common, x.military, x.field, s, formation, generated.value, abilities);
    if (!initial.ok) return {false, {}, initial.error};
    if (initial.value.entry &&
        initial.value.entry->event_sequence > cut.last_local_event_sequence)
      continue;
    auto roster = read_field_local_roster_at_event_prefix(
        x.common, x.military, x.military_catalog, x.field, s, formation,
        *abilities.original_rosters, cut.last_local_event_sequence);
    if (!roster.ok) return {false, {}, roster.error};
    PointI position = initial.value.position_dm;
    std::optional<Integer> previous_motion;
    bool withdrawn = false;
    for (const auto &event : s.events) {
      if (event.sequence > cut.last_local_event_sequence) break;
      if (!no_later(event.at, selected_at))
        return fail("FieldArmyExitChoiceEventCutMismatch");
      if (event.formation_id != id) continue;
      if (event.kind == field_event_kind::MotionApplied) {
        if (!field_event_motion_application(event) || withdrawn ||
            !same_point(field_event_motion_application(event)->from_dm, position) ||
            field_event_motion_application(event)->previous_motion_sequence != previous_motion)
          return fail("FieldArmyExitOriginalMotionPrefixRequired");
        position = field_event_motion_application(event)->to_dm;
        previous_motion = event.sequence;
      }
      if (event.kind == field_event_kind::Withdrawn) withdrawn = true;
    }
    // 普通兵已归逃散不再计权；独立活将领仍保军籍，零兵编队不能凭此消失。
    const auto retained_officer = [&](const StableId &actor) {
      if (actor.empty()) return false;
      for (const auto *lost : {&roster.value.dead, &roster.value.captured, &roster.value.deserted})
        if (std::find(lost->begin(), lost->end(), actor) != lost->end()) return false;
      for (const auto &loss : formation.commander_casualties)
        if (loss.actor_id == actor && loss.loss == 2 && no_later(loss.at, selected_at))
          for (const auto &event : s.events)
            if (event.sequence <= cut.last_local_event_sequence &&
                event.kind == field_event_kind::CommanderLost &&
                event.formation_id == id && event.at.tick == loss.at.tick &&
                event.at.t == loss.at.t)
              return false;
      return true;
    };
    if (withdrawn && roster.value.fit.empty() && roster.value.wounded.empty() &&
        !retained_officer(formation.commander_actor_id) &&
        (!formation.deputy_actor_id || !retained_officer(*formation.deputy_actor_id)))
      continue;
    if (roster.value.fit.size() > std::size_t(std::numeric_limits<Integer>::max()) ||
        roster.value.wounded.size() > std::size_t(std::numeric_limits<Integer>::max()))
      return fail("FieldArmyExitRosterBoundRequired");
    result.push_back({id, position, Integer(roster.value.fit.size()),
                      Integer(roster.value.wounded.size())});
  }
  if (result.empty()) return fail("FieldArmyExitActualFormationRequired");
  return {true, std::move(result), {}};
}
// 相等总距离沿原格序稳定选择；不引RNG、费用或新作者几何参数。
Outcome<std::size_t> read_field_army_weighted_exit_cell(
    const GeneratedBattleField &field,
    const std::vector<FieldArmyExitWeight> &weights,
    const std::vector<std::size_t> &eligible) {
  auto fail = [&](const char *code) -> Outcome<std::size_t> {
    return {false, {}, common_failure(code).error};
  };
  auto parameters = validate_field_battle_parameters(field.parameters);
  if (!parameters.ok) return {false, {}, parameters.error};
  if (field.columns <= 0 || field.rows <= 0 ||
      field.columns != field.parameters.length_m / field.parameters.cell_m ||
      field.rows != field.parameters.width_m / field.parameters.cell_m ||
      __int128_t(field.columns) * field.rows != field.cells.size() ||
      weights.empty() || weights.size() > std::size_t(field.parameters.max_formations_per_side))
    return fail("FieldArmyExitOriginalGeometryRequired");
  std::set<StableId> formations;
  for (const auto &w : weights)
    if (w.formation_id.empty() || !formations.insert(w.formation_id).second ||
        w.fit < 0 || w.wounded < 0 ||
        w.position_dm.x < -field.parameters.length_m * 5 ||
        w.position_dm.x >= field.parameters.length_m * 5 ||
        w.position_dm.y < -field.parameters.width_m * 5 ||
        w.position_dm.y >= field.parameters.width_m * 5)
      return fail("FieldArmyExitOriginalWeightRequired");
  std::optional<std::size_t> selected;
  __int128_t best = 0;
  std::set<std::size_t> unique;
  for (auto cell : eligible) {
    if (cell >= field.cells.size() || !unique.insert(cell).second)
      return fail("FieldArmyExitOriginalExitRequired");
    const auto point = field_cell_center_dm(field, cell);
    __int128_t sum = 0;
    for (const auto &w : weights) {
      const auto distance = field_integer_sqrt(
          __uint128_t(field_distance_squared(w.position_dm, point)));
      __int128_t term;
      if (__builtin_mul_overflow(__int128_t(w.fit) + w.wounded,
                                 __int128_t(distance), &term) ||
          __builtin_add_overflow(sum, term, &sum))
        return fail("FieldArmyExitWeightedDistanceOverflow");
    }
    if (!selected || sum < best || (sum == best && cell < *selected)) {
      selected = cell;
      best = sum;
    }
  }
  if (!selected) return fail("FieldArmyExitReachableDestinationRequired");
  return {true, *selected, {}};
}
} // namespace hanmo::resident
