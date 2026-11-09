// 新道路会战仅装入同一未发布世界候选；失败由外层事务丢弃，不创建旧棋盘。
#pragma once
#include "field_ability_sources.hpp"
#include "field_author_data.hpp"
#include "field_session_build.hpp"
#include "field_world.hpp"
namespace hanmo::resident {
struct SocialState;
struct FieldExactContactReadContext;
struct PoliticalRelationsReadContext;
class CheckpointMemoryCharge;
struct OrganizationState;
// 实际原步之后才能调用；ID 来自原 Common 序号，原路触截断器冻结世界锚点。
// 未审定的窗口末/AI 中军任务分支显式拒绝；不会先发布半成品 Session。
Outcome<StableId> install_field_road_session(
    CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    TravelState &, const TravelCatalog &, const FieldBattleCatalog &,
    FieldBattleWorldState &, const FieldRoadContact &,
    const StableId &player_actor_id, Integer battle_seed, const SocialState &,
    const OrganizationState &, const FieldBattleAbilityReadContext &,
    CheckpointMemoryCharge &actual_world_owner,
    const MilitaryFormationSourceRead *exact_formations = nullptr);
// Automatic hostile encounter only, using the actual P3 relation before this
// session exists. Neutral/Allied deliberate attacks need their original named
// attack source; this entry must not turn a mere crossing into such an attack.
Outcome<StableId> install_hostile_field_exact_group_session_in_candidate(
    CommonStore &, FieldBattleWorldState &,
    const FieldExactContactReadContext &, const PoliticalRelationsReadContext &,
    const FieldBattleCatalog &, const SourceRef &actual_contact,
    const StableId &player_actor_id, Integer battle_seed, const SocialState &,
    const OrganizationState &, const FieldBattleAbilityReadContext &,
    CheckpointMemoryCharge &);
} // namespace hanmo::resident
