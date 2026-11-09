// Read the two existing training mutations from their original S6 sources.
// This is a derived qualification view, not another population or training log.
#pragma once
#include "organization_original_runtime.hpp"
#include "organization_calendar_admission.hpp"
namespace hanmo::resident {
struct OrganizationTrainingSourceContext {
 const CommonStore& common;const OrganizationState& organization;
 const OrganizationCatalog& catalog;const LocalEconomyState& economy;
 const OrganizationRuntime& runtime;const OrganizationAuthorityClock& clock;
 const OrganizationCalendarAdmissionRead& calendar;
};
struct OrganizationPopulationTrainingChange {
 MomentOwnerSource actual_source;WorldMoment occurred_at;
 StableId activity_id,cohort_id,allocation_id;
 PopulationStatus before_status=PopulationStatus::training,after_status=PopulationStatus::training;
 std::optional<StableId> before_assignment,after_assignment;
 SourceRef original_approval,original_transfer;
};
// The caller replays true Common order and chooses before/after its actual cut.
// This reader checks original authority, transfer, activity and calendar binding.
// It deliberately does not call the P02/production/whole-training validator back:
// completed training's actual participation/ration coverage remains the separate
// original S6 complete validator, consuming S5's historical evidence once.
Outcome<OrganizationPopulationTrainingChange> read_organization_population_training_change(
 const OrganizationTrainingSourceContext&,const MomentOwnerSource&);
}
