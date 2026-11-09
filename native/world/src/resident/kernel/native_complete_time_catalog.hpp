// Root-owned preparation candidate: one immutable K2 runtime, no second registry/state.
#pragma once
#include "exact_time_profile.hpp"
#include "resident/domains/tasks/work_execution/work_exact_runtime.hpp"
#include "resident/domains/social/social_calendar_phase.hpp"
namespace hanmo::resident {
struct PreparedCatalog;
// Actual old-writer route selection comes from the same original central dispatcher.
// This input is not authored data and must not be inferred from missing handlers.
// Required root scheduler read, not implemented by a domain or author profile.
Outcome<std::vector<StableId>> read_native_complete_selected_legacy_writer_keys(const PreparedCatalog&);
Outcome<std::vector<FacilityHolderRegistration>> read_native_complete_facility_holder_registrations(const PreparedCatalog&);
Outcome<TaskWorkWorldFactory> read_native_complete_task_work_factory(const PreparedCatalog&);
Outcome<SocialCalendarDispatchFunction> read_native_complete_social_dispatch(const PreparedCatalog&);
Status prepare_native_complete_exact_runtime(PreparedCatalog&,const std::vector<StableId>& selected_legacy_writer_keys,const std::vector<FacilityHolderRegistration>& compiled_facility_holders,TaskWorkWorldFactory,SocialCalendarDispatchFunction);
}
