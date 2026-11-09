#include "field_event_legacy_kinds.hpp"
#include "field_event_contact_phase_from.hpp"
#include "resident/model/common_model.hpp"
#include "field_contacts.hpp"
#include "field_deployment.hpp"
#include "field_integer_geometry.hpp"
#include "field_navigation.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool present(const FieldFormation &f) {
  return !f.fit.empty() && f.state != FieldFormationState::Withdrawn &&
         f.state != FieldFormationState::Destroyed &&
         f.state != FieldFormationState::Deploying;
}
// 生成格仍属同场；不把越界位置夹到边缘格。
Outcome<const FieldCell *> terrain(const GeneratedBattleField &field,
                                   PointI p) {
  const auto &v = field.parameters;
  if (p.x < -v.length_m * 5 || p.x >= v.length_m * 5 || p.y < -v.width_m * 5 ||
      p.y >= v.width_m * 5)
    return {false, {}, common_failure("FieldContactPointOutsideField").error};
  const auto x = (p.x + v.length_m * 5) / (v.cell_m * 10);
  const auto y = (p.y + v.width_m * 5) / (v.cell_m * 10);
  return {
      true, &field.cells[static_cast<std::size_t>(y * field.columns + x)], {}};
}
Integer facing(PointI attacker, PointI target, Integer target_facing,
               bool routing) {
  if (routing)
    return 3;
  const auto angle =
      field_direction_mdeg({attacker.x - target.x, attacker.y - target.y});
  auto difference = (angle - target_facing + 360000) % 360000;
  difference = std::min(difference, 360000 - difference);
  return difference < 60000 ? 0 : difference <= 120000 ? 2 : 3;
}

} // namespace
Outcome<FieldContactRead>
read_field_attack_contacts_at_prefix(const FieldBattleSession &session,
                                     const GeneratedBattleField &field,
                                     const FieldAttackPrefixRead *prefix) {
  auto fail = [](const char *code) -> Outcome<FieldContactRead> {
    return {false, {}, common_failure(code).error};
  };
  auto valid = validate_field_battle_parameters(field.parameters);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto &p = field.parameters;
  if ((!prefix && session.state != FieldBattleState::Running) ||
      session.field_hash != field.field_hash || p.length_m % p.cell_m ||
      p.width_m % p.cell_m || field.columns != p.length_m / p.cell_m ||
      field.rows != p.width_m / p.cell_m ||
      __int128_t(field.columns) * field.rows != field.cells.size() ||
      __int128_t(p.length_m) * 10 > std::numeric_limits<Integer>::max() ||
      __int128_t(p.width_m) * 10 > std::numeric_limits<Integer>::max())
    return fail("FieldContactSourceMismatch");
  if (prefix) {
    if (prefix->motion.formations.size() != session.formations.size() ||
        prefix->facing_mdeg.size() != session.formations.size())
      return fail("FieldContactOriginalPrefixRequired");
    for (const auto &[id, f] : session.formations) {
      auto m = prefix->motion.formations.find(id);
      auto face = prefix->facing_mdeg.find(id);
      if (m == prefix->motion.formations.end() ||
          face == prefix->facing_mdeg.end() || m->second.fit < 0 ||
          m->second.fit > 500 || face->second < 0 || face->second >= 360000)
        return fail("FieldContactOriginalPrefixRequired");
      if (f.ability)
        for (const auto &v : f.ability->active)
          if (v.kind == FieldTacticKind::Volley &&
              std::tie(v.started.tick, v.started.t) <=
                  std::tie(prefix->motion.at.tick, prefix->motion.at.t)) {
            auto count = prefix->remaining_volley_pulses.find(v.activation_id);
            if (count == prefix->remaining_volley_pulses.end() ||
                count->second < 0 || count->second > 3)
              return fail("FieldContactOriginalVolleyPrefixRequired");
          }
    }
  }
  auto actual = [&](const FieldFormation &f) -> FieldMotionPrefixFormation {
    if (prefix)
      return prefix->motion.formations.at(f.formation_id);
    return {f.position_dm, Integer(f.fit.size()),
            f.state == FieldFormationState::Routing, !present(f)};
  };
  auto is_present = [&](const FieldFormation &f) {
    auto v = actual(f);
    return v.fit > 0 && !v.withdrawn;
  };
  auto face = [&](const FieldFormation &f) {
    return prefix ? prefix->facing_mdeg.at(f.formation_id) : f.facing_mdeg;
  };
  auto radius = [&](const FieldFormation &f) {
    return field_formation_radius_dm(f, actual(f).fit);
  };
  FieldContactRead out;
  std::set<StableId> melee;
  for (const auto &[id, f] : session.formations) {
    if (!is_present(f))
      continue;
    if (f.side < 0 || f.side > 1 || face(f) < 0 || face(f) >= 360000)
      return fail("FieldContactFormationInvalid");
    auto cell = terrain(field, actual(f).position_dm);
    if (!cell.ok)
      return {false, {}, cell.error};
  }
  for (const auto &[id, f] : session.formations) {
    if (!is_present(f))
      continue;
    for (const auto &[other_id, other] : session.formations) {
      if (other_id <= id || !is_present(other) || other.side == f.side)
        continue;
      const __int128_t reach = __int128_t(radius(f)) + radius(other) +
                               __int128_t(p.melee_contact_m) * 10;
      if (field_distance_squared(actual(f).position_dm,
                                 actual(other).position_dm) <= reach * reach) {
        melee.insert(id);
        melee.insert(other_id);
      }
    }
  }
  out.melee_formations.assign(melee.begin(), melee.end());
  for (const auto &[id, f] : session.formations) {
    if (!is_present(f) || actual(f).routing)
      continue;
    const auto own_cell = terrain(field, actual(f).position_dm);
    for (const auto &[other_id, other] : session.formations) {
      if (!is_present(other) || other.side == f.side)
        continue;
      const auto squared = field_distance_squared(actual(f).position_dm,
                                                  actual(other).position_dm);
      const __int128_t reach = __int128_t(radius(f)) + radius(other) +
                               __int128_t(p.melee_contact_m) * 10;
      const bool close = squared <= reach * reach;
      bool ranged = false;
      if (!close) {
        if (f.category != FieldTroopCategory::Archer || melee.contains(id))
          continue;
        const __int128_t minimum = __int128_t(p.archer_min_range_m) * 10;
        Integer bonus_m = 0;
        if (f.ability) {
          if (std::find(f.ability->traits.begin(), f.ability->traits.end(),
                        FieldTrait::ArcheryMaster) != f.ability->traits.end())
            bonus_m += 20;
          // 齐射按实际剩余脉冲生效，不借冷却结束时刻冒有效期。
          if (std::any_of(f.ability->active.begin(), f.ability->active.end(),
                          [&](const auto &active) {
                            if (active.kind != FieldTacticKind::Volley)
                              return false;
                            if (!prefix)
                              return active.remaining_pulses > 0;
                            return std::tie(active.started.tick,
                                            active.started.t) <=
                                       std::tie(prefix->motion.at.tick,
                                                prefix->motion.at.t) &&
                                   prefix->remaining_volley_pulses.at(
                                       active.activation_id) > 0;
                          }))
            bonus_m += 20;
        }
        const __int128_t maximum =
            (__int128_t(p.archer_max_range_m) + bonus_m) * 10;
        if (squared < minimum * minimum || squared > maximum * maximum)
          continue;
        auto sight = read_field_line_of_sight(field, actual(f).position_dm,
                                              actual(other).position_dm);
        if (!sight.ok)
          return {false, {}, sight.error};
        if (!sight.value)
          continue;
        ranged = true;
      }
      auto target_cell = terrain(field, actual(other).position_dm);
      if (!target_cell.ok)
        return {false, {}, target_cell.error};
      const auto &t = *target_cell.value;
      out.attacks.push_back(
          {id, other_id, ranged,
           facing(actual(f).position_dm, actual(other).position_dm, face(other),
                  actual(other).routing),
           t.cover, __int128_t(own_cell.value->height_dm) - t.height_dm >= 50,
           t.terrain == FieldTerrainKind::Forest,
           t.terrain == FieldTerrainKind::Rough ||
               t.terrain == FieldTerrainKind::Steep,
           own_cell.value->terrain == FieldTerrainKind::Forest});
    }
  }
  return {true, std::move(out), {}};
}
Outcome<FieldContactRead>
read_field_attack_contacts(const FieldBattleSession &session,
                           const GeneratedBattleField &field) {
  return read_field_attack_contacts_at_prefix(session, field, nullptr);
}
Outcome<FieldContactRead> apply_field_contacts_in_candidate(
    FieldBattleSession &session, const GeneratedBattleField &field,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner) {
  if (step.steps != 1 || session.now.tick != step.from.tick ||
      session.now.t != step.from.t)
    return {
        false, {}, common_failure("FieldContactStepBoundaryRequired").error};
  auto end = add_field_battle_seconds(step.from, field.parameters.step_s,
                                      field.parameters);
  if (!end.ok)
    return {false, {}, end.error};
  if (end.value.tick != step.to.tick || end.value.t != step.to.t ||
      session.next_event_sequence <= 0 ||
      session.next_event_sequence == std::numeric_limits<Integer>::max())
    return {
        false, {}, common_failure("FieldContactStepBoundaryRequired").error};
  auto contacts = read_field_attack_contacts(session, field);
  if (!contacts.ok)
    return contacts;
  std::set<StableId> engaged(contacts.value.melee_formations.begin(),
                             contacts.value.melee_formations.end());
  for (const auto &attack : contacts.value.attacks)
    engaged.insert(attack.attacker_id);


  (owner).grow(0);
  FieldBattleEvent phase;
  phase.sequence = session.next_event_sequence++;
  phase.kind = field_event_kind::ContactPhaseApplied;
  phase.at = step.to;
  field_event_contact_phase_from(phase) = step.from;
  session.events.push_back(std::move(phase));
  for (auto &[id, formation] : session.formations) {
    if (!present(formation))
      continue;
    if (engaged.contains(id)) {
      if (!formation.has_engaged) {
        if (session.next_event_sequence <= 0 ||
            session.next_event_sequence == std::numeric_limits<Integer>::max())
          return {false,
                  {},
                  common_failure("FieldEventSequenceExhausted", id).error};
        FieldBattleEvent event;
        event.sequence = session.next_event_sequence++;
        event.at = step.to;
        event.kind = field_event_kind::Contact;
        event.side = formation.side;
        event.formation_id = id;
        session.events.push_back(std::move(event));
        formation.has_engaged = true;
        auto &triggers = session.ai[formation.side].pending_triggers;
        if (std::find(triggers.begin(), triggers.end(),
                      FieldAiTrigger::FirstEngagement) == triggers.end()) {
          triggers.push_back(FieldAiTrigger::FirstEngagement);
          std::sort(triggers.begin(), triggers.end());
        }
      }
      // 退却与溃退仍能离开接触，不被 Engaged 写回冻结到原地。
      if (formation.state != FieldFormationState::Routing &&
          !std::holds_alternative<FieldWithdraw>(formation.mission))
        formation.state = FieldFormationState::Engaged;
      formation.hold_since.reset();
    } else if (formation.state == FieldFormationState::Engaged) {
      if (!formation.pending_player_entry_order &&
          std::holds_alternative<FieldHold>(formation.mission)) {
        formation.state = FieldFormationState::Holding;
        formation.hold_since = step.to;
      } else
        formation.state = FieldFormationState::Moving;
    }
  }
  return contacts;
}
} // namespace hanmo::resident
