// 同一原Growth validator的精确来源分支；保留旧条款/价格/XP规则，不用legacy
// Tick等式验Time2。
#pragma once
#include "study_pending_release.hpp"
#include "study_permanent_contribution_read.hpp"
namespace hanmo::resident {
struct GrowthMarketExactSources;
struct GrowthCompleteValidationSources {
  const CharacterConditionSources &condition;
  const VerifiedDeathRead *deaths = nullptr;
  const ReadStamp *death_read_stamp = nullptr;
  // Only a private same-capture reader may borrow these writer-issued
  // witnesses. Export, restore and the published commit validation leave this
  // null.
  const StudyPendingReleaseProofs *pending_study_releases = nullptr;
  const GrowthMarketExactSources *market_exact = nullptr;
};
Status validate_complete_rest_result_sources(
    const CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, const StableId &action_id, const RestFact &,
    const GrowthCompleteValidationSources &);
Status validate_complete_training_result_sources(
    const CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, const StableId &action_id, const TrainingFact &,
    const GrowthCompleteValidationSources &);
Status validate_complete_study_result_sources(
    const CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, const GrowthAccess &, const StableId &action_id,
    const StudyFact &, const GrowthCompleteValidationSources &);
Status validate_growth_with_complete_sources(
    const CommonStore &, const GrowthState &, const GrowthCatalog &,
    const CommonCatalog &, const GrowthAccess &,
    const GrowthCompleteValidationSources &);
} // namespace hanmo::resident
