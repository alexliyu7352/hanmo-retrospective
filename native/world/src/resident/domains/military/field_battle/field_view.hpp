// §9/§13 唯一公开投影；调用者仍须完成原命令身份、权限与版本核验。
#pragma once
#include "field_ability_sources.hpp"
#include "field_deployment_command_model.hpp"
#include "field_view_model.hpp"
#include "field_world.hpp"
namespace hanmo::resident {
struct FieldBattleCatalog;
struct MilitaryCatalog;
struct TravelCatalog;
// 同 World 的原 Common/军事/习得目录是唯一名字与能力来源。
// 纯读不生成 cursor、command_result、事件、RNG 或任何世界写入。
Outcome<FieldBattleView> read_field_battle_view(
    const CommonStore &, const MilitaryState &, const TravelCatalog &,
    const FieldBattleCatalog &, const FieldBattleWorldState &,
    const StableId &observer_actor_id, const FieldBattleRead &,
    const FieldBattleAbilityReadContext &abilities);
// Same exact observer-filtered projection for the native local AI phases.
// Does not pretend the intermediate candidate is a published valid checkpoint.
Outcome<FieldBattleView> read_field_battle_candidate_view(
    const CommonStore &, const MilitaryState &, const TravelCatalog &,
    const FieldBattleCatalog &, const FieldBattleWorldState &,
    const StableId &observer_actor_id, const FieldBattleRead &,
    const FieldBattleAbilityReadContext &abilities);
} // namespace hanmo::resident
