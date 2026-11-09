// Original population writers retain mutation ownership. This token closes only
// the production inputs they actually change, in the same outer candidate.
#pragma once
#include "production_adoption_source.hpp"
#include "production_population_source.hpp"
namespace hanmo::resident {
struct ProductionPopulationChangeContext {
 CommonStore& common;LocalEconomyState& economy;const LocalEconomyCatalog& catalog;
 EconomicProductionContext& production;const EconomicProductionAdoptionRead& adoptions;
 const OriginalProductionPopulationChangeContext& original;
};
struct ProductionPopulationChangeAccess;
class PreparedProductionPopulationChange {
 friend struct ProductionPopulationChangeAccess;
 struct Entry {StableId unit_id;std::optional<PreparedProductionInputClose> close;};
 const CommonStore* common_=nullptr;const EconomicWorkSourcesState* sources_=nullptr;
 WorldMoment at_;std::size_t fact_count_=0;StableId last_fact_;
 std::vector<Entry> entries_;bool valid_=false;
public:
 PreparedProductionPopulationChange()=default;
 PreparedProductionPopulationChange(const PreparedProductionPopulationChange&)=delete;
 PreparedProductionPopulationChange& operator=(const PreparedProductionPopulationChange&)=delete;
 PreparedProductionPopulationChange(PreparedProductionPopulationChange&&)=default;
 PreparedProductionPopulationChange& operator=(PreparedProductionPopulationChange&&)=default;
};
// Before the original count/labor/location/qualification writer. A future split
// cohort need not be preallocated; existing source cohorts determine old inputs.
// Reads the trusted clock and changes no ID, source, or original business state.
Outcome<PreparedProductionPopulationChange> prepare_production_population_change(
 const ProductionPopulationChangeContext&,const std::vector<StableId>& original_cohort_ids);
// After every actual original row/revision and spatial/qualification result of
// this batch has been applied. The finite sources must be created after prepare.
// Uses each affected unit's last relevant Common cut; no synthetic batch source.
// Any error requires discarding the whole outer candidate, including originals.
Outcome<std::vector<SourceRef>> apply_production_population_change_in_candidate(
 ProductionPopulationChangeContext&,PreparedProductionPopulationChange&&,
 const std::vector<MomentOwnerSource>& actual_changes);
}
