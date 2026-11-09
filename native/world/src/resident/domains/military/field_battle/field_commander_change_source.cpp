#include "field_event_legacy_kinds.hpp"
#include "field_commander_change_source.hpp"
#include "field_segment_settlement.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <tuple>
namespace hanmo::resident {
namespace {
bool same(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick == b.tick && a.t == b.t;
}
bool before(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) < std::tie(b.tick, b.t);
}
} // namespace
Outcome<FieldCommanderChangeSourceRead>
read_field_commander_change_source(const CommonStore &common,
                                   const FieldBattleWorldState &world,
                                   const SourceRef &source) {
  auto fail = [&]() -> Outcome<FieldCommanderChangeSourceRead> {
    return {
        false,
        {},
        common_failure("FieldCommanderChangeSourceMismatch", source.id).error};
  };
  const auto *head = common.state().facts.find(source.id);
  if (source.domain != "military" || source.kind != "field_commander_changed" ||
      !head || head->kind != source.kind || head->domain != source.domain)
    return fail();
  const std::string_view prefix = "field_commander_changed:";
  if (std::string_view(source.id).substr(0, prefix.size()) != prefix)
    return fail();
  Integer serial = 0;
  const auto number = std::string_view(source.id).substr(prefix.size());
  auto parsed =
      std::from_chars(number.data(), number.data() + number.size(), serial);
  if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
      serial <= 0 || serial >= common.state().next_instance_id ||
      number != std::to_string(serial))
    return fail();
  FieldCommanderChangeSourceRead out;
  for (const auto &[battle, session] : world.sessions)
    for (const auto &[id, f] : session.formations) {
      if (!f.ability)
        continue;
      for (const auto &change : f.ability->commander_changes) {
        if (change.source_ref.id != source.id)
          continue;
        if (out.change || change.source_ref.domain != source.domain ||
            change.source_ref.kind != source.kind || !f.commander_lost_at ||
            !same(change.at, *f.commander_lost_at) ||
            change.previous_actor_id != f.commander_actor_id ||
            !f.deputy_actor_id ||
            change.commander_actor_id != *f.deputy_actor_id ||
            change.previous_actor_id == change.commander_actor_id ||
            change.observed_tick < session.created_tick ||
            change.observed_tick > common.tick() ||
            change.applied_at.half_day_tick != change.observed_tick ||
            change.lost_event_sequence <= 0 ||
            !common.actor(change.commander_actor_id))
          return fail();
        auto actual_moment =
            compare_world_moments(change.applied_at, change.applied_at);
        if (!actual_moment.ok)
          return {false, {}, actual_moment.error};
        Integer losses = 0;
        for (const auto &loss : f.commander_casualties)
          if (loss.actor_id == change.previous_actor_id && loss.loss == 2 &&
              same(loss.at, change.at))
            ++losses;
        if (losses != 1)
          return fail();
        const FieldBattleEvent *event = nullptr;
        for (const auto &e : session.events)
          if (e.sequence == change.lost_event_sequence) {
            if (event || e.kind != field_event_kind::CommanderLost ||
                e.formation_id != id || e.side != f.side || e.count != 2 ||
                !same(e.at, change.at) || e.source_id ||
                e.target_formation_id || e.tactic || e.stratagem || e.success)
              return fail();
            event = &e;
          }
        if (!event)
          return fail();
        auto prior =
            read_actual_common_fact_position(common, f.ability->source_ref);
        auto here = read_actual_common_fact_position(common, source);
        if (!prior.ok)
          return {false, {}, prior.error};
        if (!here.ok)
          return {false, {}, here.error};
        if (prior.value >= here.value)
          return fail();
        out = {&session, &f, &change, event};
      }
    }
  if (!out.change)
    return fail();
  return {true, out, {}};
}
Outcome<std::optional<SourceRef>>
record_field_commander_change_source_in_candidate(
    CommonStore &common, const MilitaryState &military,
    FieldBattleWorldState &world, const StableId &battle_id,
    const StableId &formation_id, const FieldBattleTime &at,
    const FieldSegmentApplicationRead &clock, CheckpointMemoryCharge &owner) {
  auto fail = [&](const char *code) -> Outcome<std::optional<SourceRef>> {
    return {false, {}, common_failure(code, formation_id).error};
  };
  if (!clock.bound_to(common, military, world))
    return fail("FieldCommanderChangeWorldMismatch");
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  if (now.value.half_day_tick != common.tick())
    return fail("FieldCommanderChangeMomentMismatch");
  auto session_it = world.sessions.find(battle_id);
  if (session_it == world.sessions.end())
    return fail("FieldBattleNotFound");
  auto &session = session_it->second;
  auto fi = session.formations.find(formation_id);
  if (session.state != FieldBattleState::Running ||
      fi == session.formations.end())
    return fail("FieldCommanderChangeBoundaryRequired");
  auto &f = fi->second;
  if (!f.ability || !f.commander_lost_at || !same(*f.commander_lost_at, at))
    return fail("FieldCommanderChangeBoundaryRequired");
  if (!f.deputy_actor_id)
    return {true, {}, {}};
  if (!f.ability->commander_changes.empty()) {
    if (f.ability->commander_changes.size() != 1)
      return fail("FieldCommanderChangeRepeated");
    const auto &prior = f.ability->commander_changes.front();
    auto checked =
        read_field_commander_change_source(common, world, prior.source_ref);
    if (!checked.ok)
      return {false, {}, checked.error};
    if (checked.value.formation != &f)
      return fail("FieldCommanderChangeRepeated");
    return {true, prior.source_ref, {}};
  }
  const auto &deputy = *f.deputy_actor_id;
  const auto contains = [&](const auto &group) {
    return std::find(group.begin(), group.end(), deputy) != group.end();
  };
  const auto *actor = common.actor(deputy);
  if (!actor || !actor->alive || !actor->available || contains(f.dead) ||
      contains(f.captured) || contains(f.deserted) ||
      (!contains(f.fit) && !contains(f.wounded)))
    return {true, {}, {}};
  for (const auto &loss : f.commander_casualties)
    if (loss.actor_id == deputy && loss.loss == 2 && !before(at, loss.at))
      return {true, {}, {}};
  const FieldBattleEvent *lost = nullptr;
  for (const auto &e : session.events)
    if (e.kind == field_event_kind::CommanderLost &&
        e.formation_id == formation_id && same(e.at, at)) {
      if (lost || e.side != f.side || e.count != 2)
        return fail("FieldCommanderLostEventRequired");
      lost = &e;
    }
  if (!lost || f.ability->commander_actor_id != f.commander_actor_id)
    return fail("FieldCommanderLostEventRequired");
  if (common.state().next_instance_id <= 0 ||
      common.state().next_instance_id == std::numeric_limits<Integer>::max())
    return fail("FieldCommanderChangeSequenceExhausted");
  char digits[32];
  auto decimal = std::to_chars(digits, digits + sizeof(digits),
                               common.state().next_instance_id);
  if (decimal.ec != std::errc{})
    return fail("FieldCommanderChangeSequenceExhausted");
  const ProducedStableIdBuildView made{
      "field_commander_changed",
      std::string_view(digits, std::size_t(decimal.ptr - digits))};


  (owner).grow(0);
  auto id = common.next_id("field_commander_changed");
  if (!id.ok)
    return {false, {}, id.error};
  auto added =
      common.append_fact({id.value, "field_commander_changed", "military"});
  if (!added.ok)
    return {false, {}, added.error};
  FieldCommanderChange change{{"military", "field_commander_changed", id.value},
                              f.commander_actor_id,
                              deputy,
                              at,
                              now.value.half_day_tick,
                              lost->sequence,
                              now.value};
  f.ability->commander_changes.push_back(std::move(change));
  const auto &source = f.ability->commander_changes.back().source_ref;
  auto checked = read_field_commander_change_source(common, world, source);
  if (!checked.ok)
    return {false, {}, checked.error};
  return {true, source, {}};
}
} // namespace hanmo::resident
