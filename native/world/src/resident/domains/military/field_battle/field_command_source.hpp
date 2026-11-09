// Council 只引用真实 S0 成功封口；原 Common 缓存/History 仍是唯一结果来源。
#pragma once
#include "field_world.hpp"
#include <functional>
#include <string_view>
namespace hanmo::resident {
class CommonStore;
class FamilyLifeHistoryRead4;
struct CommitResult;
class CheckpointMemoryCharge;
Status
bind_field_battle_command_result(const CommonStore &, FieldBattleWorldState &,
                                 const StableId &actor, Integer sequence,
                                 const CommitResult &,
                                 CheckpointMemoryCharge &actual_world_owner);
Status visit_field_battle_command_sources(
    const CommonStore &, const FieldBattleWorldState &,
    const std::function<Status(const SourceRef &)> &consume,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
Status validate_field_battle_command_results(
    const CommonStore &, const FieldBattleWorldState *,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
// Current internal AI boundary; original adopted council/delegation/loss,
// not a player's military office permission or a writable AI flag alone.
Status validate_field_current_ai_side_authority(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, Integer side, const StableId &actor,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
Outcome<bool> read_field_current_ai_formation_permission(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, const FieldFormation &,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
struct FieldOriginalControlRead {
  bool direct = false;
  std::string_view controller_actor_id;
  std::optional<FieldBattleTime> dispatched_at;
};
// Latest reduction of actual original initial control and retained successful
// control commands. This is not an arbitrary historical time-cut API.
Outcome<FieldOriginalControlRead> read_field_original_control_history(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, const FieldFormation &);
// Actual internal arrival/status event, never the later OrderArrived report.
Outcome<FieldOriginalControlRead>
read_field_original_control_before_order_resolution(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, const FieldOrderRecord &);
struct FieldControlAcceptanceBoundary;
// Original player identity and actual delegation/local-loss/Life prefix only;
// callers separately prove the formation control at this exact boundary.
Outcome<bool> read_field_original_player_control_at_boundary(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, const FieldFormation &, std::string_view actor,
    const FieldControlAcceptanceBoundary &, const FamilyLifeHistoryRead4 *);
Status validate_field_delivered_order_authority(const CommonStore &,
                                                const FieldBattleWorldState &,
                                                const FieldBattleSession &,
                                                const FieldOrderRecord &,
                                                const FamilyLifeHistoryRead4 *);
// 有限 AI Direct/Order/Tactic 的真实 Common council 接受事实。
Outcome<const FieldCouncilFact *> read_field_ai_command_source(
    const CommonStore &, const FieldBattleWorldState &, const StableId &,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
// 真 AI Direct 事实仍在原 council 账；与玩家 Common CommandResult 严格分流。
Outcome<const FieldCouncilFact *> read_field_ai_direct_source(
    const CommonStore &, const FieldBattleWorldState &,
    const StableId &source_id,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
// Actual initial AI task permission at this formation's own entry head.
// Initial council, real local Lost, real Family death or accepted delegation;
// never today's AiController as historical authority.
Status validate_field_reinforcement_ai_control_source(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, const FieldFormation &,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
struct FieldDeploymentApplied;
Outcome<const FieldDeploymentApplied *>
read_field_original_deployment_application(const CommonStore &,
                                           const FieldBattleSession &,
                                           const FieldFormation &);
struct FieldInitialPlacementRead;
Status validate_field_navigation_mission_source(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, const FieldFormation &,
    const FieldBattleEvent &, const FieldInitialPlacementRead &,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
// Called only after the original adopter installed an actually verified source.
Status record_field_actual_mission_application(
    const CommonStore &, FieldBattleWorldState &, FieldBattleSession &,
    FieldFormation &, CheckpointMemoryCharge &,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
Status validate_field_actual_mission_application(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &, const FieldBattleEvent &,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
Status validate_field_mission_application_history(
    const CommonStore &, const FieldBattleWorldState &,
    const FieldBattleSession &,
    const FamilyLifeHistoryRead4 *original_life = nullptr);
} // namespace hanmo::resident
