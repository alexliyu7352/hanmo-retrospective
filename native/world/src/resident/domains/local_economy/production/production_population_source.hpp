// Pure original population/qualification change evidence for production inputs.
// No production or political reader is called from this source projection.
#pragma once
#include "../population_change_time.hpp"
#include "resident/domains/organization/organization_population_training_source.hpp"
namespace hanmo::resident {
struct OriginalProductionPopulationChangeContext {
 const PopulationChangeTimeContext& population;
 const OrganizationTrainingSourceContext& training;
};
struct OriginalProductionPopulationChange {
 MomentOwnerSource actual_source;WorldMoment occurred_at;
 std::vector<StableId> cohort_ids;
};
bool is_original_production_population_change(const MomentOwnerSource&);
Outcome<OriginalProductionPopulationChange> read_original_production_population_change(
 const OriginalProductionPopulationChangeContext&,const MomentOwnerSource&);
}
