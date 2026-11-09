// 原 BW06 每分钟脉冲：只改同候选会话名单、余数与原 RNG，S4 真落账仍在段
// producer。
#pragma once
#include "field_contacts.hpp"
namespace hanmo::resident {
struct MilitaryBattleCatalog;
class CheckpointMemoryCharge;
struct FieldPulseLoss {
  StableId formation_id;
  Integer fit_before = 0, wounded = 0, dead = 0;
};
struct FieldPulseImpact {
  StableId attacker_id, target_id;
  Integer morale_shock = 0, flank = 0;
  bool suppressed = false;
  std::optional<StableId> charge_activation, volley_activation, ambush_source;
};
struct FieldPulseRead {
  bool applied = false;
  std::vector<FieldPulseLoss> losses;
  std::vector<FieldPulseImpact> impacts;
};
// 此叶不代办士气、溃退、将领、事件或段提交；完整 driver 必须依原次序接其出口。
Outcome<FieldPulseRead> apply_field_casualty_pulse_in_candidate(
    FieldBattleSession &, const GeneratedBattleField &,
    const MilitaryBattleCatalog &, const FieldBattleWindowSlice &,
    CheckpointMemoryCharge &actual_world_owner);
} // namespace hanmo::resident
