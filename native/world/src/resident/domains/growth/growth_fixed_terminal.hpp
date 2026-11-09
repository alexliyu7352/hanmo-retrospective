// Actual full-service completion keeps the input cut before the original
// effect.
#pragma once
#include "growth_fixed_execution.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include <functional>
namespace hanmo::resident {
struct GrowthFixedStudyApplication;
class PreparedGrowthFixedTerminalInputs {
  struct State;
  std::shared_ptr<State> state_;
  friend Outcome<PreparedGrowthFixedTerminalInputs>
  prepare_growth_fixed_terminal_inputs(CommonStore &, const GrowthState &,
                                       const CharacterConditionSources &,
                                       const GrowthFixedStepPermit &,
                                       const GrowthMarketExactSources *);
  friend Outcome<StableId> complete_growth_fixed_terminal_in_candidate(
      CommonStore &, GrowthState &, const CharacterConditionSources &,
      ActionElapsedStore &, const ActionTimeRegistration &, const GrowthStep &,
      PreparedGrowthFixedTerminalInputs &,
      const std::function<Status(const StableId &)> &);
  friend Outcome<StableId> adopt_growth_fixed_period_inputs_in_candidate(
      CommonStore &, GrowthState &, const CharacterConditionSources &,
      ActionElapsedStore &, const ActionTimeRegistration &,
      const GrowthFixedStepPermit &, const GrowthFixedStudyApplication &,
      PreparedGrowthFixedTerminalInputs &);

public:
  PreparedGrowthFixedTerminalInputs() = default;
};
// Called immediately before the original professional effect within the
// existing outer Condition capture. Does not append a fact or mark the action
// completed.
Outcome<PreparedGrowthFixedTerminalInputs> prepare_growth_fixed_terminal_inputs(
    CommonStore &, const GrowthState &, const CharacterConditionSources &,
    const GrowthFixedStepPermit &,
    const GrowthMarketExactSources *market_exact = nullptr);
// Original result is already materialized. Calls the same central
// release_action and then adopts the unique FSL terminal + original terminal
// publication. Any failure discards the original private World candidate,
// including payment.
Outcome<StableId> complete_growth_fixed_terminal_in_candidate(
    CommonStore &, GrowthState &, const CharacterConditionSources &,
    ActionElapsedStore &, const ActionTimeRegistration &, const GrowthStep &,
    PreparedGrowthFixedTerminalInputs &,
    const std::function<Status(const StableId &)> &original_release);
Outcome<StableId> adopt_growth_fixed_period_inputs_in_candidate(
    CommonStore &, GrowthState &, const CharacterConditionSources &,
    ActionElapsedStore &, const ActionTimeRegistration &,
    const GrowthFixedStepPermit &, const GrowthFixedStudyApplication &,
    PreparedGrowthFixedTerminalInputs &);
} // namespace hanmo::resident
