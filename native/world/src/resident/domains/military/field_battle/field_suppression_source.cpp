#include "field_event_legacy_kinds.hpp"
#include "field_event_mission_transition.hpp"
#include "resident/model/common_model.hpp"
#include "field_suppression_source.hpp"
#include "field_generation.hpp"
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
Status validate_field_suppression_sources(const FieldBattleSession &session,
                                          const FieldBattleParameters &p) {
  auto fail = [&](const StableId &id) {
    return common_failure("FieldVolleyImpactSourceMismatch", id);
  };
  if (p.pulse_s <= 0 || p.window_s <= 0)
    return fail(session.battle_id);
  using Moment = std::pair<Integer, Integer>;
  using Hit = std::tuple<StableId, StableId, Integer, Integer>;
  std::set<Hit> unique;
  std::map<StableId, std::set<Moment>> used_pulses;
  std::map<std::pair<StableId, StableId>, FieldBattleTime> latest_until;
  for (const auto &event : session.events) {
    if (event.kind != field_event_kind::VolleyImpact)
      continue;
    if (!event.formation_id || !event.target_formation_id || !event.source_id ||
        !event.side || event.tactic != FieldTacticKind::Volley || event.count ||
        event.stratagem || event.success || field_event_mission_transition(event))
      return fail(session.battle_id);
    auto attacker = session.formations.find(*event.formation_id);
    auto target = session.formations.find(*event.target_formation_id);
    if (attacker == session.formations.end() ||
        target == session.formations.end() ||
        attacker->second.side == target->second.side ||
        attacker->second.side != *event.side || !attacker->second.ability ||
        std::tie(event.at.tick, event.at.t) <
            std::tie(target->second.joined_at.tick, target->second.joined_at.t))
      return fail(*event.source_id);
    const FieldTacticActivation *actual = nullptr;
    for (const auto &activation : attacker->second.ability->active)
      if (activation.activation_id == *event.source_id) {
        if (actual)
          return fail(*event.source_id);
        actual = &activation;
      }
    if (!actual || actual->kind != FieldTacticKind::Volley ||
        actual->accepted_source.kind != FieldTacticKind::Volley ||
        std::tie(event.at.tick, event.at.t) <=
            std::tie(actual->started.tick, actual->started.t) ||
        event.at.t < 0 || event.at.t >= p.window_s ||
        ((__int128_t(event.at.tick) * p.window_s + event.at.t) % p.pulse_s) !=
            0 ||
        std::tie(event.at.tick, event.at.t) >
            std::tie(session.now.tick, session.now.t) ||
        !unique
             .emplace(*event.source_id, *event.target_formation_id,
                      event.at.tick, event.at.t)
             .second)
      return fail(*event.source_id);
    used_pulses[*event.source_id].emplace(event.at.tick, event.at.t);
    auto until = add_field_battle_seconds(event.at, 600, p);
    if (!until.ok)
      return {false, {}, until.error};
    auto key = std::make_pair(*event.target_formation_id, *event.source_id);
    auto found = latest_until.find(key);
    if (found == latest_until.end() ||
        std::tie(found->second.tick, found->second.t) <
            std::tie(until.value.tick, until.value.t))
      latest_until[key] = until.value;
  }
  for (const auto &[id, f] : session.formations) {
    if (f.ability)
      for (const auto &activation : f.ability->active)
        if (activation.kind == FieldTacticKind::Volley) {
          const auto found = used_pulses.find(activation.activation_id);
          const auto used =
              found == used_pulses.end() ? 0 : found->second.size();
          if (used > 3 || activation.remaining_pulses != Integer(3 - used))
            return fail(activation.activation_id);
        }
    for (const auto &status : f.statuses)
      if (status.kind == FieldStatusKind::Suppressed) {
        auto found = latest_until.find({id, status.source_id});
        if (found == latest_until.end() || !status.until ||
            std::tie(status.until->tick, status.until->t) !=
                std::tie(found->second.tick, found->second.t))
          return fail(status.source_id);
        latest_until.erase(found);
      }
  }
  // Expired statuses remain original historical entries; their time predicate
  // controls effect, not source deletion. Every real impact has exactly one.
  if (!latest_until.empty())
    return fail(latest_until.begin()->first.second);
  return common_success();
}
} // namespace hanmo::resident
