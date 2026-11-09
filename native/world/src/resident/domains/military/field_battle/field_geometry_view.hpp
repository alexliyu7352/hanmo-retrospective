// §9 读口的地形与会话身份消费叶；不是缺事件/军令的整份 FieldBattleView。
#pragma once
#include "field_validation.hpp"

namespace hanmo::resident {
// 所有值来自已安装 Session；客户端只能回传已知 hash，不能给第二套战场格。
struct FieldBattleGeometryView {
  StableId battle_id, observer_actor_id, field_hash;
  Integer revision = 0, segment = 0;
  FieldBattleTime now;
  FieldBattleState state = FieldBattleState::Council;
  FieldAnchor anchor;
  Integer length_m = 0, width_m = 0, cell_m = 0;
  std::optional<std::vector<FieldCell>> cells;
  std::array<FieldBattleSide, 2> sides;
  std::optional<Integer> observer_side;
  std::optional<FieldCouncilRecord> council;
};

// 任一真实观察者可读；冷读同源重建并核 hash，已知同 hash 时只省略格子载荷。
Outcome<FieldBattleGeometryView> read_field_battle_geometry(
    const CommonStore &, const MilitaryState &, const TravelCatalog &,
    const FieldBattleCatalog &, const FieldBattleWorldState &,
    const StableId &battle_id, const StableId &observer_actor_id,
    const StableId &known_field_hash = {},
    const FieldBattleAbilityReadContext *abilities = nullptr);
// Native local driver only: project original Contact/Session/author geometry
// while a complete candidate is between phases. Not a public read/validation
// gate; the complete World must validate before any publication.
Outcome<FieldBattleGeometryView> read_field_battle_candidate_geometry(
    const CommonStore &, const MilitaryState &, const TravelCatalog &,
    const FieldBattleCatalog &, const FieldBattleWorldState &,
    const StableId &battle_id, const StableId &observer_actor_id,
    const StableId &known_field_hash,
    const FieldBattleAbilityReadContext *abilities);
} // namespace hanmo::resident
