#include "field_event_legacy_kinds.hpp"
#include "resident/model/common_model.hpp"
#include "field_local_event_sources.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {
namespace {
bool same(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick == b.tick && a.t == b.t;
}
bool before(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) < std::tie(b.tick, b.t);
}
bool no_payload(const FieldBattleEvent &e) {
  return !e.target_formation_id && !e.source_id && !e.tactic && !e.stratagem &&
         !e.success;
}
} // namespace
Status validate_field_local_event_sources(const FieldBattleSession &session,
                                          const FieldBattleParameters &p) {
  auto fail = [&](const StableId &id) {
    return common_failure("FieldLocalEventSourceMismatch", id);
  };
  if (p.step_s <= 0 || p.window_s <= 0)
    return fail(session.battle_id);
  Integer terminal_events = 0;
  for (const auto &e : session.events) {
    if (e.kind != field_event_kind::Contact &&
        e.kind != field_event_kind::CommanderLost &&
        e.kind != field_event_kind::Ended)
      continue;
    if (!session.started_at || before(e.at, *session.started_at) ||
        before(session.now, e.at) || e.at.t < 0 || e.at.t >= p.window_s ||
        e.at.t % p.step_s || !no_payload(e))
      return fail(session.battle_id);
    if (e.kind == field_event_kind::Ended) {
      if (++terminal_events != 1 || e.formation_id || e.count ||
          session.state != FieldBattleState::Ended || !session.result ||
          !same(e.at, session.result->ended) ||
          (session.result->winner_side < 0
               ? bool(e.side)
               : e.side != session.result->winner_side))
        return fail(session.battle_id);
      continue;
    }
    if (!e.formation_id)
      return fail(session.battle_id);
    const auto f = session.formations.find(*e.formation_id);
    if (f == session.formations.end() || e.side != f->second.side ||
        before(e.at, f->second.joined_at))
      return fail(*e.formation_id);
    if (e.kind == field_event_kind::Contact) {
      if (e.count)
        return fail(*e.formation_id);
    } else {
      if (e.count != 2 || std::count_if(f->second.commander_casualties.begin(),
                                        f->second.commander_casualties.end(),
                                        [&](const auto &loss) {
                                          return loss.loss == 2 &&
                                                 same(loss.at, e.at);
                                        }) != 1)
        return fail(*e.formation_id);
    }
  }
  if ((session.state == FieldBattleState::Ended) != (terminal_events == 1))
    return fail(session.battle_id);
  for (const auto &[id, f] : session.formations) {
    const auto contacts = std::count_if(
        session.events.begin(), session.events.end(), [&](const auto &e) {
          return e.kind == field_event_kind::Contact &&
                 e.formation_id == id;
        });
    if (contacts > 1 || f.has_engaged != (contacts == 1))
      return fail(id);
    const FieldBattleEvent *withdrawn = nullptr;
    for (const auto &e : session.events)
      if (e.kind == field_event_kind::Withdrawn && e.formation_id == id) {
        if (withdrawn)
          return fail(id);
        withdrawn = &e;
      }
    if (bool(withdrawn) != bool(f.withdrawn_at) ||
        (withdrawn && (!same(withdrawn->at, *f.withdrawn_at) ||
                       f.state != FieldFormationState::Withdrawn)))
      return fail(id);
    const FieldCommanderCasualty *primary_death = nullptr;
    for (std::size_t i = 0; i < f.commander_casualties.size(); ++i) {
      const auto &loss = f.commander_casualties[i];
      if ((loss.actor_id != f.commander_actor_id &&
           f.deputy_actor_id != loss.actor_id) ||
          (loss.loss != 1 && loss.loss != 2) || before(loss.at, f.joined_at) ||
          before(session.now, loss.at) || loss.at.t < 0 ||
          loss.at.t >= p.window_s || loss.at.t % p.step_s)
        return fail(id);
      if (i && before(loss.at, f.commander_casualties[i - 1].at))
        return fail(id);
      Integer previous = 0;
      for (std::size_t j = 0; j < i; ++j)
        if (f.commander_casualties[j].actor_id == loss.actor_id)
          previous = std::max(previous, f.commander_casualties[j].loss);
      if (previous >= loss.loss)
        return fail(id);
      if (loss.actor_id != f.commander_actor_id) {
        if (!primary_death || !before(primary_death->at, loss.at))
          return fail(id);
      }
      if (loss.loss == 2) {
        if (std::count_if(session.events.begin(), session.events.end(),
                          [&](const auto &e) {
                            return e.kind ==
                                       field_event_kind::CommanderLost &&
                                   e.formation_id == id && same(e.at, loss.at);
                          }) != 1)
          return fail(id);
        if (loss.actor_id == f.commander_actor_id)
          primary_death = &loss;
      }
    }
    if (bool(primary_death) != bool(f.commander_lost_at) ||
        (primary_death && !same(primary_death->at, *f.commander_lost_at)))
      return fail(id);
  }
  return common_success();
}
} // namespace hanmo::resident
