// 只绑定原Growth一次rest写入；临时边界不能由UI构造或跨候选/rollback复用。
#pragma once
#include "condition_capture.hpp"
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
class CommonStore;
struct GrowthState;
struct GrowthCatalog;
class GrowthFixedStepPermit;
struct CharacterInjuryReplaySourcesV1;
struct CharacterInjurySourcesStateV1;
struct ActorConditionCaptureIdentity;
// 真实捕获身份与序号只活在同一候选；不进入永久收据或冷存。
using BoundAuthoredRestConditionWrite = BoundActorConditionWrite;
class AuthoredRestTimeRead {
public:
  virtual ~AuthoredRestTimeRead() = default;
  // K2从本Action真实Completed边界反读，禁止用旧Tick补offset或当前Now。
  virtual Outcome<WorldMoment>
  completed_rest_at(const StableId &action_id) const = 0;
};
struct LifeActivityRecord;
struct OriginalRestProvidedCause;
struct AuthoredRestCompletionRead {
  const LifeActivityRecord *activity = nullptr;
  WorldMoment completed_at;
  bool reduces_injury =
      false; // original terms.hp_gain > 0, not treatment qualification
};
// Original action/terms/Common head and actual completion proof only. Does not
// decide earned-point carry, create a Condition receipt or mutate the actor.
Outcome<AuthoredRestCompletionRead> read_authored_rest_completion(
    const CommonState &, const GrowthState &, const ActorFoundationCatalog &,
    const OriginalRestProvidedCause &, const StableId &actor_id,
    const AuthoredRestTimeRead &, const WorldMoment &actual_now);
class AuthoredRestConditionBoundary {
public:
  struct State;
  AuthoredRestConditionBoundary() = default;

private:
  std::shared_ptr<State> state_;
  friend Outcome<AuthoredRestConditionBoundary>
  begin_injury_rest_condition_write(CommonStore &,
                                    AuthoredActorFoundationState &,
                                    CharacterInjurySourcesStateV1 &,
                                    const CharacterInjuryReplaySourcesV1 &,
                                    const ActionCoreView &,
                                    const GrowthFixedStepPermit *);
  friend Outcome<AuthoredRestConditionBoundary>
  begin_authored_rest_condition_write(CommonStore &,
                                      const AuthoredActorFoundationState &,
                                      const ActorFoundationCatalog &,
                                      const GrowthState &,
                                      const ActionCoreView &,
                                      const WorldMoment &);
  friend Outcome<BoundAuthoredRestConditionWrite>
  bind_authored_rest_condition_write(CommonStore &,
                                     AuthoredActorFoundationState &,
                                     const ActorFoundationCatalog &,
                                     const GrowthState &,
                                     const ActionCoreView &,
                                     const WorldMoment &,
                                     AuthoredRestConditionBoundary &);
};
// K2已打开Common原捕获；本函数必须在原finish写身体之前立即调用。
Outcome<AuthoredRestConditionBoundary> begin_authored_rest_condition_write(
    CommonStore &, const AuthoredActorFoundationState &,
    const ActorFoundationCatalog &, const GrowthState &, const ActionCoreView &,
    const WorldMoment &);
// source1普通休养热边界：只借同候选真实前缀，治疗资格必须改走终局给点分支。
Outcome<AuthoredRestConditionBoundary> begin_injury_rest_condition_write(
    CommonStore &, AuthoredActorFoundationState &,
    CharacterInjurySourcesStateV1 &, const CharacterInjuryReplaySourcesV1 &,
    const ActionCoreView &, const GrowthFixedStepPermit *actual_gate = nullptr);
// 原apply_actor和LifeActivity完成已成功后绑定；只分配receipt身份，不再修改身体。
Outcome<BoundAuthoredRestConditionWrite> bind_authored_rest_condition_write(
    CommonStore &, AuthoredActorFoundationState &,
    const ActorFoundationCatalog &, const GrowthState &, const ActionCoreView &,
    const WorldMoment &, AuthoredRestConditionBoundary &);
// 在Common finish_capture之前核全部真实写入均恰一次被本候选绑定。
Status validate_authored_rest_capture(
    const CommonStore &, const AuthoredActorFoundationState &,
    const std::vector<BoundAuthoredRestConditionWrite> &);
// 冷反验同一个真实已完成服务和原公式；提供给整段validator。
Status validate_authored_rest_receipt(const CommonState &,
                                      const ConditionReceiptV2 &,
                                      const ActorFoundationCatalog &,
                                      const GrowthState &,
                                      const AuthoredRestTimeRead &);
} // namespace hanmo::resident
