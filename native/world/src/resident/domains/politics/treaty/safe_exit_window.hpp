// P21.1 exact deadline projection from actual original sources. No extra timer.
#pragma once
#include "safe_exit_entry.hpp"
namespace hanmo::resident {
struct SafeExitWindowView {
 // Natural passage expiry cites the actual activation + locked term window.
 // An earlier true termination cites the original termination source instead.
 SourceRef termination_source;WorldMoment termination_at,from,until;
 std::optional<WorldMoment> first_actual_exit_entry;
};
// Original activation/term/termination leg, also used in the first positive
// adoption before S1 can write the returned binding back-reference.
Outcome<std::optional<SafeExitWindowView>> read_safe_exit_original_termination_window(
 const TreatyTerminationContext&,const SourceRef& actual_activation,const StableId& term_id,
 const OrganizationAuthorityClock&,ExactArithmeticBudget={});
// Original time/source projection only. It does not certify scope, lineage or
// policy. The original progress validator uses this leg without calling itself.
Outcome<std::optional<SafeExitWindowView>> read_safe_exit_window_sources(
 const SafeExitEntryContext&,const SourceRef& binding_source);
// Mechanical projection after an actual crossing/termination writer. The
// calling candidate must still run full scope/history validation before publish.
Status project_safe_exit_window_sources_in_candidate(CommonStore&,SafeExitState&,
 const SafeExitEntryContext&,const SourceRef& binding_source);
// Null before objective termination. Actual source history and full retained
// lineage/crossing bindings are checked; no present-day manifest substitution.
Outcome<std::optional<SafeExitWindowView>> read_safe_exit_window(
 const SafeExitEntryContext&,const SourceRef& binding_source);
Status validate_safe_exit_window_projection(const SafeExitEntryContext&,
 const SourceRef& binding_source);
// Only for an actual S6 original termination/expiry or actual crossing candidate.
// Uses the trusted current clock, allocates no Fact and never extends a window.
// A caller must discard the entire candidate on failure.
Status adopt_safe_exit_original_deadlines_in_candidate(CommonStore&,SafeExitState&,
 const SafeExitEntryContext&);
}
