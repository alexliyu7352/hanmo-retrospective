#include "field_event_legacy_kinds.hpp"
#include "field_departures.hpp"
#include "field_army_exit_selection.hpp"
#include "field_routing.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
#include <array>
#include <limits>
namespace hanmo::resident {
namespace {
bool active(const FieldFormation &f) {
  return !f.fit.empty() && f.state != FieldFormationState::Withdrawn &&
         f.state != FieldFormationState::Destroyed;
}
} // namespace
Outcome<FieldDepartureRead> apply_field_departures_in_candidate(
    const FieldBattleWorldState &world, FieldBattleSession &session,
    const GeneratedBattleField &field,
    const FieldBattleWindowSlice &step, CheckpointMemoryCharge &owner) {
  auto fail = [](const char *code) -> Outcome<FieldDepartureRead> {
    return {false, {}, common_failure(code).error};
  };
  if (session.state != FieldBattleState::Running || session.result ||
      step.steps != 1 || session.now.tick != step.from.tick ||
      session.now.t != step.from.t || session.field_hash != field.field_hash ||
      session.segment != step.segment)
    return fail("FieldDepartureBoundaryRequired");
  const auto cell_dm = __int128_t(field.parameters.cell_m) * 10;
  if (cell_dm <= 0 || field.columns <= 0 || field.rows <= 0)
    return fail("FieldDepartureGeometryRequired");
  FieldDepartureRead out;
  // Derive exits from actual position and the already selected original cells,
  // never trust an externally supplied list of claimed arrivals.
  for (const auto &[id, f] : session.formations) {
    if (!active(f))
      continue;
    if (f.side < 0 || f.side > 1)
      return fail("FieldDepartureSideInvalid");
    if (f.state != FieldFormationState::Routing &&
        !std::holds_alternative<FieldWithdraw>(f.mission))
      continue;
    const auto x =
        __int128_t(f.position_dm.x) + __int128_t(field.parameters.length_m) * 5;
    const auto y =
        __int128_t(f.position_dm.y) + __int128_t(field.parameters.width_m) * 5;
    if (x < 0 || y < 0 || x >= cell_dm * field.columns ||
        y >= cell_dm * field.rows)
      return fail("FieldDeparturePositionInvalid");
    const auto index = std::size_t(y / cell_dm) * std::size_t(field.columns) +
                       std::size_t(x / cell_dm);
    if (index >= field.cells.size())
      return fail("FieldDepartureGeometryRequired");
    if (f.state != FieldFormationState::Routing) {
      auto locked = read_field_locked_army_exit_target(world, session, f, field,
                                                       step.from, session.next_event_sequence - 1);
      if (!locked.ok) return {false, {}, locked.error};
      if (f.position_dm.x != locked.value.x || f.position_dm.y != locked.value.y) continue;
    }
    if (std::find(session.sides[f.side].exits.begin(),
                  session.sides[f.side].exits.end(),
                  index) != session.sides[f.side].exits.end())
      out.exited_formations.push_back(id);
  }
  std::array<bool, 2> survivors{};
  for (const auto &[id, f] : session.formations)
    if (active(f) &&
        std::find(out.exited_formations.begin(), out.exited_formations.end(),
                  id) == out.exited_formations.end()) {
      if (f.side < 0 || f.side > 1)
        return fail("FieldDepartureSideInvalid");
      survivors[f.side] = true;
    }
  const bool timeout = step.closes_window &&
                       session.segment + 1 == field.parameters.max_segments;
  out.terminal = !survivors[0] || !survivors[1] || timeout;
  const char *reason =
      !survivors[0] && !survivors[1]     ? "FieldBothSidesUnavailable"
      : (!survivors[0] || !survivors[1]) ? "FieldSideDefeated"
                                         : "FieldMaximumSegments";
  const Integer winner =
      survivors[0] == survivors[1] ? -1 : (survivors[0] ? 0 : 1);
  // 10-31整军汇合：到时终局不等于已在出口。未到口的有序编队保实际位置，
  // 由独立post-result来源继续走，不能把timeout当一个已完成的世界退出。
  // 總設計師 2026-10-29 Slack 批准，1793335838.005349：
  // 终局仍 Routing 的成员从编队实际终局 position_dm 离场，不移到出口。
  // 先登记离场，再沿原逃散归约和统一 Withdrawn 事件；不改变胜负判断。
  if (out.terminal)
    for (const auto &[id, f] : session.formations)
      if (f.state == FieldFormationState::Routing &&
          std::find(out.exited_formations.begin(), out.exited_formations.end(),
                    id) == out.exited_formations.end())
        out.exited_formations.push_back(id);
  std::sort(out.exited_formations.begin(), out.exited_formations.end());


  for (const auto &[id, f] : session.formations)
    if (f.state == FieldFormationState::Routing &&
        (out.terminal || std::binary_search(out.exited_formations.begin(),
                                            out.exited_formations.end(), id))) {


    }
  (owner).grow(0);
  for (auto &[id, f] : session.formations)
    if (f.state == FieldFormationState::Routing &&
        (out.terminal || std::binary_search(out.exited_formations.begin(),
                                            out.exited_formations.end(), id))) {
      auto settled = settle_field_routing_deserters_at_exit(f);
      if (!settled.ok)
        return {false, {}, settled.error};
    }
  for (const auto &id : out.exited_formations) {
    auto &f = session.formations.at(id);
    f.state = FieldFormationState::Withdrawn;
    f.withdrawn_at = step.to;
    f.navigation_target_dm.reset();
    f.navigation_observation_sequence.reset();
    f.navigation_replanned_at.reset();
    FieldBattleEvent e;
    e.at = step.to;
    e.kind = field_event_kind::Withdrawn;
    e.formation_id = id;
    e.side = f.side;
    if (session.next_event_sequence <= 0 ||
        session.next_event_sequence == std::numeric_limits<Integer>::max())
      return fail("FieldEventSequenceExhausted");
    e.sequence = session.next_event_sequence++;
    session.events.push_back(std::move(e));
  }
  if (out.terminal) {
    session.state = FieldBattleState::Ended;
    // §4.4 invalidates outstanding arrivals at the actual terminal step.
    // Original boundary/queue/admission bodies remain retained by their owner;
    // clearing an active due reference neither releases S1 hold nor resumes it.
    session.pending_entries.clear();
    session.result = FieldBattleResult{winner, reason, step.to};
    FieldBattleEvent e;
    e.at = step.to;
    e.kind = field_event_kind::Ended;
    if (winner >= 0)
      e.side = winner;
    if (session.next_event_sequence <= 0 ||
        session.next_event_sequence == std::numeric_limits<Integer>::max())
      return fail("FieldEventSequenceExhausted");
    e.sequence = session.next_event_sequence++;
    session.events.push_back(std::move(e));
  }
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
