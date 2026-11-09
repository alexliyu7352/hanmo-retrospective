// Borrow the original completed service and its real terminal publication.
// No second completion, relation delta, clock or FSL validation algorithm.
#pragma once
#include "growth_action_time_read.hpp"
namespace hanmo::resident {
struct GrowthWorkServiceSources {
 const CommonStore& common;const GrowthState& growth;const GrowthCatalog& catalog;
 const CommonCatalog& shared;const ActionElapsedStore& elapsed;const TimeScaleRef& scale;
 const CatalogIdentity& identity;const std::vector<ActionTimeRegistration>& registrations;
 ExactArithmeticBudget budget{};
 const InteractionState* interaction=nullptr;const InteractionCatalog* interaction_catalog=nullptr;
};
struct OriginalCompletedWorkService {
 const LifeActivityRecord* activity=nullptr;
 const GrowthTerminalPublication* publication=nullptr;
 const FactRecord* actual_header=nullptr;
};
// Pure identity/source availability gate before any original completion writes.
Status validate_original_work_service_completion_candidate(
 const GrowthWorkServiceSources&,const StableId& action,const WorldMoment& actual_now);
Outcome<OriginalCompletedWorkService> read_original_completed_work_service(
 const GrowthWorkServiceSources&,const SourceRef& publication,Integer schema,
 const WorldMoment& actual_now);
Outcome<OriginalCompletedWorkService> read_original_work_service_at_terminal_boundary(
 const GrowthWorkServiceSources&,const SourceRef& boundary,Integer schema,
 const WorldMoment& actual_now);
}
