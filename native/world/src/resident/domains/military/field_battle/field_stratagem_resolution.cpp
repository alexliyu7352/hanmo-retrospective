#include "field_event_legacy_kinds.hpp"
#include "field_stratagem_resolution.hpp"
#include "field_council_validation.hpp"
#include "field_integer_geometry.hpp"
#include "resident/domains/military/military_round_rng.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
bool before(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) < std::tie(b.tick, b.t);
}
bool present(const FieldFormation &f) {
  return !f.fit.empty() && f.state != FieldFormationState::Withdrawn &&
         f.state != FieldFormationState::Destroyed &&
         f.state != FieldFormationState::Deploying;
}
} // namespace
static Status resolve_stratagems(
    CommonStore &common, FieldBattleWorldState &world, const StableId &battle,
    const GeneratedBattleField &field,
    const FieldBattleAbilityReadContext &sources,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner,
    const StableId *adopted_boundary, bool after_actual_motion = false) {
  const auto found = world.sessions.find(battle);
  if (found == world.sessions.end())
    return common_failure("FieldBattleNotFound", battle);
  auto &session = found->second;
  if ((!adopted_boundary && session.state != FieldBattleState::Running) ||
      session.state == FieldBattleState::Ended ||
      session.field_hash != field.field_hash ||
      (!adopted_boundary && step.steps != 1) ||
      session.now.tick != step.from.tick || session.now.t != step.from.t)
    return common_failure("FieldStratagemStepSourceRequired", battle);
  std::vector<StableId> ordered;
  for (const auto &[id, row] : session.stratagems)
    if (!adopted_boundary || row.adopted_source.id == *adopted_boundary)
      ordered.push_back(id);
  std::sort(ordered.begin(), ordered.end(), [&](const auto &a, const auto &b) {
    return std::tie(session.stratagems.at(a).issued_sequence, a) <
           std::tie(session.stratagems.at(b).issued_sequence, b);
  });


  (owner).grow(0);
  auto event = [&](const FieldStratagemRecord &row,
                   FieldBattleEventKind kind) -> Status {
    if (session.next_event_sequence <= 0 ||
        session.next_event_sequence == std::numeric_limits<Integer>::max())
      return common_failure("FieldEventSequenceExhausted", battle);
    FieldBattleEvent e;
    e.sequence = session.next_event_sequence++;
    e.at = after_actual_motion && kind == field_event_kind::Revealed
               ? step.to
               : session.now;
    e.kind = kind;
    e.side = row.side;
    e.stratagem = row.kind;
    e.source_id = row.stratagem_id;
    e.success = row.success;
    e.formation_id = row.formation_id;
    e.target_formation_id = row.target_formation_id;
    session.events.push_back(std::move(e));
    return common_success();
  };
  Integer previous_sequence = 0;
  for (const auto &id : ordered) {
    auto &row = session.stratagems.at(id);
    if (row.stratagem_id != id || row.side < 0 || row.side > 1 ||
        row.issued_sequence <= previous_sequence ||
        bool(row.success) != bool(row.resolved_at))
      return common_failure("FieldStratagemSourceMismatch", id);
    previous_sequence = row.issued_sequence;
    if (!row.resolved_at) {
      if ((row.kind == FieldStratagemKind::Confuse) == bool(adopted_boundary))
        return common_failure("FieldStratagemAdoptionTimingMismatch", id);
      auto strategist = read_field_officer_statistics_before_stratagem(
          common, sources, world, session, row, row.strategist_actor_id);
      if (!strategist.ok)
        return {false, {}, strategist.error};
      bool success = false;
      if (row.kind == FieldStratagemKind::Confuse) {
        if (!row.resolve_at || !row.target_formation_id ||
            !row.target_commander_actor_id || row.formation_id || row.area_dm)
          return common_failure("FieldStratagemSourceMismatch", id);
        auto due = add_field_battle_seconds(row.issued, 120, field.parameters);
        if (!due.ok)
          return {false, {}, due.error};
        if (before(*row.resolve_at, due.value) ||
            before(due.value, *row.resolve_at))
          return common_failure("FieldStratagemSourceMismatch", id);
        if (!before(*row.resolve_at, step.to))
          continue;
        if (before(*row.resolve_at, step.from))
          return common_failure("FieldStratagemUnconsumedDue", id);
        auto target = read_field_officer_statistics_before_stratagem(
            common, sources, world, session, row,
            *row.target_commander_actor_id);
        if (!target.ok)
          return {false, {}, target.error};
        const auto chance = std::clamp<__int128_t>(
            200 +
                (__int128_t(strategist.value.attributes.intellect) -
                 target.value.attributes.intellect) *
                    2 +
                strategist.value.tactics,
            40, 360);
        if (chance != row.success_chance_quarters)
          return common_failure("FieldStratagemChanceMismatch", id);
        auto draw = military_round_random_draw(session.rng_seed,
                                               session.rng_state, 1, 400);
        if (!draw.ok)
          return {false, {}, draw.error};
        success = draw.value <= chance;
        auto target_formation =
            session.formations.find(*row.target_formation_id);
        if (target_formation == session.formations.end())
          return common_failure("FieldStratagemTargetRequired", id);
        if (success && present(target_formation->second)) {
          auto until =
              add_field_battle_seconds(session.now, 600, field.parameters);
          if (!until.ok)
            return {false, {}, until.error};
          target_formation->second.statuses.push_back(
              {FieldStatusKind::Confused, id, until.value});
        }
      } else {
        if (row.resolve_at || row.target_formation_id ||
            row.target_commander_actor_id || row.success_chance_quarters != 0)
          return common_failure("FieldStratagemSourceMismatch", id);
        const auto adoption = world.council_facts.find(row.adopted_source.id);
        if (adoption == world.council_facts.end() ||
            !adoption->second.council.adopted_proposal_id)
          return common_failure("FieldStratagemAdoptionSourceRequired", id);
        const auto &council = adoption->second.council;
        const auto selected =
            std::find_if(council.proposals.begin(), council.proposals.end(),
                         [&](const auto &p) {
                           return p.proposal_id == *council.adopted_proposal_id;
                         });
        if (selected == council.proposals.end())
          return common_failure("FieldStratagemAdoptionSourceRequired", id);
        Integer matches = 0;
        for (const auto &item : selected->plan.stratagems) {
          if (item.voided_at_adoption)
            continue;
          const bool ambush = item.kind == FieldCouncilStratagemKind::Ambush;
          if (ambush != (row.kind == FieldStratagemKind::Ambush))
            continue;
          if (item.formation_id != row.formation_id ||
              bool(item.area_dm) != bool(row.area_dm))
            continue;
          if (item.area_dm && (item.area_dm->x != row.area_dm->x ||
                               item.area_dm->y != row.area_dm->y))
            continue;
          ++matches;
        }
        if (matches != 1)
          return common_failure("FieldStratagemAdoptionSourceRequired", id);
        const auto enemy_scout = session.councils[1 - row.side].intel.level;
        if (row.kind == FieldStratagemKind::Ambush) {
          if (!row.formation_id || !row.area_dm ||
              !session.formations.contains(*row.formation_id))
            return common_failure("FieldStratagemFormationRequired", id);
          const auto x = __int128_t(row.area_dm->x) +
                         __int128_t(field.parameters.length_m) * 5;
          const auto y = __int128_t(row.area_dm->y) +
                         __int128_t(field.parameters.width_m) * 5;
          const auto size = __int128_t(field.parameters.cell_m) * 10;
          if (size <= 0 || x < 0 || y < 0 || x >= size * field.columns ||
              y >= size * field.rows)
            return common_failure("FieldStratagemAreaInvalid", id);
          const auto index =
              std::size_t(y / size) * std::size_t(field.columns) +
              std::size_t(x / size);
          if (index >= field.cells.size())
            return common_failure("FieldStratagemAreaInvalid", id);
          success = enemy_scout < 3 &&
                    (field.cells[index].terrain == FieldTerrainKind::Forest ||
                     field.cells[index].terrain == FieldTerrainKind::Rough);
          if (success)
            session.formations.at(*row.formation_id)
                .statuses.push_back({FieldStatusKind::Hidden, id, {}});
          else
            row.revealed_at = after_actual_motion ? step.to : session.now;
        } else if (row.kind == FieldStratagemKind::FeintStrength) {
          if (row.formation_id || row.area_dm)
            return common_failure("FieldStratagemSourceMismatch", id);
          auto enemy = read_field_officer_statistics_before_council_adoption(
              common, sources, world, adoption->second,
              session.sides[1 - row.side].commander_actor_id);
          if (!enemy.ok)
            return {false, {}, enemy.error};
          success =
              enemy_scout < 2 && enemy.value.attributes.intellect <=
                                     strategist.value.attributes.intellect;
          if (success)
            session.sides[1 - row.side].feint_strength_id = id;
        } else
          return common_failure("FieldStratagemKindInvalid", id);
      }
      row.success = success;
      row.resolved_at = session.now;
      auto recorded = event(row, field_event_kind::Stratagem);
      if (!recorded.ok)
        return recorded;
    }
    if (!adopted_boundary && row.kind == FieldStratagemKind::Ambush &&
        row.success.value_or(false) && !row.revealed_at) {
      if (!row.formation_id || !session.formations.contains(*row.formation_id))
        return common_failure("FieldStratagemFormationRequired", id);
      auto &f = session.formations.at(*row.formation_id);
      bool reveal = !std::holds_alternative<FieldHold>(f.mission);
      for (const auto &[other_id, other] : session.formations)
        if (other.side != f.side && present(other) &&
            field_distance_squared(f.position_dm, other.position_dm) <=
                __int128_t(1500) * 1500)
          reveal = true;
      if (reveal) {
        row.revealed_at = session.now;
        f.statuses.erase(std::remove_if(f.statuses.begin(), f.statuses.end(),
                                        [&](const auto &status) {
                                          return status.kind ==
                                                     FieldStatusKind::Hidden &&
                                                 status.source_id == id;
                                        }),
                         f.statuses.end());
        auto recorded = event(row, field_event_kind::Revealed);
        if (!recorded.ok)
          return recorded;
      }
    }
  }
  return common_success();
}
Status resolve_field_stratagems_in_candidate(
    CommonStore &common, FieldBattleWorldState &world, const StableId &battle,
    const GeneratedBattleField &field,
    const FieldBattleAbilityReadContext &sources,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner) {
  return resolve_stratagems(common, world, battle, field, sources, step, owner,
                            nullptr);
}
Status reveal_field_stratagems_after_motion_in_candidate(
    CommonStore &common, FieldBattleWorldState &world, const StableId &battle,
    const GeneratedBattleField &field,
    const FieldBattleAbilityReadContext &sources,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner) {
  return resolve_stratagems(common, world, battle, field, sources, step, owner,
                            nullptr, true);
}
Status install_field_adopted_stratagems_in_candidate(
    CommonStore &common, FieldBattleWorldState &world, const StableId &id,
    const GeneratedBattleField &field,
    const FieldBattleAbilityReadContext &sources,
    CheckpointMemoryCharge &owner) {
  const auto accepted = world.council_facts.find(id);
  const auto *head = common.state().facts.find(id);
  if (accepted == world.council_facts.end() || !head ||
      head->domain != "military" || head->kind != "military_council")
    return common_failure("FieldStratagemAdoptionSourceRequired", id);
  const auto &fact = accepted->second;
  auto actual = world.sessions.find(fact.battle_id);
  if (fact.kind != FieldCouncilFactKind::Adopted ||
      actual == world.sessions.end() || fact.side < 0 || fact.side > 1 ||
      !fact.council.closed || !fact.council.adopted_proposal_id)
    return common_failure("FieldStratagemAdoptionSourceRequired", id);
  auto &session = actual->second;
  if (before(fact.at, session.now) || before(session.now, fact.at) ||
      fact.recorded_tick != common.tick())
    return common_failure("FieldStratagemAdoptionTimingMismatch", id);
  auto selected =
      std::find_if(fact.council.proposals.begin(), fact.council.proposals.end(),
                   [&](const auto &p) {
                     return p.proposal_id == *fact.council.adopted_proposal_id;
                   });
  if (selected == fact.council.proposals.end())
    return common_failure("FieldStratagemAdoptionSourceRequired", id);
  std::vector<FieldCouncilStratagemPlan> plans;
  for (const auto &item : selected->plan.stratagems) {
    if (item.voided_at_adoption ==
        field_council_stratagem_assignment_eligible(item, selected->plan))
      return common_failure("FieldStratagemAdoptionEligibilityMismatch", id);
    if (!item.voided_at_adoption)
      plans.push_back(item);
  }
  Integer existing = 0;
  for (const auto &[key, row] : session.stratagems)
    if (row.adopted_source.id == id)
      ++existing;
  if (existing && existing != Integer(plans.size()))
    return common_failure("FieldStratagemAdoptionPartial", id);
  if (!existing && !plans.empty()) {
    auto &side = session.sides[fact.side];
    if (!side.strategist_actor_id ||
        side.stratagem_points < Integer(plans.size()))
      return common_failure("FieldStratagemPointsRequired", id);
    if (common.state().next_instance_id <= 0 ||
        __int128_t(common.state().next_instance_id) + plans.size() >
            std::numeric_limits<Integer>::max() ||
        session.next_stratagem_sequence <= 0 ||
        __int128_t(session.next_stratagem_sequence) + plans.size() >
            std::numeric_limits<Integer>::max())
      return common_failure("FieldStratagemSequenceExhausted", id);

    for (std::size_t n = 0; n < plans.size(); ++n) {
      char text[32];
      auto decimal =
          std::to_chars(text, text + sizeof(text),
                        common.state().next_instance_id + Integer(n));
      if (decimal.ec != std::errc{})
        return common_failure("FieldStratagemSequenceExhausted", id);
      const ProducedStableIdBuildView produced{
          "field_stratagem",
          std::string_view(text, std::size_t(decimal.ptr - text))};


    }
    (owner).grow(0);
    for (const auto &planned : plans) {
      if (planned.kind != FieldCouncilStratagemKind::Ambush &&
          planned.kind != FieldCouncilStratagemKind::FeintStrength)
        return common_failure("FieldStratagemKindInvalid", id);
      auto identity = common.next_id("field_stratagem");
      if (!identity.ok)
        return {false, {}, identity.error};
      FieldStratagemRecord row;
      row.stratagem_id = identity.value;
      row.side = fact.side;
      row.kind = planned.kind == FieldCouncilStratagemKind::Ambush
                     ? FieldStratagemKind::Ambush
                     : FieldStratagemKind::FeintStrength;
      row.issuer_actor_id = side.commander_actor_id;
      row.strategist_actor_id = *side.strategist_actor_id;
      row.issued_sequence = session.next_stratagem_sequence++;
      row.issued = session.now;
      row.adopted_source = {"military", "military_council", id};
      row.observed_tick = fact.recorded_tick;
      if (!fact.adopted_actual_at)
        return common_failure("FieldCouncilActualMomentRequired", id);
      row.applied_at = *fact.adopted_actual_at;
      row.formation_id = planned.formation_id;
      row.area_dm = planned.area_dm;
      if (!session.stratagems.emplace(identity.value, std::move(row)).second)
        return common_failure("FieldStratagemRepeated", identity.value);
      --side.stratagem_points;
    }
  }
  FieldBattleWindowSlice boundary;
  boundary.from = session.now;
  boundary.to = session.now;
  boundary.segment = session.segment;
  return resolve_stratagems(common, world, session.battle_id, field, sources,
                            boundary, owner, &id);
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status validate_field_stratagem_sources(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const GeneratedBattleField &field,
    const FieldBattleAbilityReadContext &sources) {
  auto fail = [&](const StableId &id) {
    return common_failure("FieldStratagemSourceMismatch", id);
  };
  const auto root = world.sessions.find(session.battle_id);
  if (root == world.sessions.end() || &root->second != &session ||
      session.field_hash != field.field_hash)
    return fail(session.battle_id);
  const auto &parameters = field.parameters;
  auto valid_time = [&](const FieldBattleTime &at) {
    return at.tick >= 0 && at.t >= 0 && at.t < parameters.window_s;
  };
  auto seconds = [&](const FieldBattleTime &a, const FieldBattleTime &b) {
    return (__int128_t(a.tick) - b.tick) * parameters.window_s + a.t - b.t;
  };
  std::set<Integer> sequences;
  std::array<Integer, 2> spent{};
  std::array<std::optional<StableId>, 2> feints;
  for (const auto &[id, row] : session.stratagems) {
    if (id != row.stratagem_id || row.side < 0 || row.side > 1 ||
        row.issued_sequence <= 0 ||
        row.issued_sequence >= session.next_stratagem_sequence ||
        !sequences.insert(row.issued_sequence).second ||
        !valid_time(row.issued) || before(session.now, row.issued) ||
        row.observed_tick < session.created_tick ||
        row.observed_tick > common.tick() ||
        bool(row.resolved_at) != bool(row.success) ||
        (row.resolved_at && (!valid_time(*row.resolved_at) ||
                             before(*row.resolved_at, row.issued) ||
                             before(session.now, *row.resolved_at))))
      return fail(id);
    ++spent[row.side];
    auto strategist = read_field_officer_statistics_before_stratagem(
        common, sources, world, session, row, row.strategist_actor_id);
    if (!strategist.ok)
      return {false, {}, strategist.error};
    if (row.kind == FieldStratagemKind::Confuse) {
      if (!row.command_result || !row.resolve_at || !row.target_formation_id ||
          !row.target_commander_actor_id || row.formation_id || row.area_dm ||
          row.revealed_at || row.first_contact_consumed)
        return fail(id);
      auto due = add_field_battle_seconds(row.issued, 120, parameters);
      if (!due.ok)
        return {false, {}, due.error};
      if (seconds(*row.resolve_at, due.value) != 0)
        return fail(id);
      auto target = read_field_officer_statistics_before_stratagem(
          common, sources, world, session, row, *row.target_commander_actor_id);
      if (!target.ok)
        return {false, {}, target.error};
      const auto chance = std::clamp<__int128_t>(
          200 +
              (__int128_t(strategist.value.attributes.intellect) -
               target.value.attributes.intellect) *
                  2 +
              strategist.value.tactics,
          40, 360);
      if (chance != row.success_chance_quarters)
        return fail(id);
      if (row.resolved_at) {
        auto offset = seconds(*row.resolved_at, *row.resolve_at);
        if (offset < 0 || offset >= parameters.step_s)
          return fail(id);
      } else if (before(*row.resolve_at, session.now))
        return fail(id);
    } else {
      if (row.command_result || row.resolve_at || row.target_formation_id ||
          row.target_commander_actor_id || row.success_chance_quarters != 0 ||
          !row.resolved_at)
        return fail(id);
      const auto adoption = world.council_facts.find(row.adopted_source.id);
      if (adoption == world.council_facts.end() ||
          !adoption->second.council.adopted_proposal_id ||
          seconds(row.issued, adoption->second.at) != 0 ||
          seconds(*row.resolved_at, row.issued) != 0)
        return fail(id);
      const auto &c = adoption->second.council;
      auto selected = std::find_if(
          c.proposals.begin(), c.proposals.end(), [&](const auto &p) {
            return p.proposal_id == *c.adopted_proposal_id;
          });
      if (selected == c.proposals.end())
        return fail(id);
      Integer matches = 0;
      for (const auto &item : selected->plan.stratagems)
        if (!item.voided_at_adoption &&
            (item.kind == FieldCouncilStratagemKind::Ambush) ==
                (row.kind == FieldStratagemKind::Ambush) &&
            item.formation_id == row.formation_id &&
            bool(item.area_dm) == bool(row.area_dm) &&
            (!item.area_dm || (item.area_dm->x == row.area_dm->x &&
                               item.area_dm->y == row.area_dm->y)))
          ++matches;
      if (matches != 1)
        return fail(id);
      bool success = false;
      if (row.kind == FieldStratagemKind::Ambush) {
        if (!row.formation_id || !row.area_dm ||
            !session.formations.contains(*row.formation_id) ||
            session.formations.at(*row.formation_id).side != row.side)
          return fail(id);
        const auto x =
            __int128_t(row.area_dm->x) + __int128_t(parameters.length_m) * 5;
        const auto y =
            __int128_t(row.area_dm->y) + __int128_t(parameters.width_m) * 5;
        const auto size = __int128_t(parameters.cell_m) * 10;
        if (size <= 0 || x < 0 || y < 0 || x >= size * field.columns ||
            y >= size * field.rows)
          return fail(id);
        const auto index = std::size_t(y / size) * std::size_t(field.columns) +
                           std::size_t(x / size);
        if (index >= field.cells.size())
          return fail(id);
        success = session.councils[1 - row.side].intel.level < 3 &&
                  (field.cells[index].terrain == FieldTerrainKind::Forest ||
                   field.cells[index].terrain == FieldTerrainKind::Rough);
        if ((row.revealed_at && (!valid_time(*row.revealed_at) ||
                                 before(*row.revealed_at, *row.resolved_at) ||
                                 before(session.now, *row.revealed_at))) ||
            (!success && (!row.revealed_at ||
                          seconds(*row.revealed_at, *row.resolved_at) != 0)) ||
            (row.first_contact_consumed && (!success || !row.revealed_at)))
          return fail(id);
      } else if (row.kind == FieldStratagemKind::FeintStrength) {
        if (row.formation_id || row.area_dm || row.revealed_at ||
            row.first_contact_consumed)
          return fail(id);
        auto enemy = read_field_officer_statistics_before_council_adoption(
            common, sources, world, adoption->second,
            session.sides[1 - row.side].commander_actor_id);
        if (!enemy.ok)
          return {false, {}, enemy.error};
        success = session.councils[1 - row.side].intel.level < 2 &&
                  enemy.value.attributes.intellect <=
                      strategist.value.attributes.intellect;
        if (success) {
          if (feints[1 - row.side])
            return fail(id);
          feints[1 - row.side] = id;
        }
      } else
        return fail(id);
      if (*row.success != success)
        return fail(id);
    }
    Integer resolved_events = 0, revealed_events = 0;
    for (const auto &event : session.events)
      if (event.source_id == id &&
          (event.kind == field_event_kind::Stratagem ||
           event.kind == field_event_kind::Revealed)) {
        if (event.side != row.side || event.stratagem != row.kind ||
            event.success != row.success ||
            event.formation_id != row.formation_id ||
            event.target_formation_id != row.target_formation_id ||
            event.tactic || event.count)
          return fail(id);
        if (event.kind == field_event_kind::Stratagem) {
          if (!row.resolved_at || seconds(event.at, *row.resolved_at) != 0)
            return fail(id);
          ++resolved_events;
        } else {
          if (row.kind != FieldStratagemKind::Ambush ||
              !row.success.value_or(false) || !row.revealed_at ||
              seconds(event.at, *row.revealed_at) != 0)
            return fail(id);
          ++revealed_events;
        }
      }
    if (resolved_events != (row.resolved_at ? 1 : 0) ||
        revealed_events != ((row.kind == FieldStratagemKind::Ambush &&
                             row.success.value_or(false) && row.revealed_at)
                                ? 1
                                : 0))
      return fail(id);
  }
  // Every selected council item has exactly one installed original row;
  // accepting only forward rows would allow a deleted effect and refunded
  // point.
  for (const auto &[adoption_id, adoption] : world.council_facts) {
    if (adoption.battle_id != session.battle_id ||
        adoption.kind != FieldCouncilFactKind::Adopted)
      continue;
    if (!adoption.council.adopted_proposal_id)
      return fail(adoption_id);
    const auto selected = std::find_if(
        adoption.council.proposals.begin(), adoption.council.proposals.end(),
        [&](const auto &p) {
          return p.proposal_id == *adoption.council.adopted_proposal_id;
        });
    if (selected == adoption.council.proposals.end())
      return fail(adoption_id);
    for (const auto &item : selected->plan.stratagems) {
      if (item.voided_at_adoption ==
          field_council_stratagem_assignment_eligible(item, selected->plan))
        return fail(adoption_id);
      Integer matches = 0;
      for (const auto &[id, row] : session.stratagems)
        if (row.adopted_source.domain == "military" &&
            row.adopted_source.kind == "military_council" &&
            row.adopted_source.id == adoption_id && row.side == adoption.side &&
            (item.kind == FieldCouncilStratagemKind::Ambush
                 ? row.kind == FieldStratagemKind::Ambush
                 : row.kind == FieldStratagemKind::FeintStrength) &&
            item.formation_id == row.formation_id &&
            bool(item.area_dm) == bool(row.area_dm) &&
            (!item.area_dm || (item.area_dm->x == row.area_dm->x &&
                               item.area_dm->y == row.area_dm->y)))
          ++matches;
      if (matches != (item.voided_at_adoption ? 0 : 1))
        return fail(adoption_id);
    }
  }
  if (session.next_stratagem_sequence <= 0 ||
      Integer(sequences.size()) != session.next_stratagem_sequence - 1)
    return fail(session.battle_id);
  for (Integer side = 0; side < 2; ++side)
    if (session.sides[side].stratagem_points < 0 ||
        session.sides[side].initial_stratagem_points -
                session.sides[side].stratagem_points !=
            spent[side] ||
        session.sides[side].feint_strength_id != feints[side])
      return fail(session.battle_id);
  for (const auto &[formation_id, f] : session.formations) {
    std::set<std::pair<FieldStatusKind, StableId>> distinct_statuses;
    for (const auto &status : f.statuses)
      if (status.kind == FieldStatusKind::Hidden ||
          status.kind == FieldStatusKind::Confused) {
        if (!distinct_statuses.emplace(status.kind, status.source_id).second)
          return fail(status.source_id);
        const auto source = session.stratagems.find(status.source_id);
        if (source == session.stratagems.end() ||
            !source->second.success.value_or(false))
          return fail(status.source_id);
        const auto &row = source->second;
        if (status.kind == FieldStatusKind::Hidden) {
          if (row.kind != FieldStratagemKind::Ambush ||
              row.formation_id != formation_id || row.revealed_at ||
              status.until)
            return fail(status.source_id);
        } else {
          if (row.kind != FieldStratagemKind::Confuse ||
              row.target_formation_id != formation_id || !row.resolved_at ||
              !status.until)
            return fail(status.source_id);
          auto until =
              add_field_battle_seconds(*row.resolved_at, 600, parameters);
          if (!until.ok)
            return {false, {}, until.error};
          if (seconds(*status.until, until.value) != 0)
            return fail(status.source_id);
        }
      }
  }
  for (const auto &[id, row] : session.stratagems)
    if (row.kind == FieldStratagemKind::Ambush && row.success.value_or(false) &&
        !row.revealed_at) {
      Integer hidden = 0;
      for (const auto &status :
           session.formations.at(*row.formation_id).statuses)
        if (status.kind == FieldStatusKind::Hidden && status.source_id == id)
          ++hidden;
      if (hidden != 1)
        return fail(id);
    }
  return common_success();
}
} // namespace hanmo::resident
