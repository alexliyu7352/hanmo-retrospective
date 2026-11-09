// contact_install 的私有构造叶；只准备同候选会话值，不分配另一套世界或旧棋盘。
#pragma once
#include "field_contact.hpp"
#include "field_formations.hpp"
namespace hanmo::resident {
struct FieldBattleCatalog;
// 格子仅作为本次准备结果；安装与存读不得把它当永久存档字段。
struct FieldSessionPreparation {
  GeneratedBattleField field;
  FieldBattleSession session;
};
// 冷读按原接触弧长重建同一地形，不回放世界行动，也不另造投影公式。
Outcome<GeneratedBattleField>
regenerate_field_road_battle(const TravelCatalog &, const FieldBattleCatalog &,
                             const FieldRoadContact &);
// 两军与部队必须来自同一真实成军来源；此步尚未执行 AI 布阵、S1 冻结或发布事实。
Outcome<FieldSessionPreparation> prepare_field_road_session(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelState &, const TravelCatalog &, const FieldBattleCatalog &,
    const FieldRoadContact &, const StableId &battle_id,
    const StableId &contact_id, const StableId &player_actor_id,
    Integer battle_seed,
    const MilitaryFormationSourceRead *exact_formations = nullptr,
    const WorldMoment *actual_now = nullptr);
} // namespace hanmo::resident
