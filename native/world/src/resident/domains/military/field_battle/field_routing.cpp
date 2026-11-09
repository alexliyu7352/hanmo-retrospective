#include "field_event_legacy_kinds.hpp"
#include "resident/model/common_model.hpp"
#include "field_routing.hpp"
#include "field_integer_geometry.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
bool present(const FieldFormation &f) {
  return !f.fit.empty() && f.state != FieldFormationState::Withdrawn &&
         f.state != FieldFormationState::Destroyed &&
         f.state != FieldFormationState::Deploying;
}
void trigger(FieldAiState &state) {
  if (std::find(state.pending_triggers.begin(), state.pending_triggers.end(),
                FieldAiTrigger::Rout) == state.pending_triggers.end()) {
    state.pending_triggers.push_back(FieldAiTrigger::Rout);
    std::sort(state.pending_triggers.begin(), state.pending_triggers.end());
  }
}
} // namespace
Outcome<FieldRoutingRead> apply_field_routing_in_candidate(
    FieldBattleSession &session, const GeneratedBattleField &field,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner) {
  auto fail = [](const char *code) -> Outcome<FieldRoutingRead> {
    return {false, {}, common_failure(code).error};
  };
  if (session.state != FieldBattleState::Running ||
      session.field_hash != field.field_hash ||
      session.now.tick != step.from.tick || session.now.t != step.from.t ||
      step.steps != 1)
    return fail("FieldRoutingBoundaryRequired");
  FieldRoutingRead out;
  bool any = false;
  for (const auto &[id, f] : session.formations)
    if (present(f) && f.state != FieldFormationState::Routing && f.morale <= 0)
      any = true;
  if (!any)
    return {true, std::move(out), {}};
  // 分配前由原候选 owner 保活名册扩容和真实事件；不把临时 vector 的 lease
  // 冒世界拥有者。


  const StableId *largest_army = nullptr;
  for (const auto &[id, f] : session.formations)
    if (!largest_army || f.army_id.size() > largest_army->size())
      largest_army = &f.army_id;
  for (const auto &[id, f] : session.formations) {
    const auto maximum = f.fit.size() + f.wounded.size() + f.captured.size();


  }
  (owner).grow(0);
  std::set<StableId> started;
  while (true) {
    std::vector<StableId> wave;
    for (const auto &[id, f] : session.formations)
      if (present(f) && f.state != FieldFormationState::Routing &&
          f.morale <= 0 && !started.contains(id))
        wave.push_back(id);
    if (wave.empty())
      break;
    // 同批先标 Routing，互溃者不能互造俘虏。
    for (const auto &id : wave) {
      auto &formation = session.formations.at(id);
      formation.state = FieldFormationState::Routing;
      formation.navigation_target_dm.reset();
      formation.navigation_observation_sequence.reset();
      formation.navigation_replanned_at.reset();
      formation.hold_since.reset();
      started.insert(id);
    }
    for (const auto &id : wave) {
      auto &f = session.formations.at(id);
      if (f.side < 0 || f.side > 1)
        return fail("FieldRoutingSideInvalid");
      Integer adjacent = 0;
      const FieldFormation *captor = nullptr;
      for (const auto &[enemy_id, enemy] : session.formations)
        if (enemy.side != f.side && present(enemy) &&
            enemy.state != FieldFormationState::Routing && enemy.morale > 0 &&
            field_distance_squared(f.position_dm, enemy.position_dm) <=
                __int128_t(300) * 300) {
          ++adjacent;
          if (!captor)
            captor = &enemy;
        }
      auto fit = f.fit;
      std::sort(fit.begin(), fit.end());
      const Integer captured = adjacent >= 2   ? Integer(fit.size())
                               : adjacent == 1 ? Integer(fit.size()) / 2
                                               : 0;
      for (Integer n = 0; n < captured; ++n) {
        f.fit.erase(std::find(f.fit.begin(), f.fit.end(), fit[n]));
        f.captured.push_back(fit[n]);
        f.captors.emplace_back(fit[n], captor->army_id);
      }
      const Integer wounded = adjacent >= 2 ? Integer(f.wounded.size()) : 0;
      if (adjacent >= 2) {
        for (const auto &actor : f.wounded) {
          f.captured.push_back(actor);
          f.captors.emplace_back(actor, captor->army_id);
        }
        f.wounded.clear();
      }
      f.hold_since.reset();
      f.navigation_target_dm.reset();
      f.navigation_observation_sequence.reset();
      f.navigation_replanned_at.reset();
      if (f.fit.empty())
        f.state = FieldFormationState::Destroyed;
      if (session.next_event_sequence == std::numeric_limits<Integer>::max())
        return fail("FieldEventSequenceOverflow");
      FieldBattleEvent event;
      event.sequence = session.next_event_sequence++;
      event.at = step.to;
      event.kind = field_event_kind::Rout;
      event.formation_id = id;
      event.side = f.side;
      event.count = captured + wounded;
      session.events.push_back(std::move(event));
      trigger(session.ai[f.side]);
      trigger(session.ai[1 - f.side]);
      out.started.push_back(id);
      for (auto &[ally_id, ally] : session.formations)
        if (ally_id != id && ally.side == f.side && present(ally) &&
            ally.state != FieldFormationState::Routing &&
            field_distance_squared(f.position_dm, ally.position_dm) <=
                __int128_t(3000) * 3000)
          ally.morale = std::max<Integer>(0, ally.morale - 10);
    }
  }
  return {true, std::move(out), {}};
}
Status settle_field_routing_deserters_at_exit(FieldFormation &f) {
  if (f.state != FieldFormationState::Routing)
    return common_success();
  // 10-31修订：活主将/副将保军籍。兼任兵员也不能按普通兵逃散，
  // 独立将领本来不在五类兵员中；其后真正汇合另沿Army exit来源。
  const auto retained_officer = [&](const StableId &actor) {
    return actor == f.commander_actor_id || f.deputy_actor_id == actor;
  };
  for (auto *members : {&f.fit, &f.wounded}) {
    for (const auto &actor : *members)
      if (!retained_officer(actor)) f.deserted.push_back(actor);
    members->erase(std::remove_if(members->begin(), members->end(),
        [&](const StableId &actor) { return !retained_officer(actor); }), members->end());
  }
  return common_success();
}
} // namespace hanmo::resident
