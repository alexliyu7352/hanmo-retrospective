#include "field_event_registry.hpp"
#include "field_event_legacy_kinds.hpp"
#include "field_event_mission_transition.hpp"
#include "field_event_navigation_observation.hpp"
#include "field_event_motion_application.hpp"
#include "field_command_source.hpp"
#include "field_mission_transition.hpp"
#include "field_navigation_source.hpp"
#include "field_order_resolution.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_suppression_source.hpp"
#include "field_winner_continuation.hpp"
#include "resident/kernel/original_world_read_boundary.hpp"
// 验证只读当前真实所属与持久来源；损坏银行不能靠补空、排序或改位置通过。
#include "field_binding_source.hpp"
#include "field_boundary_install.hpp"
#include "field_commander_change_source.hpp"
#include "field_contact_origin.hpp"
#include "field_council_validation.hpp"
#include "field_exact_contact_source.hpp"
#include "field_exact_session_build.hpp"
#include "field_exit_policy.hpp"
#include "field_exit_validation.hpp"
#include "field_local_event_sources.hpp"
#include "field_roster_origin.hpp"
#include "field_segment_settlement.hpp"
#include "field_stratagem_resolution.hpp"
#include "field_validation.hpp"
#include "field_state_codec.hpp"
#include "resident/domains/military/military_field_command_source.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/domains/travel/movement_contact_source_equal.hpp"
#include "resident/kernel/native_military_state.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
// 保持一个明确的领域错误出口，不把未能证明的来源降为独行。
Status invalid(const StableId &path) {
  return common_failure("InvalidFieldBattleState", path);
}
// 只有实际 Common 头和本域 typed record 相配，才构成可追溯事实。
bool fact(const CommonStore &common, const StableId &id, const char *kind) {
  const auto *header = common.state().facts.find(id);
  return header && header->id == id && header->domain == "military" &&
         header->kind == kind;
}
// 时间合法不意味着已到期；future orders 保原 due，不用当前 tick 改写。
bool time(const FieldBattleTime &at, const FieldBattleParameters &p) {
  return at.tick >= 0 && at.t >= 0 && at.t < p.window_s;
}
bool earlier(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick < b.tick || (a.tick == b.tick && a.t < b.t);
}
bool point(PointI p, const GeneratedBattleField &field) {
  return p.x >= -field.parameters.length_m * 5 &&
         p.y >= -field.parameters.width_m * 5 &&
         p.x < field.parameters.length_m * 5 &&
         p.y < field.parameters.width_m * 5;
}
// 任务目标沿同会话实际编队归属核对；不因尚未接战就放过跨侧 Support。
Status mission(const FieldMission &mission, const FieldFormation &formation,
               const FieldBattleSession &session,
               const GeneratedBattleField &field) {
  return std::visit(
      [&](const auto &m) -> Status {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::is_same_v<T, FieldHold> ||
                      std::is_same_v<T, FieldMoveTo>) {
          return point(m.point_dm, field) && m.facing_mdeg >= 0 &&
                         m.facing_mdeg < 360000
                     ? common_success()
                     : invalid(formation.formation_id + "/mission");
        } else if constexpr (std::is_same_v<T, FieldWithdraw>) {
          return m.exit_side == formation.side
                     ? common_success()
                     : invalid(formation.formation_id + "/exit");
        } else {
          const auto id = [&]() -> const StableId & {
            if constexpr (std::is_same_v<T, FieldEngage>)
              return m.target_formation_id;
            else
              return m.ally_formation_id;
          }();
          const auto target = session.formations.find(id);
          if (target == session.formations.end())
            return invalid(id);
          if constexpr (std::is_same_v<T, FieldEngage>)
            return target->second.side != formation.side ? common_success()
                                                         : invalid(id);
          else
            return target->second.side == formation.side &&
                           id != formation.formation_id
                       ? common_success()
                       : invalid(id);
        }
      },
      mission);
}
} // namespace
// 所有公开布阵与军议命令复用原有限任务门，不复制另一套目标约束。
Status validate_field_battle_mission(const FieldMission &value,
                                     const FieldFormation &formation,
                                     const FieldBattleSession &session,
                                     const GeneratedBattleField &field) {
  return mission(value, formation, session, field);
}
Outcome<Integer>
selected_field_battle_version(const CatalogIdentity &identity) {
  Integer version = 0;
  for (const auto &[name, value] : identity.module_versions)
    if (name == "field_battle") {
      if (version || value != "1")
        return {false, 0,
                common_failure("UnsupportedFieldBattleVersion", name).error};
      version = 1;
    }
  return {true, version, {}};
}
Status validate_field_battle_selection(const CatalogIdentity &identity,
                                       const FieldBattleCatalog *catalog) {
  auto selected = selected_field_battle_version(identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (bool(selected.value) != bool(catalog))
    return invalid("field_battle/catalog_selection");
  if (catalog &&
      (!same_original_catalog_identity(identity, catalog->identity) ||
       catalog->parameters.version != "field_battle/1" ||
       catalog->maps.empty()))
    return invalid("field_battle/catalog_identity");
  return common_success();
}
Status validate_field_battle_layout(const CatalogIdentity &identity,
                                    const FieldBattleCatalog *catalog,
                                    const FieldBattleWorldState *world) {
  auto selected = validate_field_battle_selection(identity, catalog);
  if (!selected.ok)
    return selected;
  if (bool(catalog) != bool(world) || (world && world->schema != 1))
    return invalid("field_battle/state_selection");
  return common_success();
}
Status validate_field_battle_world(
    const CommonStore &common, const MilitaryState &military,
    const TravelCatalog &travel, const FieldBattleCatalog &catalog,
    const FieldBattleWorldState &world,
    const FieldBattleAbilityReadContext *abilities) {
  auto selected = selected_field_battle_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (selected.value != 1 || catalog.parameters.version != "field_battle/1" ||
      world.schema != 1 || catalog.maps.empty())
    return invalid("field_battle/profile");
  // Actual independent S1 boundary, diplomacy and per-formation queue source
  // gates replace the old blanket placeholder. Missing same-World owners do
  // not become an empty provider or a legacy geometry fallback.
  auto exit_handoffs = validate_field_original_exit_handoffs(
      common, military, world, travel, catalog, abilities);
  if (!exit_handoffs.ok)
    return exit_handoffs;
  const auto *exact = abilities ? abilities->exact_contacts : nullptr;
  if (exact) {
    if (&exact->common != &common || &exact->military != &military ||
        &exact->field != &world || &exact->travel != &travel)
      return common_failure("CrossCandidateFieldWinnerContext");
    auto winners = validate_field_winner_continuations(*exact, catalog);
    if (!winners.ok)
      return winners;
  }
  const bool boundary_sources =
      !world.boundary_contacts.empty() ||
      (exact && !exact->movement.boundary_contacts.empty());
  if (boundary_sources) {
    if (!abilities || !exact || !abilities->diplomacy ||
        !abilities->original_rosters || &exact->common != &common ||
        &exact->military != &military || &exact->field != &world ||
        &exact->travel != &travel ||
        &exact->military_catalog != &abilities->military ||
        !exact->clock.bound_to(common, military, world))
      return common_failure("FieldBoundaryOriginalWorldSourcesRequired");
    auto boundaries = validate_field_exact_boundary_contacts(
        *exact, *abilities->diplomacy, catalog);
    if (!boundaries.ok)
      return boundaries;
    auto queues = validate_field_reinforcement_queue_sources(
        *exact, catalog, *abilities->original_rosters);
    if (!queues.ok)
      return queues;
  }
  // 当前首项未装精确成军 reader；不能让冷读绕过热 installer 的同一明确缺口。
  auto military_selection =
      selected_native_military_state_versions(catalog.identity);
  if (!military_selection.ok)
    return {false, {}, military_selection.error};
  if (!world.sessions.empty() &&
      (military_selection.value.military_action_time ||
       military_selection.value.dynamic_military) &&
      (!abilities || !abilities->original_formations ||
       !abilities->actual_clock))
    return common_failure("FieldBattleExactFormationReaderRequired",
                          "field_battle/formation",
                          ErrorKind::unsupported_capability);
  // 冷读核真实成军来源，不重新用今日存活/可用/fit 生成旧会话。
  if (!world.sessions.empty() && abilities && abilities->original_formations) {
    if (!abilities->actual_clock ||
        !abilities->original_formations->bound_to(common.state(), military))
      return common_failure("CrossCandidateFieldFormationRead");
    auto actual = abilities->actual_clock->read_current_moment();
    if (!actual.ok)
      return {false, {}, actual.error};
    if (actual.value.half_day_tick != common.tick()) {
      if (!abilities->exact_contacts ||
          !abilities->exact_contacts->clock.bound_to(common, military, world))
        return common_failure("FieldFormationActualMomentMismatch");
      auto original_now =
          abilities->exact_contacts->clock.read_current_moment();
      if (!original_now.ok)
        return {false, {}, original_now.error};
      auto same = compare_world_moments(original_now.value, actual.value);
      if (!same.ok)
        return {false, {}, same.error};
      if (same.value != 0)
        return common_failure("FieldFormationActualMomentMismatch");
    }
    std::set<StableId> army_ids;
    for (const auto &[session_id, session] : world.sessions)
      for (const auto &[formation_id, formation] : session.formations)
        army_ids.insert(formation.army_id);
    for (const auto &army_id : army_ids) {
      const ArmyRecord *army = nullptr;
      for (const auto &row : military.armies)
        if (row.id == army_id) {
          if (army)
            return invalid(army_id + "/formation_source");
          army = &row;
        }
      if (!army)
        return invalid(army_id + "/formation_source");
      auto formed = read_original_military_formation(
          common.state(), military, *army, actual.value,
          abilities->original_formations);
      if (!formed.ok)
        return {false, {}, formed.error};
    }
  }
  if (!military.battles.empty())
    return invalid("field_battle/legacy_battles");
  auto held = read_field_battle_held_armies(
      common, military, world,
      abilities ? abilities->active_membership : nullptr);
  if (!held.ok)
    return {false, {}, held.error};
  const auto &parameters = catalog.parameters;
  auto parameters_valid = validate_field_battle_parameters(parameters);
  if (!parameters_valid.ok)
    return parameters_valid;
  auto original_segments =
      validate_field_segment_settlement_sources(common, military, world);
  if (!original_segments.ok)
    return original_segments;
  bool has_segments = false;
  for (const auto &[id, session] : world.sessions)
    has_segments = has_segments || !session.settlements.empty();
  if (has_segments) {
    if (!abilities || !abilities->exact_contacts ||
        !abilities->exact_contacts->clock.bound_to(common, military, world))
      return common_failure("FieldSegmentOriginalApplicationOwnerRequired");
    auto applications = validate_field_segment_application_sources(
        common, military, world, abilities->exact_contacts->clock);
    if (!applications.ok)
      return applications;
  }
  std::set<StableId> contact_owners;
  for (const auto &[id, record] : world.contacts) {
    if (id != record.contact_id || !fact(common, id, "field_battle_contact") ||
        !world.sessions.contains(record.battle_id) ||
        !contact_owners.insert(record.battle_id).second)
      return invalid(id);
    auto origin_head = validate_field_contact_origin_head(common, record);
    if (!origin_head.ok)
      return origin_head;
    for (const auto &binding : record.original_bindings) {
      auto original_binding = read_field_original_battle_binding(
          common, world, record.battle_id, binding.army_id);
      if (!original_binding.ok)
        return {false, {}, original_binding.error};
    }
    if (const auto legacy = std::get_if<FieldRoadContact>(&record.source)) {
      if (!legacy->contact.road || legacy->source_tick < 0 ||
          legacy->source_tick >= common.tick())
        return invalid(id);
      auto source = original_road_contact_intersection(
          travel, legacy->contact.road->first.trace,
          legacy->contact.road->second.trace);
      if (!source || !movement_contact_detail::same(*source, legacy->contact))
        return invalid(id + "/source");
    } else {
      if (!abilities || !abilities->exact_contacts)
        return common_failure("FieldExactContactOriginalContextRequired", id);
      const auto &x = *abilities->exact_contacts;
      if (&x.common != &common || &x.military != &military ||
          &x.field != &world || &x.travel != &travel)
        return common_failure("CrossCandidateFieldExactContact", id);
      const auto &ref =
          std::get<FieldExactGroupContactOrigin>(record.source).actual_contact;
      auto actual = read_field_exact_road_contact_source(x, ref, parameters);
      if (!actual.ok)
        return {false, {}, actual.error};
      if (!abilities->diplomacy || &abilities->diplomacy->common != &common ||
          &abilities->diplomacy->field != &world ||
          &abilities->diplomacy->military != &military)
        return common_failure("FieldOriginalDiplomacyRequired", id);
      auto relation = read_political_relation_view_before_field_source(
          *abilities->diplomacy, {"military", "field_battle_contact", id},
          *actual.value.endpoints[0].faction_id,
          *actual.value.endpoints[1].faction_id);
      if (!relation.ok)
        return {false, {}, relation.error};
      if (relation.value.relation != PoliticalRelation::hostile)
        return invalid(id + "/hostile_encounter");
      for (const auto &endpoint : actual.value.endpoints) {
        const auto i = endpoint.geometry_endpoint ==
                               &actual.value.contact->original_hit.road->first
                           ? 0
                           : 1;
        const auto &binding = record.original_bindings[i];
        if (binding.army_id != endpoint.army->id || !binding.action_id.empty())
          return invalid(id + "/original_binding");
        auto commander = query_military_army_command_source_before_at(
            common, military, endpoint.army->id, ref.id,
            actual.value.contact->occurred_at, &x.formations);
        if (!commander.ok)
          return {false, {}, commander.error};
        if (binding.commander_actor_id != commander.value.commander_actor_id)
          return invalid(id + "/original_commander");
        std::set<StableId> actual_names;
        for (const auto &member : binding.members)
          if (!actual_names.insert(member.actor_id).second)
            return invalid(id);
        const auto &names = endpoint.opening->named_participants;
        if (std::set<StableId>(names.begin(), names.end()) != actual_names ||
            actual_names.size() != names.size())
          return invalid(id + "/original_members");
      }
    }
  }
  for (const auto &[id, session] : world.sessions) {
    if (session.started_at) {
      if ((military_selection.value.military_action_time ||
           military_selection.value.dynamic_military) &&
          (!abilities || !abilities->original_rosters))
        return common_failure("FieldOriginalRosterReaderRequired", id);
      if (!abilities)
        return common_failure("FieldAbilitySourcesRequired", id);
      auto valid = validate_field_battle_initial_abilities(
          common, *abilities, world, session, catalog.parameters);
      if (!valid.ok)
        return valid;
    } else {
      if (session.started_source || session.started_actual_at ||
          session.started_observed_tick != 0 || session.started_by_actor_id)
        return invalid(id + "/unexpected_start_source");
      for (const auto &[formation_id, formation] : session.formations)
        if (formation.ability)
          return common_failure("UnexpectedFieldAbilityState", formation_id);
    }

    if (id != session.battle_id || !fact(common, id, "field_battle_session") ||
        !contact_owners.contains(id) || session.created_tick < 0 ||
        session.created_tick > common.tick() || session.segment < 0 ||
        session.segment >= parameters.max_segments ||
        static_cast<Integer>(session.state) < 0 ||
        static_cast<Integer>(session.state) > 3 ||
        !time(session.now, parameters) || session.now.t % parameters.step_s)
      return invalid(id + "/time");
    const FieldBattleContactRecord *origin = nullptr;
    for (const auto &[key, record] : world.contacts)
      if (record.battle_id == id)
        origin = &record;
    if (!origin)
      return invalid(id + "/contact");
    auto first = [&]() -> Outcome<FieldBattleTime> {
      if (const auto legacy = std::get_if<FieldRoadContact>(&origin->source)) {
        if (legacy->source_tick + 1 != session.created_tick)
          return {false, {}, invalid(id + "/contact").error};
        return field_contact_battle_time(legacy->source_tick, legacy->contact,
                                         parameters);
      }
      if (!abilities || !abilities->exact_contacts)
        return {false,
                {},
                common_failure("FieldExactContactOriginalContextRequired", id)
                    .error};
      auto actual = read_field_exact_road_contact_source(
          *abilities->exact_contacts,
          std::get<FieldExactGroupContactOrigin>(origin->source).actual_contact,
          parameters);
      if (!actual.ok)
        return {false, {}, actual.error};
      if (actual.value.contact->occurred_at.half_day_tick !=
          session.created_tick)
        return {false, {}, invalid(id + "/contact").error};
      return {true, actual.value.field_at, {}};
    }();
    if (!first.ok)
      return {false, {}, first.error};
    const auto expected_tick = static_cast<__int128_t>(first.value.tick) +
                               session.segment +
                               (session.window_complete ? 1 : 0);
    if (expected_tick != session.now.tick || session.now.tick > common.tick() ||
        (session.segment == 0 && !session.window_complete &&
         session.now.t < first.value.t) ||
        (session.window_complete && session.now.t != 0))
      return invalid(id + "/physical_window");
    // A closed window/terminal must own its genuine completed segment. The
    // original reader above proves both directions of parent/casualty/terminal
    // and actual S0 application, rather than trusting this local boolean.
    const bool settled_current =
        session.window_complete || session.state == FieldBattleState::Ended;
    const auto expected_segments =
        std::size_t(session.segment) + (settled_current ? 1 : 0);
    if (session.settlements.size() != expected_segments)
      return invalid(id + "/segment_coverage");
    if (settled_current) {
      const auto &last = session.settlements.back();
      if (last.segment != session.segment ||
          last.settled_at.tick != session.now.tick ||
          last.settled_at.t != session.now.t ||
          bool(last.battle_fact_id) !=
              (session.state == FieldBattleState::Ended))
        return invalid(id + "/settled_boundary");
    }
    auto rebuilt = regenerate_field_contact_battle(
        travel, catalog, origin->source,
        abilities ? abilities->exact_contacts : nullptr);
    if (!rebuilt.ok)
      return {false, {}, rebuilt.error};
    const auto &field = rebuilt.value;
    if (session.field_hash != field.field_hash ||
        session.anchor.route_id != field.anchor.route_id ||
        session.anchor.point_m.x != field.anchor.point_m.x ||
        session.anchor.point_m.y != field.anchor.point_m.y ||
        session.anchor.tangent.x != field.anchor.tangent.x ||
        session.anchor.tangent.y != field.anchor.tangent.y)
      return invalid(id + "/field_hash");
    if (abilities) {
      auto stratagem_sources = validate_field_stratagem_sources(
          common, world, session, field, *abilities);
      if (!stratagem_sources.ok)
        return stratagem_sources;
    } else if (!session.stratagems.empty() ||
               session.next_stratagem_sequence != 1)
      return common_failure("FieldBattleAbilityReaderRequired", id);
    if (session.formations.empty() ||
        session.formations.size() / 2 >
            static_cast<std::size_t>(parameters.max_formations_per_side))
      return invalid(id + "/formations");
    std::array<std::size_t, 2> counts{};
    std::set<StableId> roster;
    for (const auto &[key, f] : session.formations) {
      if (key != f.formation_id || f.side < 0 || f.side > 1 ||
          static_cast<Integer>(f.category) < 0 ||
          static_cast<Integer>(f.category) > 2 ||
          static_cast<Integer>(f.state) < 0 ||
          static_cast<Integer>(f.state) > 6 || f.fit_start <= 0 ||
          f.morale < 0 || f.morale > 100 || f.morale_start < 0 ||
          f.morale_start > 100 || f.facing_mdeg < 0 ||
          f.facing_mdeg >= 360000 ||
          f.ranks != (f.category == FieldTroopCategory::Infantry ? 6
                      : f.category == FieldTroopCategory::Archer ? 4
                                                                 : 3))
        return invalid(key);
      // Before Start there is no motion event history yet. Pending flank
      // continuation must already match the actually adopted immutable plan,
      // or be absent after an actual SetDeployment replacement.
      if (!session.started_at) {
        std::optional<Integer> expected_flank;
        const auto &c = session.councils[f.side];
        if (c.adopted_proposal_id && !f.deployment_command_result) {
          const FieldCouncilProposal *adopted = nullptr;
          for (const auto &proposal : c.proposals)
            if (proposal.proposal_id == *c.adopted_proposal_id) {
              if (adopted)
                return invalid(key + "/flank_plan");
              adopted = &proposal;
            }
          if (!adopted)
            return invalid(key + "/flank_plan");
          for (std::size_t a = 0; a < adopted->plan.assignments.size(); ++a) {
            const auto &assignment = adopted->plan.assignments[a];
            if (assignment.formation_id == key && assignment.flank_followup) {
              if (expected_flank)
                return invalid(key + "/flank_assignment");
              expected_flank = Integer(a);
            }
          }
        }
        if (f.flank_assignment_index != expected_flank)
          return invalid(key + "/flank_pending_source");
      }
      if (++counts[f.side] >
          static_cast<std::size_t>(parameters.max_formations_per_side))
        return invalid(id + "/formations");
      const MilitaryUnitRecord *unit = nullptr;
      std::optional<MilitaryUnitRecord> original_unit;
      if (session.started_at && abilities && abilities->original_rosters) {
        auto original = read_field_original_participation_roster(
            common, military, abilities->military, world, session, f,
            *abilities->original_rosters);
        if (!original.ok)
          return {false, {}, original.error};
        original_unit.emplace(std::move(original.value));
        unit = &*original_unit;
      } else {
        for (const auto &row : military.military_units)
          if (row.id == key) {
            if (unit)
              return invalid(key);
            unit = &row;
          }
      }
      if (!unit || unit->army_id != f.army_id ||
          unit->troop_type_id != f.troop_type_id ||
          !common.actor(f.commander_actor_id))
        return invalid(key + "/unit");
      std::set<StableId> members;
      for (const auto *group :
           {&f.fit, &f.wounded, &f.dead, &f.captured, &f.deserted})
        for (const auto &actor : *group)
          if (actor.empty() || !members.insert(actor).second ||
              !roster.insert(actor).second)
            return invalid(actor + "/roster");
      if (members !=
          std::set<StableId>(unit->member_ids.begin(), unit->member_ids.end()))
        return invalid(key + "/roster");
      if (session.started_at && abilities && abilities->original_rosters) {
        auto outputs = validate_field_local_roster_outputs(
            common, military, abilities->military, world, session, f,
            *abilities->original_rosters);
        if (!outputs.ok)
          return outputs;
      } else if (f.captors != unit->captors)
        return common_failure("NOT_IMPLEMENTED",
                              "field_battle/captor_settlement",
                              ErrorKind::unsupported_capability);
      if (f.deputy_actor_id && (!common.actor(*f.deputy_actor_id) ||
                                *f.deputy_actor_id == f.commander_actor_id ||
                                !members.contains(*f.deputy_actor_id)))
        return invalid(key + "/deputy");
      bool actual_reinforcement = false;
      for (const auto &[contact_id, boundary] : world.boundary_contacts)
        if (boundary.battle_id == id && boundary.reinforcement_queue)
          for (const auto &entry : boundary.reinforcement_queue->admitted)
            if (entry.formation_id == key) {
              auto original = read_field_reinforcement_admission_identity(
                  common, world, session, f);
              if (!original.ok)
                return {false, {}, original.error};
              if (actual_reinforcement || original.value != &entry)
                return invalid(key + "/reinforcement_identity");
              actual_reinforcement = true;
            }
      // An actual later entry never participated in the old council wave
      // assignment. Its own immutable admission supplies mission and placement.
      if (actual_reinforcement) {
        if (f.wave || f.wave_mission || f.flank_assignment_index ||
            f.deployment_command_result)
          return invalid(key + "/reinforcement_council_wave");
      } else if (session.councils[f.side].closed != bool(f.wave) ||
                 (f.wave && (static_cast<Integer>(*f.wave) < 0 ||
                             static_cast<Integer>(*f.wave) > 2)))
        return invalid(key + "/wave");
      auto original_mission = read_field_current_mission_origin(session, f);
      if (!original_mission.ok)
        return {false, {}, original_mission.error};
      const bool second = f.wave && *f.wave == FieldCouncilWave::Second;
      const bool replaced_mission = f.mission_command_result ||
                                    f.adopted_order_id ||
                                    f.mission_ai_source_id;
      const bool wave_consumed = std::any_of(
          session.events.begin(), session.events.end(), [&](const auto &e) {
            return e.formation_id == f.formation_id && field_event_mission_transition(e) &&
                   (field_event_mission_transition(e)->reason ==
                        FieldMissionTransitionReason::WaveReleased ||
                    field_event_mission_transition(e)->reason ==
                        FieldMissionTransitionReason::Rallied);
          });
      if ((f.wave_mission && !second) ||
          (second && !f.wave_mission && !replaced_mission && !wave_consumed) ||
          (f.wave && *f.wave == FieldCouncilWave::Reserve &&
           !std::holds_alternative<FieldHold>(f.mission) &&
           !replaced_mission) ||
          (f.wave_mission && !std::holds_alternative<FieldHold>(f.mission)))
        return invalid(key + "/wave_mission");
      if (f.wave_mission) {
        auto valid = mission(*f.wave_mission, f, session, field);
        if (!valid.ok)
          return valid;
      }
      // DTO 已收齐不代表推进 writer 可用；未产生过的新热状态不可伪装冷读成功。
      if (f.pending_direct) {
        const auto &pending = *f.pending_direct;
        const auto *controller = std::get_if<FieldDirectControl>(&f.control);
        const bool delegated_pending =
            session.sides[f.side].delegated_by_actor_id ==
                pending.issuer_actor_id &&
            session.sides[f.side].delegation_command_result.has_value();
        if ((!controller && !delegated_pending) ||
            (controller &&
             controller->controller_actor_id != pending.issuer_actor_id) ||
            pending.request_sequence < 0 ||
            pending.accepted_revision != f.mission_revision ||
            pending.accepted_revision <= 0 ||
            pending.ai_issued == bool(pending.command_result) ||
            pending.ai_issued != bool(pending.ai_source_id) ||
            (pending.ai_issued && (!controller ||
                                   session.sides[f.side].commander_actor_id !=
                                       pending.issuer_actor_id ||
                                   !std::holds_alternative<FieldAiController>(
                                       session.sides[f.side].controller))) ||
            pending.apply_at.tick != session.now.tick ||
            pending.apply_at.t != session.now.t)
          return invalid(key + "/pending_direct");
        auto valid = mission(pending.mission, f, session, field);
        if (!valid.ok)
          return valid;
      }
      if (f.adopted_order_id) {
        const auto order = session.orders.find(*f.adopted_order_id);
        if (f.mission_command_result || order == session.orders.end() ||
            order->second.target_formation_id != key ||
            order->second.status != FieldOrderStatus::Adopted ||
            !order->second.adopted_mission_revision ||
            (!f.pending_direct &&
             *order->second.adopted_mission_revision != f.mission_revision) ||
            (f.pending_direct && *order->second.adopted_mission_revision >=
                                     f.pending_direct->accepted_revision) ||
            !std::holds_alternative<FieldMission>(order->second.payload) ||
            !same_field_battle_mission(
                *original_mission.value.mission,
                std::get<FieldMission>(order->second.payload)))
          return invalid(key + "/adopted_order");
      }
      if (f.hold_since && (!time(*f.hold_since, parameters) ||
                           earlier(session.now, *f.hold_since) ||
                           earlier(*f.hold_since, f.joined_at) ||
                           !std::holds_alternative<FieldHold>(f.mission) ||
                           f.state != FieldFormationState::Holding))
        return invalid(key + "/hold_since");
      if (f.stopped_since && (!time(*f.stopped_since, parameters) ||
                              earlier(session.now, *f.stopped_since) ||
                              earlier(*f.stopped_since, f.joined_at)))
        return invalid(key + "/stopped_since");
      for (const auto &status : f.statuses)
        if (status.kind != FieldStatusKind::Charging &&
            status.kind != FieldStatusKind::Guarding &&
            status.kind != FieldStatusKind::Suppressed &&
            (status.kind != FieldStatusKind::Hidden || !abilities) &&
            (status.kind != FieldStatusKind::Confused || !abilities))
          return common_failure("NOT_IMPLEMENTED",
                                "field_battle/status_driver_source",
                                ErrorKind::unsupported_capability);
      // Navigation and AI-pursuit states are verified below against actual
      // mission adoption, per-step contact phases and original motion sources.
      // An original flank follow-up still needs its own finite source producer.
      if (f.followup_mission || (f.casualty_ordinal != 0 &&
                                 (!abilities || !abilities->original_rosters)))
        return common_failure("NOT_IMPLEMENTED",
                              "field_battle/formation_driver_state",
                              ErrorKind::unsupported_capability);
      if (f.state != FieldFormationState::Deploying) {
        if (!point(f.position_dm, field))
          return invalid(key + "/position");
        if (f.pending_player_entry_order) {
          auto admission = read_field_reinforcement_admission_identity(
              common, world, session, f);
          if (!admission.ok)
            return {false, {}, admission.error};
          if (!admission.value->pending_player_entry_order || f.hold_since ||
              f.wave_mission || f.followup_mission || f.adopted_order_id ||
              f.mission_command_result || f.mission_ai_source_id ||
              f.mission_transition_sequence ||
              !same_field_battle_mission(f.mission,
                                         admission.value->initial_mission))
            return invalid(key + "/pending_player_entry_order");
        } else {
          auto valid = mission(f.mission, f, session, field);
          if (!valid.ok)
            return valid;
        }
      } else if (session.state != FieldBattleState::Deploying &&
                 session.state != FieldBattleState::Council)
        return invalid(key + "/deployment");
      if (const auto *direct = std::get_if<FieldDirectControl>(&f.control))
        if (!common.actor(direct->controller_actor_id))
          return invalid(key + "/controller");
    }
    if (!counts[0] || !counts[1])
      return invalid(id + "/sides");
    for (std::size_t side = 0; side < 2; ++side) {
      const auto &s = session.sides[side];
      const auto &ai = session.ai[side];
      if (s.side != Integer(side) || ai.side != Integer(side) ||
          !time(ai.last_eval, parameters) ||
          earlier(session.now, ai.last_eval) ||
          !common.actor(s.commander_actor_id))
        return invalid(id + "/side");
      const auto strategist = std::find_if(
          session.councils[side].participants.begin(),
          session.councils[side].participants.end(),
          [](const auto &p) { return p.role == FieldCouncilRole::Strategist; });
      if (bool(s.strategist_actor_id) !=
              (strategist != session.councils[side].participants.end()) ||
          (s.strategist_actor_id &&
           *s.strategist_actor_id != strategist->actor_id) ||
          s.initial_stratagem_points < (s.strategist_actor_id ? 1 : 0) ||
          s.stratagem_points < 0 ||
          s.stratagem_points > s.initial_stratagem_points ||
          (!s.strategist_actor_id && s.initial_stratagem_points != 0) ||
          (!abilities && (s.stratagem_points != s.initial_stratagem_points ||
                          s.feint_strength_id)))
        return invalid(id + "/side_abilities");
      if (bool(s.delegated_by_actor_id) != bool(s.delegation_command_result) ||
          (s.delegated_by_actor_id &&
           (session.councils[side].player_actor_id != s.delegated_by_actor_id ||
            *s.delegated_by_actor_id != s.commander_actor_id ||
            !std::holds_alternative<FieldAiController>(s.controller))))
        return invalid(id + "/delegation");
      if (const auto exact =
              std::get_if<FieldExactGroupContactOrigin>(&origin->source)) {
        if (!abilities || !abilities->exact_contacts || !abilities->diplomacy)
          return common_failure("FieldExitTerritoryOriginalOwnerRequired", id);
        auto assignment = read_field_exact_side_assignment(
            *abilities->exact_contacts, catalog, exact->actual_contact);
        if (!assignment.ok)
          return {false, {}, assignment.error};
        auto exits = read_field_exits_at_original_contact(
            *abilities->exact_contacts, *abilities->diplomacy,
            abilities->territory, catalog,
            {"military", "field_battle_contact", origin->contact_id},
            Integer(side));
        if (!exits.ok)
          return {false, {}, exits.error};
        if (s.faction_id != *assignment.value.faction_by_side[side] ||
            s.deploy_zone !=
                field.sides[assignment.value.geometry_end_by_side[side]]
                    .deploy_cells ||
            s.exits != exits.value.cells)
          return invalid(id + "/original_exit_policy");
      } else {
        bool zone = false;
        for (const auto &original : field.sides)
          if (s.deploy_zone == original.deploy_cells &&
              s.exits == original.exit_cells)
            zone = true;
        if (!zone)
          return invalid(id + "/deploy_zone");
      }
      for (std::size_t i = 0; i < ai.pending_triggers.size(); ++i)
        if (static_cast<Integer>(ai.pending_triggers[i]) < 0 ||
            static_cast<Integer>(ai.pending_triggers[i]) > 3 ||
            (i && ai.pending_triggers[i - 1] >= ai.pending_triggers[i]))
          return invalid(id + "/triggers");
    }
    auto council_valid = validate_field_council_records(common, session, field);
    if (!council_valid.ok)
      return council_valid;
    for (const auto &[key, order] : session.orders) {
      const bool adopted_mission =
          order.status == FieldOrderStatus::Adopted &&
          std::holds_alternative<FieldMission>(order.payload);
      if (bool(order.adopted_mission_revision) != adopted_mission ||
          (order.adopted_mission_revision &&
           *order.adopted_mission_revision <= 0))
        return invalid(key + "/adopted_revision");
      auto target = session.formations.find(order.target_formation_id);
      if (key != order.order_id || target == session.formations.end() ||
          !common.actor(order.issuer_actor_id) ||
          !time(order.issued, parameters) || !time(order.due, parameters) ||
          earlier(order.due, order.issued) || order.delay_s < 0 ||
          order.distance_m < 0 || earlier(session.now, order.issued) ||
          static_cast<Integer>(order.status) < 0 ||
          static_cast<Integer>(order.status) > 5)
        return invalid(key);
      auto due =
          add_field_battle_seconds(order.issued, order.delay_s, parameters);
      if (!due.ok || due.value.tick != order.due.tick ||
          due.value.t != order.due.t ||
          order.ai_issued == bool(order.command_result))
        return invalid(key + "/order_source");
      if (order.ai_issued &&
          (session.sides[target->second.side].commander_actor_id !=
               order.issuer_actor_id ||
           !std::holds_alternative<FieldAiController>(
               session.sides[target->second.side].controller)))
        return invalid(key + "/ai_issuer");
      if (order.status == FieldOrderStatus::InTransit) {
        if (order.status_at || order.report_due || !order.reason.empty() ||
            !order.superseded_by.empty() || earlier(order.due, session.now))
          return invalid(key + "/in_transit");
      } else {
        // 采用点是 due 所属半开步的起点，due 本身仍为最初冻结的精确整秒。
        const FieldBattleTime arrival{order.due.tick, order.due.t /
                                                          parameters.step_s *
                                                          parameters.step_s};
        auto report =
            add_field_battle_seconds(arrival, order.delay_s, parameters);
        if (!order.status_at || !order.report_due || !report.ok ||
            order.status_at->tick != arrival.tick ||
            order.status_at->t != arrival.t ||
            !earlier(*order.status_at, session.now) ||
            order.report_due->tick != report.value.tick ||
            order.report_due->t != report.value.t ||
            !order.superseded_by.empty())
          return invalid(key + "/arrival");
        if ((order.status == FieldOrderStatus::Adopted ||
             order.status == FieldOrderStatus::Delivered) &&
            !order.reason.empty())
          return invalid(key + "/arrival_reason");
        if (order.status == FieldOrderStatus::NotResponding &&
            order.reason != "FieldTacticNoCommander" &&
            order.reason != "FieldFormationRouting" &&
            order.reason != "FieldFormationLowMorale" &&
            order.reason != "FieldFormationConfused" &&
            order.reason != "FieldTacticUnknown" &&
            order.reason != "FieldTacticNoPoints" &&
            order.reason != "FieldTacticCooling")
          return invalid(key + "/arrival_reason");
        if (order.status == FieldOrderStatus::Undeliverable &&
            order.reason != "FieldFormationWithdrawn" &&
            order.reason != "FieldFormationDestroyed")
          return invalid(key + "/arrival_reason");
        // 替换的是当前任务，不倒写已采用令的历史结果和早期回传时间。
        if (order.status == FieldOrderStatus::Superseded)
          return invalid(key + "/unproduced_superseded");
        if (order.status == FieldOrderStatus::Delivered) {
          auto player = validate_field_delivered_order_authority(
              common, world, session, order,
              abilities ? abilities->original_life : nullptr);
          if (!player.ok)
            return player;
        }
      }
      if (const auto *task = std::get_if<FieldMission>(&order.payload)) {
        auto valid = mission(*task, target->second, session, field);
        if (!valid.ok)
          return valid;
      } else {
        const auto &intent = std::get<FieldTacticIntent>(order.payload);
        if (intent.tactic_id.empty())
          return invalid(key + "/tactic");
        if (order.status == FieldOrderStatus::Adopted) {
          Integer activations = 0;
          if (target->second.ability)
            for (const auto &activation : target->second.ability->active)
              if (activation.order_id == key)
                ++activations;
          if (activations != 1)
            return invalid(key + "/tactic_application");
        }
      }
    }
    auto applied_missions = validate_field_mission_application_history(
        common, world, session, abilities ? abilities->original_life : nullptr);
    if (!applied_missions.ok)
      return applied_missions;
    auto local_events = validate_field_local_event_sources(session, parameters);
    if (!local_events.ok)
      return local_events;
    auto suppression = validate_field_suppression_sources(session, parameters);
    if (!suppression.ok)
      return suppression;
    auto transitions =
        validate_field_mission_transition_events(common, world, session, field);
    if (!transitions.ok)
      return transitions;
    if (session.started_at) {
      if (!abilities)
        return common_failure("FieldNavigationOriginalOwnersRequired", id);
      auto navigation = validate_field_navigation_sources(
          common, military, world, session, field, *abilities);
      if (!navigation.ok)
        return navigation;
    } else {
      for (const auto &[key, f] : session.formations)
        if (f.navigation_target_dm || f.navigation_replanned_at ||
            f.navigation_observation_sequence || f.last_motion_sequence)
          return invalid(key + "/navigation_before_start");
      for (const auto &e : session.events)
        if (field_event_navigation_observation(e) || field_event_motion_application(e))
          return invalid(id + "/navigation_before_start");
    }
    for (const auto &[order_id, order] : session.orders)
      if (order.resolution_boundary) {
        if (!abilities || !abilities->actual_clock ||
            !abilities->original_read_boundary ||
            !abilities->original_read_boundary->bound_to(
                common, *abilities->actual_clock))
          return common_failure("FieldOrderResolutionWorldBoundaryRequired",
                                order_id);
        auto observed =
            abilities->original_read_boundary->read_observed_moment();
        if (!observed.ok)
          return {false, {}, observed.error};
        auto moment = compare_world_moments(
            order.resolution_boundary->observed_at, observed.value);
        if (!moment.ok)
          return {false, {}, moment.error};
        if (moment.value > 0)
          return invalid(order_id + "/resolution_future");
        auto actual = validate_field_control_acceptance(
            common, session, *order.resolution_boundary);
        if (!actual.ok)
          return actual;
      }
    auto order_resolution = validate_field_order_resolution_sources(session);
    if (!order_resolution.ok)
      return order_resolution;
    Integer event_sequence = 1;
    std::optional<FieldBattleTime> previous_event_at;
    std::set<StableId> order_reports, tactic_events;
    for (const auto &event : session.events) {
      auto payload_layout = validate_field_event_payload_layout(event);
      if (!payload_layout.ok) return payload_layout;
      if (event.sequence != event_sequence ||
          (previous_event_at && earlier(event.at, *previous_event_at)) ||
          event_sequence == std::numeric_limits<Integer>::max() ||
          !time(event.at, parameters) || earlier(session.now, event.at) ||
          (event.kind != field_event_kind::Ended &&
           event.kind != field_event_kind::ContactPhaseApplied &&
           !event.side) ||
          (event.side && (*event.side < 0 || *event.side > 1)))
        return invalid(id + "/event");
      // The original local phases append in execution order; a later phase
      // cannot manufacture a report/reveal back at the preceding step origin.
      // This is only chronology, not a substitute for each real event source.
      previous_event_at = event.at;
      if (event.kind == field_event_kind::Contact ||
          event.kind == field_event_kind::CommanderLost ||
          event.kind == field_event_kind::Ended ||
          event.kind == field_event_kind::MissionTransition ||
          event.kind == field_event_kind::VolleyImpact ||
          event.kind == field_event_kind::NavigationObserved ||
          event.kind == field_event_kind::MotionApplied ||
          event.kind == field_event_kind::MissionApplied ||
          event.kind == field_event_kind::ContactPhaseApplied ||
          event.kind == field_event_kind::OrderResolved) {
        // Verified bidirectionally by the original local-output validator;
        // these are not global facts and carry no invented source_id.
        ++event_sequence;
        continue;
      }
      if (event.kind == field_event_kind::Casualties ||
          event.kind == field_event_kind::Rout ||
          event.kind == field_event_kind::Rallied ||
          event.kind == field_event_kind::Withdrawn) {
        // Reconstructed from the original participation roster and this
        // formation's actual ordered outputs above, not today's mirrored unit.
        if (!session.started_at || !abilities || !abilities->original_rosters ||
            !event.formation_id ||
            !session.formations.contains(*event.formation_id))
          return invalid(id + "/local_roster_event_source");
        ++event_sequence;
        continue;
      }
      if (!event.source_id || event.source_id->empty())
        return invalid(id + "/event_source");
      if (event.kind == field_event_kind::Stratagem ||
          event.kind == field_event_kind::Revealed) {
        // Full source, payload and exact-once checks ran above. An orphan
        // event must not escape the forward record checks.
        if (!abilities || !session.stratagems.contains(*event.source_id))
          return invalid(id + "/stratagem_event_source");
        ++event_sequence;
        continue;
      }
      if (event.target_formation_id || event.count || event.stratagem)
        return invalid(id + "/event");
      if (event.kind == field_event_kind::Tactic) {
        if (!event.formation_id || !event.tactic || event.success != true ||
            !session.formations.contains(*event.formation_id))
          return invalid(id + "/tactic_event");
        const auto &formation = session.formations.at(*event.formation_id);
        Integer matches = 0;
        if (formation.side != *event.side || !formation.ability)
          return invalid(id + "/tactic_event");
        for (const auto &activation : formation.ability->active)
          if (activation.activation_id == *event.source_id &&
              activation.kind == *event.tactic &&
              activation.started.tick == event.at.tick &&
              activation.started.t == event.at.t)
            ++matches;
        if (matches != 1 || !tactic_events.insert(*event.source_id).second)
          return invalid(id + "/tactic_event");
      } else if (event.tactic || event.success)
        return invalid(id + "/unexpected_tactic_event");
      else if (event.kind == field_event_kind::OrderArrived) {
        const auto order = session.orders.find(*event.source_id);
        if (order == session.orders.end() || !order->second.report_due ||
            earlier(event.at, *order->second.report_due) ||
            event.formation_id != order->second.target_formation_id ||
            session.formations.at(order->second.target_formation_id).side !=
                *event.side ||
            !order_reports.insert(*event.source_id).second)
          return invalid(id + "/order_report");
      } else if (event.kind != field_event_kind::CouncilAdopted &&
                 event.kind != field_event_kind::RequestGranted &&
                 event.kind != field_event_kind::RequestDenied) {
        // 旧已选分支保持原完整来源门；未来注册子型必须提交自己的原证据 validator。
        auto original = validate_field_event_extension_source(
            {common,military,travel,catalog,world,session,field,abilities},event);
        if (!original.ok) return original;
      }
      ++event_sequence;
    }
    for (const auto &[order_id, order] : session.orders)
      if (order.report_due && !earlier(session.now, *order.report_due) &&
          !order_reports.contains(order_id))
        return invalid(order_id + "/report_missing");
    for (const auto &[formation_id, formation] : session.formations)
      if (formation.ability)
        for (const auto &activation : formation.ability->active)
          if (!tactic_events.contains(activation.activation_id))
            return invalid(activation.activation_id + "/event_missing");
    if (session.next_event_sequence != event_sequence)
      return invalid(id + "/event_sequence");

    std::set<StableId> pending_armies;
    if (session.state == FieldBattleState::Ended &&
        !session.pending_entries.empty())
      return invalid(id + "/terminal_pending_entries");
    for (const auto &entry : session.pending_entries) {
      bool actual_partial_queue = false;
      const auto boundary = world.boundary_contacts.find(entry.contact_id);
      if (boundary != world.boundary_contacts.end() &&
          boundary->second.battle_id == id &&
          boundary->second.reinforcement_queue) {
        const auto &queue = *boundary->second.reinforcement_queue;
        if (!queue.admitted.empty() &&
            queue.admitted.size() < queue.formation_ids.size()) {
          const auto &first = queue.admitted.front();
          const auto formation = session.formations.find(first.formation_id);
          if (formation == session.formations.end())
            return invalid(entry.contact_id + "/pending_first_admission");
          auto original = read_field_reinforcement_admission_identity(
              common, world, session, formation->second);
          if (!original.ok)
            return {false, {}, original.error};
          actual_partial_queue =
              original.value == &first && entry.entry.tick == first.at.tick &&
              entry.entry.t == first.at.t && boundary->second.army_id &&
              *boundary->second.army_id == entry.army_id &&
              boundary->second.side && *boundary->second.side == entry.side;
        }
      }
      if (entry.side < 0 || entry.side > 1 || !time(entry.entry, parameters) ||
          (earlier(entry.entry, session.now) && !actual_partial_queue) ||
          !point(entry.entry_point_dm, field) ||
          !pending_armies.insert(entry.army_id).second)
        return invalid(id + "/pending_entries");
      const auto army = std::find_if(
          military.armies.begin(), military.armies.end(),
          [&](const auto &row) { return row.id == entry.army_id; });
      if (army == military.armies.end() || army->status != "active")
        return invalid(entry.army_id);
    }
    for (const auto &[key, remainder] : session.pulse_remainders)
      if (remainder < 0 || !session.formations.contains(key.first) ||
          !session.formations.contains(key.second))
        return invalid(id + "/pulse_remainder");
    for (const auto *remainders :
         {&session.morale_remainders, &session.flank_remainders})
      for (const auto &[key, remainder] : *remainders)
        if (remainder < 0 || !session.formations.contains(key))
          return invalid(id + "/morale_remainder");
    // 原 Godot PCG 以有符号 Variant int64 保存位型；合法负值不得按 uint64
    // 拒绝。
    for (const auto *text : {&session.rng_seed, &session.rng_state}) {
      Integer rng = 0;
      const auto parsed =
          std::from_chars(text->data(), text->data() + text->size(), rng);
      if (text->empty() || parsed.ec != std::errc{} ||
          parsed.ptr != text->data() + text->size() ||
          std::to_string(rng) != *text)
        return invalid(id + "/rng_state");
    }
    if (session.state == FieldBattleState::Ended) {
      if (!session.result || !time(session.result->ended, parameters) ||
          session.result->winner_side < -1 || session.result->winner_side > 1)
        return invalid(id + "/result");
      if (std::holds_alternative<FieldExactGroupContactOrigin>(origin->source))
        for (const auto &original : origin->original_bindings) {
          const auto active = world.army_bindings.find(original.army_id);
          if (active != world.army_bindings.end() &&
              active->second.battle_id == id)
            continue;
          if (!exact)
            return common_failure("FieldWinnerHistoricalContinuationRequired",
                                  original.army_id);
          Integer adopted = 0;
          for (const auto &item :
               exact->movement.contact_continuations.rows()) {
            const auto &row = item.second;
            if (row.actual_resolution_source.domain != "military" ||
                row.actual_resolution_source.kind != "field_battle_segment")
              continue;
            const bool terminal = std::any_of(
                session.settlements.begin(), session.settlements.end(),
                [&](const auto &v) {
                  return v.battle_fact_id &&
                         v.fact_id == row.actual_resolution_source.id;
                });
            const auto unit = exact->movement.units.find(row.closed_unit_ref);
            const auto group =
                unit ? std::get_if<GroupMovementExecution>(&unit->execution)
                     : nullptr;
            const auto army =
                group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
            if (!terminal || !army || army->army_id != original.army_id)
              continue;
            const SourceRef actual{"travel", "movement_contact_continuation",
                                   row.id};
            auto read = read_field_winner_continuation(
                *exact, catalog, id, original.army_id, &actual);
            if (!read.ok)
              return {false, {}, read.error};
            ++adopted;
          }
          for (const auto &item : exact->movement.arrivals.rows()) {
            const auto &row = item.second;
            const auto *anchor =
                std::get_if<OriginalMovementAnchor>(&row.actual_anchor);
            const auto *group =
                std::get_if<GroupMovementExecution>(&row.execution);
            const auto *army =
                group ? std::get_if<ArmyCarrierRef>(&group->carrier) : nullptr;
            if (!anchor || anchor->actual_source.domain != "military" ||
                anchor->actual_source.kind != "field_battle_segment" || !army ||
                army->army_id != original.army_id)
              continue;
            if (!std::any_of(session.settlements.begin(),
                             session.settlements.end(), [&](const auto &v) {
                               return v.battle_fact_id &&
                                      v.fact_id == anchor->actual_source.id;
                             }))
              continue;
            const SourceRef actual{"travel", "movement_arrival", item.first};
            auto read = read_field_winner_continuation(
                *exact, catalog, id, original.army_id, &actual);
            if (!read.ok)
              return {false, {}, read.error};
            ++adopted;
          }
          if (adopted != 1)
            return common_failure("FieldWinnerHistoricalContinuationRequired",
                                  original.army_id);
        }
    } else {
      if (session.result)
        return invalid(id + "/result");
      for (const auto &original : origin->original_bindings) {
        const auto &army = original.army_id;
        const auto binding = world.army_bindings.find(army);
        if (binding == world.army_bindings.end() ||
            binding->second.battle_id != id)
          return invalid(army + "/binding");
      }
    }
  }
  auto council_facts = validate_field_council_facts(
      common, world, abilities ? abilities->original_life : nullptr);
  if (!council_facts.ok)
    return council_facts;
  for (const auto &[id, header] : common.state().facts.rows()) {
    if (header.kind == "field_battle_contact" && !world.contacts.contains(id) &&
        !world.boundary_contacts.contains(id))
      return invalid(id);
    if (header.kind == "field_battle_session" && !world.sessions.contains(id))
      return invalid(id);
    if (header.kind == "field_battle_exit" && !world.exits.contains(id))
      return invalid(id);
    if (header.kind == "field_battle_entry_queue" ||
        header.kind == "field_battle_entered") {
      Integer owners = 0;
      const auto matches = [&](const SourceRef &source) {
        return source.id == id && source.kind == header.kind &&
               source.domain == header.domain;
      };
      for (const auto &[key, contact] : world.boundary_contacts) {
        // 入场队列逐项拥有来源，边界的首项别名不得再次计数。
        if (contact.reinforcement_queue) {
          const auto &queue = *contact.reinforcement_queue;
          if (header.kind == "field_battle_entry_queue")
            owners += matches(queue.initialized_source) ? 1 : 0;
          else
            for (const auto &admission : queue.admitted)
              owners += matches(admission.application_source) ? 1 : 0;
        } else if (header.kind == "field_battle_entered" &&
                   contact.entry_application_source &&
                   matches(*contact.entry_application_source))
          ++owners;
      }
      if (header.domain != "military" || owners != 1)
        return invalid(id);
    }
    if (header.kind == "field_commander_changed") {
      auto actual = read_field_commander_change_source(
          common, world, SourceRef{header.domain, header.kind, id});
      if (!actual.ok)
        return {false, {}, actual.error};
    }
    if (header.kind == "field_battle_stratagem") {
      Integer owners = 0;
      for (const auto &[battle, session] : world.sessions)
        for (const auto &[key, row] : session.stratagems)
          if (row.kind == FieldStratagemKind::Confuse && key == id &&
              row.adopted_source.id == id &&
              row.adopted_source.domain == header.domain &&
              row.adopted_source.kind == header.kind)
            ++owners;
      if (owners != 1)
        return invalid(id);
    }
    if (header.kind == "field_battle_started") {
      Integer owners = 0;
      for (const auto &[key, session] : world.sessions)
        if (session.started_source && session.started_source->id == id &&
            session.started_source->kind == header.kind &&
            session.started_source->domain == header.domain)
          ++owners;
      if (owners != 1)
        return invalid(id);
    }
  }
  // 完整来源仍由以上同 World reader 校验；最后统一核 owned Common 头闭包。
  return validate_field_battle_fact_headers(common, world);
}
} // namespace hanmo::resident
