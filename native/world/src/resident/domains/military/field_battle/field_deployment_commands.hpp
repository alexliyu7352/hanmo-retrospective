// 已选 Field 会话的零时间布阵命令；只修改外层事务的真实候选，不推进世界或战场。
#pragma once
#include "field_council_sources.hpp"
#include "field_deployment_command_model.hpp"
#include "field_validation.hpp"

namespace hanmo::resident {
class CurrentMomentRead;
class OriginalWorldReadBoundary;
class FamilyLifeHistoryRead4;
class CheckpointMemoryCharge;
// 只读预览完整实际布局，不改世界；成功输出与真正写入口共用同一准备体。
Status prepare_field_battle_deployment(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldBattleWorldState &, const StableId &issuer_actor_id,
    const FieldBattleSetDeployment &, FieldFormation &prepared_formation,
    const FieldBattleAbilityReadContext &abilities);
Status validate_field_battle_start(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldBattleWorldState &, const StableId &issuer_actor_id,
    const FieldBattleStart &, const FieldBattleAbilityReadContext &);
// 原军职/本部权限与同一战场来源先核完，才写一份实际 Formation；失败不改候选。
Status set_field_battle_deployment_in_candidate(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &issuer_actor_id, const FieldBattleSetDeployment &,
    const FieldBattleAbilityReadContext &abilities);
// 本侧实际编队已摆位才启动；不跳步、不改细时间、RNG 或原 S1 冻结位置。
Status start_field_battle_in_candidate(
    CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &issuer_actor_id, const FieldBattleStart &,
    const FieldBattleAbilityReadContext &, CheckpointMemoryCharge &);
} // namespace hanmo::resident
namespace hanmo::resident {
// 玩家主将的提案/定案与布阵同为零时操作；先核同场实际军职权限，再准备真实会话。
Status prepare_field_council_speak(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldBattleWorldState &, const StableId &, const FieldCouncilSpeak &,
    FieldBattleSession &, const FieldBattleAbilityReadContext &abilities,
    const FieldCouncilReadContext &council_sources);
Status prepare_field_council_adopt(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldBattleWorldState &, const StableId &, const FieldCouncilAdopt &,
    FieldBattleSession &, const FieldBattleAbilityReadContext &abilities,
    const FieldCouncilReadContext &council_sources);
// 提案不预写采纳事实；Adopt 才同候选写 MilitaryCouncilFact，绝不修改原
// SocialState。
Status speak_field_council_in_candidate(
    CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &, const FieldCouncilSpeak &,
    const FieldBattleAbilityReadContext &abilities,
    const FieldCouncilReadContext &council_sources);
Status adopt_field_council_in_candidate(
    CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &, const FieldCouncilAdopt &,
    const FieldBattleAbilityReadContext &abilities,
    const FieldCouncilReadContext &council_sources,
    CheckpointMemoryCharge &actual_world_owner);
} // namespace hanmo::resident

namespace hanmo::resident {
struct FieldBattleMutationResult;
// 来源由正式玩家 dispatcher 或本场 AI driver
// 提供，不是公开命令里的可伪造布尔值。
enum class FieldControlCommandOrigin { Player, Ai };
// 五个控制命令共用真实权源、任务/时限预核；只读入口不分配 ID 或改变位置。
Status validate_field_battle_control_command(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldBattleWorldState &, const StableId &, Integer request_sequence,
    const FieldDeploymentCommand &, FieldControlCommandOrigin,
    const FieldBattleAbilityReadContext &);
// 外层原 World 候选唯一写入；回执直接引用本次记录，不从后来状态猜结果。
Status apply_field_battle_control_command_in_candidate(
    CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &, Integer request_sequence, const FieldDeploymentCommand &,
    FieldControlCommandOrigin, const FieldBattleAbilityReadContext &,
    FieldBattleMutationResult &);
} // namespace hanmo::resident

namespace hanmo::resident {
// §7 第一步的真实控制令采用；仅借外层原候选和已经开放的一步。
// 不推进 now，不处理援军/战法/伤亡；任意失败由原事务丢弃整个候选。
Status apply_field_control_arrivals_in_candidate(
    CommonStore &, FieldBattleWorldState &, const StableId &battle_id,
    const FieldBattleParameters &, const FieldBattleWindowSlice &one_step,
    CheckpointMemoryCharge &actual_world_owner,
    const FamilyLifeHistoryRead4 *original_life = nullptr,
    const CurrentMomentRead *actual_clock = nullptr,
    const OriginalWorldReadBoundary *actual_boundary = nullptr);
// 完成真实一步后才发布已经回传的军令事件，防视图游标提前泄露或漏报。
Status publish_field_order_reports_in_candidate(FieldBattleSession &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 同原五类军职/本部权源；Direct 即刻发动，其余只接受实际传令，不提前扣点。
Status validate_field_battle_tactic_command(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldBattleWorldState &, const StableId &, const FieldBattleTactic &,
    FieldControlCommandOrigin, const FieldBattleAbilityReadContext &);
Status apply_field_battle_tactic_command_in_candidate(
    CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &, const FieldBattleTactic &, FieldControlCommandOrigin,
    const FieldBattleAbilityReadContext &,
    CheckpointMemoryCharge &actual_world_owner, FieldBattleMutationResult &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Running Confuse consumes the original side strategist and actual accepted
// source head. Council plans use the separate real Adopted installer.
Status validate_field_confuse_command(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldBattleWorldState &, const StableId &actor,
    const FieldBattleStratagem &, const FieldBattleAbilityReadContext &);
Status apply_field_confuse_command_in_candidate(
    CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &actor, const FieldBattleStratagem &,
    const FieldBattleAbilityReadContext &, CheckpointMemoryCharge &,
    FieldBattleMutationResult &);
} // namespace hanmo::resident

namespace hanmo::resident {
// Council attachment only. The original Common result binder supplies the
// persistent source after this call, in the same unpublished candidate.
Status validate_field_council_stratagem_command(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &,
    const FieldBattleWorldState &, const StableId &,
    const FieldBattleStratagem &, const FieldBattleAbilityReadContext &);
Status attach_field_council_stratagem_in_candidate(
    const CommonStore &, const MilitaryState &, const MilitaryCatalog &,
    const TravelCatalog &, const FieldBattleCatalog &, FieldBattleWorldState &,
    const StableId &, const FieldBattleStratagem &,
    const FieldBattleAbilityReadContext &, CheckpointMemoryCharge &,
    FieldBattleMutationResult &);
} // namespace hanmo::resident
