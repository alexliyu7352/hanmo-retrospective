// The original LongRest result remains the sole owner of every actual N-period.
#pragma once
#include "growth_fixed_execution.hpp"
#include "resident/domains/estate/estate_rest_history.hpp"
namespace hanmo::resident {
struct CharacterConditionSources;
struct LongRestPeriodSources {
 const CommonStore& common;const GrowthState& growth;const GrowthCatalog& catalog;const CommonCatalog& shared;
 const ActionElapsedStore& elapsed;const TimeScaleRef& scale;const CatalogIdentity& identity;
 const std::vector<ActionTimeRegistration>& registrations;const EstateRestHistoryContext& estates;
 WorldMoment actual_now;const CharacterConditionSources* condition=nullptr;
};
struct LongRestActualPeriodRead {
 const RestFact* rest=nullptr;const LongRestActualPeriod* period=nullptr;
 EstateRestHistoricalRead estate;
};
Outcome<LongRestActualPeriodRead> read_long_rest_actual_period(const LongRestPeriodSources&,const StableId& action_id,Integer ordinal);
Status validate_long_rest_actual_periods(const LongRestPeriodSources&);
// Original pre-application mouths algorithm, shared by the original writer and
// its same-candidate capture; it does not certify historical family/position.
Outcome<std::vector<StableId>> read_growth_rest_mouths(const CommonStore&,const GrowthAccess&,const StableId&,const RestEstateView&);
}
