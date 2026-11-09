// 原 §7 接战报价；不抽随机、不写世界时钟，也不替代后续真实伤亡 writer。
#pragma once
#include "field_session.hpp"
#include <map>
namespace hanmo::resident {
struct FieldAttackContact {
  StableId attacker_id, target_id;
  bool ranged = false;
  Integer facing = 0, cover = 0;
  bool height_advantage = false, target_forest = false, target_hill = false,
       attacker_forest = false;
};
struct FieldContactRead {
  std::vector<StableId> melee_formations;
  std::vector<FieldAttackContact> attacks;
};
// 按攻击方、目标原 formation_id 排序；溃退可被攻击但不产生攻击。
Outcome<FieldContactRead>
read_field_attack_contacts(const FieldBattleSession &,
                           const GeneratedBattleField &);
struct FieldMotionPrefixRead;
struct FieldAttackPrefixRead {
  const FieldMotionPrefixRead &motion;
  const std::map<StableId, Integer> &facing_mdeg;
  const std::map<StableId, Integer> &remaining_volley_pulses;
};
// Same original contact kernel over a verified historical local prefix.
Outcome<FieldContactRead>
read_field_attack_contacts_at_prefix(const FieldBattleSession &,
                                     const GeneratedBattleField &,
                                     const FieldAttackPrefixRead *);
// 完成移动后只更新原 Formation 接战状态与原 AI 触发集合；不推进时刻。
class CheckpointMemoryCharge;
Outcome<FieldContactRead> apply_field_contacts_in_candidate(
    FieldBattleSession &, const GeneratedBattleField &,
    const FieldBattleWindowSlice &, CheckpointMemoryCharge &);
} // namespace hanmo::resident
