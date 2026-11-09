#include "field_event_legacy_kinds.hpp"
#include "field_roster_origin.hpp"
#include "field_ability_sources.hpp"
#include "resident/domains/military/naval_contact_history.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
Outcome<MilitaryUnitRecord> read_field_original_participation_roster(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation,
    const FieldOriginalRosterReadContext &original) {
  auto fail = [&]() -> Outcome<MilitaryUnitRecord> {
    return {false,
            {},
            common_failure("FieldOriginalRosterSourceRequired",
                           formation.formation_id)
                .error};
  };
  auto actual_session = world.sessions.find(session.battle_id);
  auto actual_formation = session.formations.find(formation.formation_id);
  if (&original.common != &common || &original.military != &military ||
      actual_session == world.sessions.end() ||
      &actual_session->second != &session ||
      actual_formation == session.formations.end() ||
      &actual_formation->second != &formation ||
      !original.formations.bound_to(common.state(), military) ||
      !original.returns.bound_to(common, military))
    return fail();
  auto boundary = read_field_ability_source_boundary(
      common, world, session, formation, &original.clock);
  if (!boundary.ok)
    return {false, {}, boundary.error};
  // Ordinal cut is the original Common head. The enclosing actual S0 clock is
  // read by the original owner; local battlefield seconds never become M.
  // Reuse the existing tracked Common-prefix scratch, not its Naval world
  // context. This helper allocates no naval state and grants no command right.
  auto cut =
      read_naval_original_prefix_before(common, boundary.value.source->id);
  if (!cut.ok)
    return {false, {}, cut.error};
  auto now = original.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  if (!original.field_applications)
    return fail();
  const MilitaryFieldHistoryRead field_history{world,
                                               *original.field_applications};
  auto rows = read_military_rosters_at_selected_actual_prefix(
      common, military, catalog, original.identity, original.exact,
      {formation.formation_id}, cut.value.prefix, now.value,
      original.population, original.population_catalog, original.budget,
      &original.formations, &original.returns, &original.periods, nullptr,
      &field_history);
  if (!rows.ok)
    return {false, {}, rows.error};
  if (rows.value.units.size() != 1)
    return fail();
  auto &row = rows.value.units.front();
  Integer morale_at_participation = row.morale;
  if (boundary.value.source->kind != "field_battle_entered") {
    const FieldCouncilFact *adopted = nullptr;
    for (const auto &[id, fact] : world.council_facts)
      if (fact.battle_id == session.battle_id && fact.side == formation.side &&
          fact.kind == FieldCouncilFactKind::Adopted) {
        const auto *head = common.state().facts.find(id);
        const auto &order = common.state().facts.order();
        const auto adopted_at = std::find(order.begin(), order.end(), id);
        const auto start_at =
            std::find(order.begin(), order.end(), boundary.value.source->id);
        if (adopted || !head || head->domain != "military" ||
            head->kind != "military_council" || adopted_at == order.end() ||
            start_at == order.end() || adopted_at >= start_at ||
            !fact.council.closed || !fact.council.adopted_proposal_id)
          return fail();
        adopted = &fact;
      }
    if (!adopted)
      return fail();
    const auto &council = adopted->council;
    const auto chosen = std::find_if(
        council.proposals.begin(), council.proposals.end(), [&](const auto &p) {
          return p.proposal_id == *council.adopted_proposal_id;
        });
    if (chosen == council.proposals.end())
      return fail();
    if (formation.commander_actor_id == chosen->speaker_actor_id)
      morale_at_participation =
          std::min<Integer>(100, morale_at_participation + 5);
    else
      for (const auto &proposal : council.proposals)
        if (proposal.stance == FieldCouncilStance::Aggressive &&
            proposal.speaker_actor_id == formation.commander_actor_id)
          morale_at_participation =
              std::max<Integer>(0, morale_at_participation - 3);
  }
  if (row.id != formation.formation_id || row.army_id != formation.army_id ||
      row.troop_type_id != formation.troop_type_id ||
      row.commander_actor_id != formation.commander_actor_id ||
      formation.fit_start != Integer(row.fit.size()) ||
      formation.morale_start != morale_at_participation)
    return fail();
  return {true, std::move(row), {}};
}
} // namespace hanmo::resident

#include "resident/domains/military/military_attack_damage_core.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 热/冷读共用原名册归约；cut之外不消费事件，也不从今日fit反猜过去。
Outcome<MilitaryUnitRecord> local_roster_prefix(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &f,
    const FieldOriginalRosterReadContext &sources, Integer cut,
    Integer &ordinal, bool &withdrawn) {
  auto start = read_field_original_participation_roster(
      common, military, catalog, world, session, f, sources);
  if (!start.ok)
    return {false, {}, start.error};
  auto row = std::move(start.value);
  auto fail = [&]() -> Outcome<MilitaryUnitRecord> {
    return {false, {}, common_failure("FieldLocalRosterOutputMismatch", f.formation_id).error};
  };
  if (f.fit_start != Integer(row.fit.size()))
    return fail();
  if (cut < 0 || cut >= session.next_event_sequence)
    return fail();
  ordinal = 0;
  withdrawn = false;
  Integer sequence = 0;
  bool routing = false;
  const auto before = [](const FieldBattleTime &a, const FieldBattleTime &b) {
    return std::tie(a.tick, a.t) < std::tie(b.tick, b.t);
  };
  const auto desert = [&]() {
    // 与真实routing出口writer一致：兼任兵员的活将领亦保军籍，不变成逃散兵。
    for (auto *members : {&row.fit, &row.wounded}) {
      const auto retained = [&](const StableId &actor) {
        return actor == f.commander_actor_id || f.deputy_actor_id == actor;
      };
      for (const auto &actor : *members)
        if (!retained(actor)) row.deserted.push_back(actor);
      members->erase(std::remove_if(members->begin(), members->end(),
          [&](const StableId &actor) { return !retained(actor); }), members->end());
    }
  };
  for (const auto &event : session.events) {
    if (event.sequence > cut) break;
    if (event.sequence <= sequence)
      return fail();
    sequence = event.sequence;
    if (event.kind == field_event_kind::Ended) {
      // 總設計師 2026-10-29 Slack 批准，1793335838.005349：
      // remaining terminal Routing members must have the actual preceding
      // Withdrawn event; Ended cannot silently substitute for its handoff.
      if (routing && !withdrawn && f.state != FieldFormationState::Destroyed)
        return fail();
      continue;
    }
    if (event.formation_id != f.formation_id)
      continue;
    const bool roster_event = event.kind == field_event_kind::Casualties ||
                              event.kind == field_event_kind::Rout ||
                              event.kind == field_event_kind::Rallied ||
                              event.kind == field_event_kind::Withdrawn;
    if (!roster_event)
      continue;
    if (withdrawn || before(event.at, f.joined_at) ||
        before(session.now, event.at) || event.side != f.side ||
        event.target_formation_id || event.source_id || event.tactic ||
        event.stratagem || event.success)
      return fail();
    if (event.kind == field_event_kind::Casualties) {
      if (!event.count || *event.count <= 0 ||
          *event.count > Integer(row.fit.size()) ||
          ordinal > std::numeric_limits<Integer>::max() - *event.count)
        return fail();
      auto ordered = row.fit;
      std::sort(ordered.begin(), ordered.end());
      for (Integer n = 0; n < *event.count; ++n) {
        row.fit.erase(std::find(row.fit.begin(), row.fit.end(), ordered[n]));
        if (original_military_casualty_is_wounded(ordinal++, *event.count,
                                                  false))
          row.wounded.push_back(ordered[n]);
        else
          row.dead.push_back(ordered[n]);
      }
    } else if (event.kind == field_event_kind::Rout) {
      if (routing || row.fit.empty() || !event.count || *event.count < 0)
        return fail();
      routing = true;
      const Integer total = Integer(row.fit.size() + row.wounded.size());
      const bool all = *event.count == total;
      if (!all && *event.count != 0 &&
          *event.count != Integer(row.fit.size()) / 2)
        return fail();
      auto ordered = row.fit;
      std::sort(ordered.begin(), ordered.end());
      const Integer fit_count = all ? Integer(ordered.size()) : *event.count;
      std::vector<StableId> captured;
      for (Integer n = 0; n < fit_count; ++n) {
        captured.push_back(ordered[n]);
        row.fit.erase(std::find(row.fit.begin(), row.fit.end(), ordered[n]));
      }
      if (all) {
        captured.insert(captured.end(), row.wounded.begin(), row.wounded.end());
        row.wounded.clear();
      }
      for (const auto &actor : captured) {
        const std::pair<StableId, StableId> *keeper = nullptr;
        for (const auto &pair : f.captors)
          if (pair.first == actor) {
            if (keeper)
              return fail();
            keeper = &pair;
          }
        if (!keeper)
          return fail();
        bool enemy = false;
        for (const auto &[id, other] : session.formations)
          if (other.army_id == keeper->second && other.side != f.side &&
              !before(event.at, other.joined_at))
            enemy = true;
        if (!enemy)
          return fail();
        row.captured.push_back(actor);
        row.captors.push_back(*keeper);
      }
    } else if (event.kind == field_event_kind::Rallied) {
      if (!routing || event.count || row.fit.empty())
        return fail();
      routing = false;
    } else {
      if (event.count)
        return fail();
      if (routing)
        desert();
      withdrawn = true;
    }
  }
  return {true, std::move(row), {}};
}
} // namespace
// 指定原事件cut的名册读口；真实参与来源仍由同World reader独占核验。
Outcome<MilitaryUnitRecord> read_field_local_roster_at_event_prefix(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation,
    const FieldOriginalRosterReadContext &sources, Integer cut) {
  Integer ordinal = 0;
  bool withdrawn = false;
  return local_roster_prefix(common, military, catalog, world, session,
                             formation, sources, cut, ordinal, withdrawn);
}
// 原全量验证保持逐五类与真实退出状态对称比较，不因增加历史读口放宽。
Status validate_field_local_roster_outputs(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &f,
    const FieldOriginalRosterReadContext &sources) {
  if (session.next_event_sequence <= 0)
    return common_failure("FieldLocalRosterOutputMismatch", f.formation_id);
  // 全量原门不允许历史cut把非法尾事件静默截掉。
  for (const auto &event : session.events)
    if (event.sequence >= session.next_event_sequence)
      return common_failure("FieldLocalRosterOutputMismatch", f.formation_id);
  Integer ordinal = 0;
  bool withdrawn = false;
  auto result = local_roster_prefix(common, military, catalog, world, session,
                                    f, sources, session.next_event_sequence - 1,
                                    ordinal, withdrawn);
  if (!result.ok) return {false, {}, result.error};
  const auto &row = result.value;
  if (row.fit != f.fit || row.wounded != f.wounded || row.dead != f.dead ||
      row.captured != f.captured || row.deserted != f.deserted ||
      row.captors != f.captors || ordinal != f.casualty_ordinal ||
      withdrawn != (f.state == FieldFormationState::Withdrawn))
    return common_failure("FieldLocalRosterOutputMismatch", f.formation_id);
  return common_success();
}
} // namespace hanmo::resident
