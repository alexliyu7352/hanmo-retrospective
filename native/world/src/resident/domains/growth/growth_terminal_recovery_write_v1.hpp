// 新终局治疗：只在原FSL满工许可与同候选capture内prepare/finish/bind，不逐期给点。
#pragma once
#include "growth_recovery_progress.hpp"
#include "resident/domains/actor_foundation/condition_capture.hpp"
namespace hanmo::resident {
struct GrowthRecoveryOccupationRead;
struct CharacterInjuryReplaySourcesV1;
struct CharacterInjurySourcesStateV1;
struct AuthoredActorFoundationState;
struct GrowthState;
struct ActionCoreView;
class GrowthFixedStepPermit;
class CommonStore;
class PreparedTerminalRecoveryV1 {
  struct State;
  std::shared_ptr<State> state_;
  friend Outcome<PreparedTerminalRecoveryV1> prepare_terminal_recovery_v1(
      CommonStore &, AuthoredActorFoundationState &,
      CharacterInjurySourcesStateV1 &, const CharacterInjuryReplaySourcesV1 &,
      const GrowthFixedStepPermit &, const GrowthRecoveryOccupationRead &);
  friend Outcome<GrowthRecoveryCredit> read_terminal_recovery_service_write_v1(
      const PreparedTerminalRecoveryV1 &, const CommonStore &,
      const GrowthState &, const ActionCoreView &);
  friend Outcome<BoundActorConditionWrite> bind_terminal_recovery_condition_v1(
      PreparedTerminalRecoveryV1 &, CommonStore &,
      AuthoredActorFoundationState &, CharacterInjurySourcesStateV1 &,
      const CharacterInjuryReplaySourcesV1 &);

public:
  PreparedTerminalRecoveryV1() = default;
};
// 真实完整前缀、方法目录、满工服务窗决定点数；无法由请求指定raw delta。
Outcome<PreparedTerminalRecoveryV1> prepare_terminal_recovery_v1(
    CommonStore &, AuthoredActorFoundationState &,
    CharacterInjurySourcesStateV1 &, const CharacterInjuryReplaySourcesV1 &,
    const GrowthFixedStepPermit &, const GrowthRecoveryOccupationRead &);
// 原finish_service_trade唯一身体写者借读；重复/跨候选/写前像变化皆拒。
Outcome<GrowthRecoveryCredit> read_terminal_recovery_service_write_v1(
    const PreparedTerminalRecoveryV1 &, const CommonStore &,
    const GrowthState &, const ActionCoreView &);
// 原业务成功后配对真实单次写；不再写身体或付款，技术失败回滚整World候选。
Outcome<BoundActorConditionWrite> bind_terminal_recovery_condition_v1(
    PreparedTerminalRecoveryV1 &, CommonStore &, AuthoredActorFoundationState &,
    CharacterInjurySourcesStateV1 &, const CharacterInjuryReplaySourcesV1 &);
} // namespace hanmo::resident
