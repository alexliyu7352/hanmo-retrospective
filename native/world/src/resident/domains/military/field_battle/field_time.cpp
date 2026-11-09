// 只读原 ContactHit 分子分母；不会移动世界 tick 或写入另一种全局时刻。
#include "field_time.hpp"
#include "field_session.hpp"
#include "resident/indexes/contact_model.hpp"
#include <limits>
namespace hanmo::resident {
Outcome<Integer> field_contact_step_seconds(const ContactHit &contact,
                                            const FieldBattleParameters &p) {
  if (contact.time_den <= 0 || contact.time_num < 0 ||
      contact.time_num > contact.time_den || p.window_s <= 0 || p.step_s <= 0 ||
      p.window_s % p.step_s)
    return {false, {}, {{"InvalidFieldBattleContactTime", "contact/time"}}};
  const auto seconds =
      static_cast<Integer>(static_cast<__int128_t>(contact.time_num) *
                           p.window_s / contact.time_den);
  return {true, seconds / p.step_s * p.step_s, {}};
}
// 商余与原 normalize_world_moment 完全同义，只处理合同里的整数战场秒。
Outcome<FieldBattleTime>
add_field_battle_seconds(const FieldBattleTime &at, Integer seconds,
                         const FieldBattleParameters &p) {
  if (at.tick < 0 || at.t < 0 || p.window_s <= 0 || at.t >= p.window_s ||
      seconds < 0)
    return {false, {}, common_failure("InvalidFieldBattleTime").error};
  const auto total = static_cast<__int128_t>(at.t) + seconds;
  const auto tick = static_cast<__int128_t>(at.tick) + total / p.window_s;
  if (tick > std::numeric_limits<Integer>::max())
    return {false, {}, common_failure("FieldBattleTimeOverflow").error};
  return {
      true,
      {static_cast<Integer>(tick), static_cast<Integer>(total % p.window_s)},
      {}};
}
Outcome<FieldBattleTime>
field_contact_battle_time(Integer source_tick, const ContactHit &contact,
                          const FieldBattleParameters &p) {
  auto seconds = field_contact_step_seconds(contact, p);
  if (!seconds.ok)
    return {false, {}, seconds.error};
  return add_field_battle_seconds({source_tick, 0}, seconds.value, p);
}
Outcome<FieldBattleWindowSlice> read_field_battle_window_slice(
    const FieldBattleSession &session, Integer committed_tick,
    const FieldBattleTime &to, const FieldBattleParameters &p) {
  auto fail = [&](const char *code) -> Outcome<FieldBattleWindowSlice> {
    return {false, {}, common_failure(code, session.battle_id).error};
  };
  if (session.state != FieldBattleState::Running)
    return fail("FieldBattleNotRunning");
  if (p.window_s <= 0 || p.step_s <= 0 || p.window_s % p.step_s ||
      p.max_segments <= 0 || session.segment < 0 ||
      session.segment >= p.max_segments || session.now.tick < 0 ||
      session.now.t < 0 || session.now.t >= p.window_s ||
      session.now.t % p.step_s || committed_tick < 0 ||
      (session.window_complete && session.now.t != 0))
    return fail("InvalidFieldBattleTime");
  if (session.now.tick == committed_tick)
    return fail("FieldBattleWindowNotOpen");
  if (session.now.tick == std::numeric_limits<Integer>::max() ||
      session.now.tick + 1 != committed_tick)
    return fail("FieldBattleWindowMismatch");
  if (to.tick < session.now.tick || to.tick > committed_tick || to.t < 0 ||
      to.t >= p.window_s || to.t % p.step_s ||
      (to.tick == session.now.tick && to.t < session.now.t) ||
      (to.tick == committed_tick && to.t != 0))
    return fail("FieldBattleAdvanceOutsideWindow");
  const Integer seconds = to.tick == session.now.tick
                              ? to.t - session.now.t
                              : p.window_s - session.now.t;
  FieldBattleWindowSlice slice{session.now,     to,
                               session.segment, seconds / p.step_s,
                               false,           to.tick == committed_tick};
  if (session.window_complete && slice.steps != 0) {
    if (session.segment + 1 >= p.max_segments)
      return fail("FieldBattleSegmentLimit");
    slice.segment = session.segment + 1;
    slice.opens_next_segment = true;
  }
  return {true, slice, {}};
}
} // namespace hanmo::resident
