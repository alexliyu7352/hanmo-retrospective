// Same-candidate original Personal terminal body writer. No caller supplied
// delta.
#pragma once
#include "condition_capture.hpp"
namespace hanmo::resident {
struct PersonalState;
struct PersonalCatalog;
struct PersonalBattleLifecycleWriteContext;
struct CharacterInjurySourcesStateV1;
struct CharacterInjuryReplaySourcesV1;
struct GrowthPermanentState;
class GrowthCapabilityRevisionWrite;
class PersonalTerminalConditionWrite {
public:
  virtual ~PersonalTerminalConditionWrite() = default;
  virtual bool bound_to(const CommonStore &, const PersonalState &,
                        const PersonalCatalog &) const noexcept = 0;
  virtual Status apply_actual_terminal(const StableId &battle,
                                       const StableId &actor) = 0;
};
Outcome<BoundActorConditionWrite>
apply_personal_terminal_condition_in_candidate_v1(
    CommonStore &, AuthoredActorFoundationState &,
    CharacterInjurySourcesStateV1 &, const CharacterInjuryReplaySourcesV1 &,
    const SourceRef &, const StableId &,
    const PersonalBattleLifecycleWriteContext &, const GrowthPermanentState &,
    GrowthCapabilityRevisionWrite &);
// Stack-bound by central to the actual World candidate and its lifecycle clock.
class NativePersonalTerminalConditionWrite final
    : public PersonalTerminalConditionWrite {
public:
  NativePersonalTerminalConditionWrite(
      CommonStore &c, AuthoredActorFoundationState &s,
      CharacterInjurySourcesStateV1 &i, const CharacterInjuryReplaySourcesV1 &r,
      const PersonalBattleLifecycleWriteContext &clock,
      const GrowthPermanentState &g, GrowthCapabilityRevisionWrite &w)
      : common_(c), state_(s), injury_(i), sources_(r), clock_(clock),
        growth_(g), capability_(w) {}
  bool bound_to(const CommonStore &, const PersonalState &,
                const PersonalCatalog &) const noexcept override;
  Status apply_actual_terminal(const StableId &, const StableId &) override;

private:
  CommonStore &common_;
  AuthoredActorFoundationState &state_;
  CharacterInjurySourcesStateV1 &injury_;
  const CharacterInjuryReplaySourcesV1 &sources_;
  const PersonalBattleLifecycleWriteContext &clock_;
  const GrowthPermanentState &growth_;
  GrowthCapabilityRevisionWrite &capability_;
};
} // namespace hanmo::resident
