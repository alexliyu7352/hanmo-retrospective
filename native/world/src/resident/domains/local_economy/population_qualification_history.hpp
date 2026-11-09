// Original P02 counts and S6 qualification sources, shared by actual economic
// consumers. No saved eligibility table or current-head substitution.
#pragma once
#include "population_change_time.hpp"
#include "resident/domains/organization/organization_population_training_source.hpp"
namespace hanmo::resident {
struct PopulationQualificationHistoryContext {
 const PopulationChangeTimeContext& population;
 const LocalEconomyCatalog& catalog;
 const OrganizationTrainingSourceContext& training;
};
struct OriginalPopulationQualification {
 PopulationRevisionView population;
 PopulationStatus status=PopulationStatus::civilian;
 std::optional<StableId> assignment;
 std::vector<SourceRef> original_sources;
};
// The adopting owner proves its actual cut and M. This reader reconstructs the
// exact original revision and qualification after that real Common source.
// Unsupported military/condition histories fail, never become zero or civilian.
Outcome<OriginalPopulationQualification> read_original_population_qualification_after_source(
 const PopulationQualificationHistoryContext&,const StableId& cohort,
 const SourceRef& actual_cut,const WorldMoment& actual_at);
Status validate_original_population_qualification_window(
 const PopulationQualificationHistoryContext&,const StableId& cohort,
 const SourceRef& actual_cut,const MomentSpan&);
}
