// 原场内 AI 决策只经正式 Direct/Order 写者；不代公开玩家命令造封口。
#pragma once
#include "field_ai_decision.hpp"
namespace hanmo::resident {
class CommonStore;
struct MilitaryState;
struct MilitaryCatalog;
struct TravelCatalog;
struct FieldBattleCatalog;
struct FieldBattleWorldState;
struct FieldBattleAbilityReadContext;
class CheckpointMemoryCharge;
Status apply_field_ai_missions_in_candidate(
    CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &battle_id, const GeneratedBattleField &,
    const FieldBattleAbilityReadContext &, const FieldBattleWindowSlice &,
    CheckpointMemoryCharge &actual_world_owner);
} // namespace hanmo::resident
