#pragma once
#include "resident/kernel/exact_phase_graph.hpp"
#include "social_calendar_phase.hpp"
namespace hanmo::resident {
// initial_tick is the selected scenario's real installation boundary, never
// inferred from current time.
Status validate_original_social_phase_selection(const WorldBundle &,
                                                Integer initial_tick);
Status validate_original_social_phase_sources(const WorldBundle &,
                                              const WorldMoment &,
                                              Integer initial_tick);
Status apply_original_social_calendar_stage(WorldBundle &, ExactStage,
                                            const ExactPeriodContext &,
                                            ExactPhaseWriteContext &,
                                            SocialCalendarDispatchFunction);
ExactPhaseRegistration make_social_exact_phase_registration(
    Integer initial_tick,
    SocialCalendarDispatchFunction selected_dispatch = nullptr);
} // namespace hanmo::resident
