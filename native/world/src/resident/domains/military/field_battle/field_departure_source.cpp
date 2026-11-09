#include "field_event_legacy_kinds.hpp"
#include "field_departure_source.hpp"
#include "field_binding_source.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {
Outcome<FieldDepartureSourceRead> read_field_actual_departure_source(
    const CommonStore &common, const FieldBattleWorldState &world,
    const StableId &battle_id, const StableId &formation_id) {
  auto fail = [&]() -> Outcome<FieldDepartureSourceRead> {
    return {false,
            {},
            common_failure("FieldDepartureOriginalSourceRequired", formation_id)
                .error};
  };
  auto s = world.sessions.find(battle_id);
  const auto *head = common.state().facts.find(battle_id);
  if (s == world.sessions.end() || !head || head->domain != "military" ||
      head->kind != "field_battle_session")
    return fail();
  const auto &session = s->second;
  auto f = session.formations.find(formation_id);
  if (f == session.formations.end() ||
      f->second.state != FieldFormationState::Withdrawn ||
      !f->second.withdrawn_at)
    return fail();
  const auto &formation = f->second;
  const auto at = *formation.withdrawn_at;
  if (std::tie(session.now.tick, session.now.t) < std::tie(at.tick, at.t) ||
      std::tie(at.tick, at.t) <
          std::tie(formation.joined_at.tick, formation.joined_at.t))
    return fail();
  auto binding = read_field_original_battle_binding(common, world, battle_id,
                                                    formation.army_id);
  if (!binding.ok)
    return {false, {}, binding.error};
  const FieldBattleEvent *event = nullptr;
  for (const auto &row : session.events)
    if (row.kind == field_event_kind::Withdrawn &&
        row.formation_id == formation_id) {
      if (event || row.sequence <= 0 ||
          row.sequence >= session.next_event_sequence ||
          row.at.tick != at.tick || row.at.t != at.t ||
          row.side != formation.side || row.target_formation_id ||
          row.source_id || row.count || row.tactic || row.stratagem ||
          row.success)
        return fail();
      event = &row;
    }
  if (!event)
    return fail();
  // 原Routing只释放普通兵；活将领即使位于该逃散出口仍由整军汇合源保留。
  bool routing_departure = false;
  for (const auto &row : session.events) {
    if (row.sequence >= event->sequence) break;
    if (row.formation_id != formation_id) continue;
    if (row.kind == field_event_kind::Rout) routing_departure = true;
    if (row.kind == field_event_kind::Rallied) routing_departure = false;
  }
  FieldDepartureSourceRead out{&session, &formation, binding.value, event, {}};
  std::set<StableId> named;
  const auto has = [](const auto &rows, const StableId &actor) {
    return std::find(rows.begin(), rows.end(), actor) != rows.end();
  };
  for (const auto &member : binding.value->members) {
    if (member.formation_id != formation_id)
      continue;
    if (member.actor_id.empty() || !common.actor(member.actor_id) ||
        !named.insert(member.actor_id).second)
      return fail();
    const bool original_officer =
        member.actor_id == formation.commander_actor_id ||
        formation.deputy_actor_id == member.actor_id;
    if (!original_officer && !has(formation.fit, member.actor_id) &&
        !has(formation.wounded, member.actor_id) &&
        !has(formation.dead, member.actor_id) &&
        !has(formation.captured, member.actor_id) &&
        !has(formation.deserted, member.actor_id))
      return fail();
    bool lost = false;
    for (const auto &row : formation.commander_casualties)
      if (row.actor_id == member.actor_id && row.loss == 2)
        lost = true;
    if (!(routing_departure && original_officer) &&
        !lost && !has(formation.dead, member.actor_id) &&
        !has(formation.captured, member.actor_id) &&
        !has(formation.deserted, member.actor_id))
      out.surviving_actor_ids.push_back(&member.actor_id);
  }
  // 原普通兵名册不等于Common人物集；仅真具名幸存者必须有原S1 binding。
  // 非人物兵员仍由原五类军事名册/整军manifest承担，不能为交接造一个Actor。
  for (const auto *rows : {&formation.fit, &formation.wounded})
    for (const auto &actor : *rows)
      if (common.actor(actor) && !named.contains(actor))
        return fail();
  std::sort(out.surviving_actor_ids.begin(), out.surviving_actor_ids.end(),
            [](const auto *a, const auto *b) { return *a < *b; });
  return {true, std::move(out), {}};
}
bool is_field_terminal_routing_departure(const FieldDepartureSourceRead &read) {
  if (!read.session || !read.formation || !read.withdrawal)
    return false;
  const auto &s = *read.session;
  const auto &f = *read.formation;
  const auto &withdrawn = *read.withdrawal;
  if (s.state != FieldBattleState::Ended || !s.result ||
      f.state != FieldFormationState::Withdrawn || !f.withdrawn_at ||
      withdrawn.at.tick != s.result->ended.tick ||
      withdrawn.at.t != s.result->ended.t)
    return false;
  for (const auto *members : {&f.fit, &f.wounded})
    for (const auto &actor : *members)
      if (actor != f.commander_actor_id && f.deputy_actor_id != actor) return false;
  bool routing = false, actual_withdrawal = false;
  const FieldBattleEvent *ended = nullptr;
  for (const auto &event : s.events) {
    if (&event == &withdrawn) {
      if (!routing)
        return false;
      actual_withdrawal = true;
    }
    if (event.kind == field_event_kind::Ended) {
      if (ended || !actual_withdrawal || event.sequence <= withdrawn.sequence ||
          event.at.tick != withdrawn.at.tick || event.at.t != withdrawn.at.t)
        return false;
      ended = &event;
    }
    if (event.sequence < withdrawn.sequence &&
        event.formation_id == f.formation_id) {
      if (event.kind == field_event_kind::Rout)
        routing = true;
      else if (event.kind == field_event_kind::Rallied)
        routing = false;
    }
  }
  return actual_withdrawal && ended;
}
} // namespace hanmo::resident
