// 原八个Growth F行动的真实服务推进；唯一累计仍在S0 ActionElapsed。
#pragma once
#include "growth_action_time_read.hpp"
#include "resident/kernel/fixed_service_history.hpp"
namespace hanmo::resident {
// 同原候选的短栈许可，不能由命令、存档或自由Moment构造。
class GrowthFixedStepPermit {
  const CommonStore *common_ = nullptr;
  const GrowthState *growth_ = nullptr;
  const ActionElapsedStore *elapsed_ = nullptr;
  WorldIdentity identity_;
  StableId action_id_;
  Integer ordinal_ = 0, required_half_days_ = 0;
  WorldMoment at_, from_;
  ActualServiceIntervalView provided_;
  bool final_ = false;
  friend Outcome<GrowthFixedStepPermit> prepare_growth_fixed_step(
      const CommonStore &, const GrowthState &, const GrowthCatalog &,
      const CommonCatalog &, const ActionElapsedStore &, const TimeScaleRef &,
      const CatalogIdentity &, const ActionTimeRegistration &, const StableId &,
      const WorldMoment &, ExactArithmeticBudget, const InteractionState *,
      const InteractionCatalog *, const GrowthMarketExactSources *);
  friend Status validate_growth_fixed_step_permit(const GrowthFixedStepPermit &,
                                                  const CommonStore &,
                                                  const GrowthState &,
                                                  const ActionCoreView &);

public:
  GrowthFixedStepPermit() = default;
  const StableId &action_id() const { return action_id_; }
  Integer ordinal() const { return ordinal_; }
  Integer required_half_days() const { return required_half_days_; }
  bool final_gate() const { return final_; }
  const WorldMoment &at() const { return at_; }
  const WorldMoment &provided_from() const { return from_; }
  const ActualServiceIntervalView &provided() const { return provided_; }
};
Outcome<std::optional<WorldMoment>> predict_growth_fixed_step(
    const CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, const ActionElapsedStore &, const TimeScaleRef &,
    const CatalogIdentity &, const ActionTimeRegistration &, const StableId &,
    const WorldMoment &, ExactArithmeticBudget budget = {},
    const InteractionState *interaction = nullptr,
    const InteractionCatalog *interaction_catalog = nullptr,
    const GrowthMarketExactSources *market_exact = nullptr);
// 只写本原Action的S0累计；前置predict禁止跨未消费课时，不触业务钱货/身体。
Status provide_growth_fixed_interval_in_candidate(
    CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, ActionElapsedStore &, const TimeScaleRef &,
    const CatalogIdentity &, const std::vector<ActionTimeRegistration> &,
    const MomentSpan &, ExactArithmeticBudget budget = {},
    const InteractionState *interaction = nullptr,
    const InteractionCatalog *interaction_catalog = nullptr,
    const GrowthMarketExactSources *market_exact = nullptr);
Outcome<GrowthFixedStepPermit> prepare_growth_fixed_step(
    const CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, const ActionElapsedStore &, const TimeScaleRef &,
    const CatalogIdentity &, const ActionTimeRegistration &, const StableId &,
    const WorldMoment &, ExactArithmeticBudget budget = {},
    const InteractionState *interaction = nullptr,
    const InteractionCatalog *interaction_catalog = nullptr,
    const GrowthMarketExactSources *market_exact = nullptr);
Status validate_growth_fixed_step_permit(const GrowthFixedStepPermit &,
                                         const CommonStore &,
                                         const GrowthState &,
                                         const ActionCoreView &);
// 与旧step共享原业务实现；该入口不递增世界tick，completed_tick只是实际Moment粗投影。
Outcome<GrowthStep> step_growth_at_fixed_gate(
    CommonStore &, GrowthState &, const GrowthCatalog &, const CommonCatalog &,
    SocialState &, InteractionState &, const GrowthAccess &,
    const ActionCoreView &, const GrowthFixedStepPermit &);
} // namespace hanmo::resident
