// 唯一玩家选口入口；中央仍独占命令序号、Common成功回执和整World候选提交。
#pragma once
#include "field_army_exit_command.hpp"
#include "field_view_model.hpp"
namespace hanmo::resident {
struct FieldExactContactReadContext;
struct FieldBattleCatalog;
struct FieldBattleAbilityReadContext;
struct MilitaryAccess;
struct FieldBattleWorldState;
struct FieldArmyExitPlan;
class CommonStore;
class CheckpointMemoryCharge;
struct CommitResult;
Status preview_field_army_exit_select(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const MilitaryAccess &return_access,
    const StableId &actor, const FieldArmyExitSelect &);
Outcome<FieldBattleMutationResult> apply_field_army_exit_select_in_candidate(
    CommonStore &, FieldBattleWorldState &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const MilitaryAccess &return_access,
    const StableId &actor, const FieldArmyExitSelect &, CheckpointMemoryCharge &);
Status bind_field_army_exit_command_result(
    const CommonStore &, FieldBattleWorldState &, const StableId &actor,
    Integer sequence, const CommitResult &, CheckpointMemoryCharge &);
} // namespace hanmo::resident
namespace hanmo::resident {
// NPC与玩家未下令共用同一自动选择；调用者仅在真实有序撤退/终局边界请求。
Outcome<SourceRef> ensure_field_army_exit_choice_in_candidate(
    CommonStore &, FieldBattleWorldState &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const MilitaryAccess &return_access,
    const StableId &battle_id, const StableId &army_id, CheckpointMemoryCharge &);
// 原Common头紧接实际检查cut；冷读只反证本来源，不推进路线或补选出口。
Outcome<const FieldArmyExitPlan *> read_field_army_exit_choice_source(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const MilitaryAccess &return_access,
    const SourceRef &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 原Common结果与唯一选口事实双向对应；PIN访问不借完整世界回调递归。
Status validate_field_army_exit_player_command_source(
    const CommonStore &, const FieldBattleWorldState &, const FieldArmyExitPlan &);
// 完整选口表及Common头反覆盖；汇合/ready的后继来源另由其真实读口验证。
Status validate_field_army_exit_choice_sources(
    const FieldExactContactReadContext &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const MilitaryAccess &return_access);
} // namespace hanmo::resident

namespace hanmo::resident {
struct FieldBattleSession;
struct FieldFormation;
struct GeneratedBattleField;
// 同军唯一已锁选择的局部投影；完整原选择由生产入口/冷表验证，不接受任意外传格。
Outcome<PointI> read_field_locked_army_exit_target(
    const FieldBattleWorldState &, const FieldBattleSession &,
    const FieldFormation &, const GeneratedBattleField &,
    const FieldBattleTime &, Integer last_local_event_sequence);
// 原局部采用后的两个边界：motion前锁有序撤退；terminal只锁实际需要离场的军。
Status ensure_field_required_army_exit_choices_in_candidate(
    CommonStore &, FieldBattleWorldState &, const FieldBattleCatalog &,
    const FieldBattleAbilityReadContext &, const StableId &battle_id,
    bool terminal, CheckpointMemoryCharge &);
} // namespace hanmo::resident

namespace hanmo::resident {
// 选口不是撤退令：胜方只有真实有序退出/仍生效Withdraw才启动汇合；败方原后果仍必需。
Outcome<bool> read_field_army_terminal_exit_required(
    const FieldBattleSession &, const StableId &army_id);
} // namespace hanmo::resident
