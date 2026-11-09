// 世界时钟不变；仅把已有接触有理时刻投影到本半日的会战步边界。
#pragma once
#include "field_generation.hpp"
namespace hanmo::resident {
struct ContactHit;
struct FieldBattleTime {
  Integer tick = 0, t = 0;
};
// 接触秒数可等于窗口长；这是数学投影，不是可保存的 BattleTime。
Outcome<Integer> field_contact_step_seconds(const ContactHit &,
                                            const FieldBattleParameters &);
// 仅复用原 WorldMoment 的商余归一化数学；不引入全世界子半日状态。
// 原 source_tick 所在窗末精确接触归入下一窗零秒，空尾窗不算首段。
Outcome<FieldBattleTime>
field_contact_battle_time(Integer source_tick, const ContactHit &,
                          const FieldBattleParameters &);
// 命令到达可越过局部窗口；返回半开坐标，溢出明确失败，不钳到本窗末。
Outcome<FieldBattleTime>
add_field_battle_seconds(const FieldBattleTime &, Integer seconds,
                         const FieldBattleParameters &);
// Advance 的实际局部窗口切片；不写时钟，只有真正开始新步才开放下一段。
struct FieldBattleSession;
struct FieldBattleWindowSlice {
  FieldBattleTime from, to;
  Integer segment = 0, steps = 0;
  bool opens_next_segment = false, closes_window = false;
};
// committed_tick 必须来自同一 Common；未完成宏观提交的下一窗口明确拒绝。
Outcome<FieldBattleWindowSlice> read_field_battle_window_slice(
    const FieldBattleSession &, Integer committed_tick,
    const FieldBattleTime &requested_to, const FieldBattleParameters &);
} // namespace hanmo::resident
