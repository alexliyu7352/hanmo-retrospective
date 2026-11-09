// Original cohort/allocation/qualification inputs of one production unit.
#pragma once
#include "production_supply_input.hpp"
#include "../population_labor_history.hpp"
#include "resident/domains/organization/organization_population_training_source.hpp"
#include "resident/domains/travel/population_spatial_history.hpp"
namespace hanmo::resident {
struct OriginalProductionLaborContext {
 const PopulationLaborHistoryContext& labor;
 const OriginalProductionSupplyContext& supply;
 const OrganizationTrainingSourceContext& training;
 const SpatialCarrierState& carriers;const MovementExactState& movement;
 const SpatialCarrierSources& spatial;
};
struct OriginalProductionLaborInput {
 Integer labor_rate_numerator=0,labor_rate_denominator=1;
 std::vector<StableId> actual_labor_allocation_refs;
 std::vector<SourceRef> participation_refs,actual_supply_refs;
 Location actual_anchor;
 std::optional<WorldMoment> next_rule_boundary;
};
Outcome<OriginalProductionLaborInput> read_original_production_labor_input(
 const OriginalProductionLaborContext&,const StableId& unit,const WorldMoment& actual_adoption,
 const SourceRef& actual_cut,Integer fixed_denominator);
Status validate_original_production_labor_window(
 const OriginalProductionLaborContext&,const StableId& unit,const WorldMoment& actual_adoption,
 const SourceRef& actual_cut,Integer fixed_denominator,const OriginalProductionLaborInput&,const MomentSpan&);
}
