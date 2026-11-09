// Original S5 Calendar step: one frozen opening input, one existing last_step_tick.
#pragma once
#include "economic_work_sources.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "resident/kernel/action_time_validation.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/organization/organization_activity_history.hpp"
namespace hanmo::resident {
struct EconomicCalendarReadContext {
 const CommonStore& common;const LocalEconomyState& economy;const EconomicWorkSourcesState& sources;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& foundation_catalog;
 const OrganizationState& organization;const OrganizationActivityOpeningRead& organization_history;
 const std::vector<ActionTimeRegistration>& action_registrations;const ActionTimeOriginalSourceRead& action_sources;
 const CurrentMomentRead& clock;
};
// Original identity/cursor closure. It deliberately borrows no Organization or
// ActionTime provider: those frozen inputs are validated only by the full path.
// This value may be built before body/ActionTime; all six actual roots must live
// through every synchronous read and any retained opening pointer use.
struct EconomicCalendarIdentityReadContext {
 const CommonStore& common;const LocalEconomyState& economy;const EconomicWorkSourcesState& sources;
 const AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& foundation_catalog;
 const CurrentMomentRead& clock;
};
EconomicCalendarIdentityReadContext economic_calendar_identity_context(const EconomicCalendarReadContext&)noexcept;
// Must be called beside the actual initial-local-economy construction, before any step.
// The supplied origin is resolved from the real selected ScenarioOrigin, never a new tick.
Status initialize_economic_calendar_origin_in_candidate(EconomicWorkSourcesState&,const EconomicCalendarReadContext&);
SourceRef economic_period_opening_source(const EconomicPeriodOpening&);
// Source identity/cursor/history only; original consumers may borrow this without
// recursively reading the frozen Action or Organization activity performance.
Outcome<const EconomicPeriodOpening*> read_economic_period_opening_identity(const EconomicCalendarIdentityReadContext&,const SourceRef&,Integer source_schema);
Outcome<const EconomicPeriodOpening*> read_economic_period_opening_identity(const EconomicCalendarReadContext&,const SourceRef&,Integer source_schema);
Outcome<const EconomicPeriodOpening*> read_economic_period_opening(const EconomicCalendarReadContext&,const SourceRef&,Integer source_schema);
Outcome<ExactPeriodOwnerSources> open_economic_calendar_period_in_candidate(CommonStore&,EconomicWorkSourcesState&,const EconomicCalendarReadContext&,const ExactPeriodContext&);
// Same original opening, with only actually expiring production inputs adopted.
Outcome<ExactPeriodOwnerSources> open_economic_production_calendar_period_in_candidate(CommonStore&,const LocalEconomyCatalog&,EconomicProductionContext&,const EconomicCalendarReadContext&,const ExactPeriodContext&);
Outcome<DynamicStepInput> read_economic_calendar_input(const EconomicCalendarReadContext&,const ExactPeriodContext&);
// This checks a real completed original step. No no-op closing fact is manufactured.
Outcome<ExactPeriodOwnerSources> close_economic_calendar_period(const EconomicCalendarReadContext&,const ExactPeriodContext&);
Outcome<ExactPeriodOwnerSources> read_economic_calendar_period_sources(const EconomicCalendarReadContext&,const ExactPeriodContext&);
Status validate_economic_calendar_sources(const EconomicCalendarReadContext&,const WorldMoment& actual_now);
// These dependencies must remain pinned while the opening is retained; they are original
// owner IDs, not duplicate activities/actions or a second current-state index.
struct EconomicCalendarOpeningDependencies {ScenarioOriginRef origin;std::vector<SourceRef> original_source_refs;std::vector<StableId> organization_activity_ids,action_ids;};
Outcome<EconomicCalendarOpeningDependencies> read_economic_calendar_opening_dependencies(const EconomicCalendarReadContext&,const SourceRef&);
}
