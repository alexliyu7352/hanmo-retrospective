// 原 settle_casualties 的唯一名册落账叶；不持有棋盘或另建兵员余额。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include <span>
namespace hanmo::resident {
struct MilitaryState;
// 同步借用已验的原部队身份及五类完整名单；调用方保持全部引用存活。
struct OriginalMilitaryRosterFinalView {
  const StableId &unit_id, &army_id, &commander_actor_id, &troop_type_id;
  const std::vector<StableId> &member_ids, &fit, &wounded, &dead, &captured,
      &deserted;
  Integer morale;
};
// 只复用原不可逆分类、俘获军团、具名变化及一份 MilitaryCasualtyFact 写入。
// 调用者先验实际业务来源、完整互斥名册及将领变化，并负责同段幂等与外层候选原子性。
Outcome<MilitaryCasualtyFact> apply_original_military_roster_casualties(
    CommonWrite &, MilitaryState &, const StableId &actual_battle_source,
    Integer recorded_tick, const std::vector<StableId> &actual_army_ids,
    std::span<const OriginalMilitaryRosterFinalView>,
    std::vector<MilitaryActorChange> actual_officer_changes,
    const std::vector<std::pair<StableId, StableId>> &new_captors,
    bool permits_zero_morale_fit);
} // namespace hanmo::resident
