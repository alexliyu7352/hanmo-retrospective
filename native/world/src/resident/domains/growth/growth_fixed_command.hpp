#pragma once
#include "growth_fixed_execution.hpp"
#include "growth_fixed_start.hpp"
namespace hanmo::resident {
// A short-lived proof of this original action's actual F history. Only the
// same-candidate reader can construct it; it is never decoded or
// caller-authored.
class GrowthFixedCommandPermit {
  CheckpointMemoryCharge memory_;
  const CommonStore *common_ = nullptr;
  const GrowthState *growth_ = nullptr;
  const ActionElapsedStore *elapsed_ = nullptr;
  WorldIdentity identity_;
  StableId actor_, action_, kind_, previous_;
  WorldMoment at_;
  friend Outcome<GrowthFixedCommandPermit> prepare_growth_fixed_command(
      const CharacterConditionSources &, const GrowthState &, const StableId &,
      const GrowthCommand &, const GrowthMarketExactSources *);
  friend Status
  validate_growth_fixed_command_permit(const GrowthFixedCommandPermit &,
                                       const CommonStore &, const GrowthState &,
                                       const StableId &, const GrowthCommand &);

public:
  GrowthFixedCommandPermit() = default;
  const WorldMoment &at() const { return at_; }
  const StableId &action_id() const { return action_; }
};
Outcome<GrowthFixedCommandPermit> prepare_growth_fixed_command(
    const CharacterConditionSources &, const GrowthState &,
    const StableId &actor, const GrowthCommand &,
    const GrowthMarketExactSources *market_exact = nullptr);
Status validate_growth_fixed_command_permit(const GrowthFixedCommandPermit &,
                                            const CommonStore &,
                                            const GrowthState &,
                                            const StableId &actor,
                                            const GrowthCommand &);
Outcome<GrowthQuote> preview_growth_at_fixed_command(
    const CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, const SocialState &, const GrowthAccess &,
    const StableId &, const GrowthCommand &, const GrowthFixedCommandPermit &);
Outcome<GrowthEffect> apply_growth_instant_at_fixed_command(
    CommonStore &, GrowthState &, const GrowthCatalog &, const CommonCatalog &,
    const SocialState &, const GrowthAccess &, const StableId &,
    const GrowthQuote &, const GrowthFixedCommandPermit &);
// Adopt each genuinely new positive late adjustment into every live F input
// that actually reads that actor/skill. Paused actions remain paused.
Status adopt_growth_adjustment_inputs_in_candidate(
    const GrowthState &before, CommonStore &, GrowthState &,
    const CharacterConditionSources &, ActionElapsedStore &,
    const StableId &actor, Integer request_seq, const CommitResult &);
// After the real result/history and NPC adoption, and after original
// cancellation release. This binds the original effect once; it never repeats
// that effect.
Outcome<StableId> bind_growth_fixed_control_command_in_candidate(
    const GrowthState &before, CommonStore &, GrowthState &,
    const CharacterConditionSources &, ActionElapsedStore &,
    const GrowthFixedCaptureServices &, const StableId &actor,
    Integer request_seq, const CommitResult &);
} // namespace hanmo::resident
