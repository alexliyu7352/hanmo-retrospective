// S6 sole line writer. All geometry, actual positive movement and final-boundary
// completion remain with S1; scope/cargo successors remain with original owners.
#pragma once
#include "safe_exit_decision_source.hpp"
#include "safe_exit_window.hpp"
namespace hanmo::resident {
// Source-only path/decision/physical-completion leg for the original retained
// scope validator. It does not certify population/cargo partition or lineage.
Status validate_safe_exit_original_path_progress(const SafeExitEntryContext&,
 const SafeExitDecisionSourceContext&,const SafeExitBinding&,
 const OrderedTable<SafeExitLine>&);
// Dispatches the original ordinary entry vs an existing binding's actual exit.
// Called at the trusted M_to only after S1's positive prefix/project succeeds.
// S1 then installs the returned back-reference and runs its full cold consumer.
Outcome<std::optional<SourceRef>> adopt_safe_exit_political_crossing_in_candidate(
 CommonStore&,SafeExitState&,const SafeExitEntryContext&,
 const SafeExitDecisionSourceContext&,const SourceRef& actual_crossing);
// Called after the true S1 close/arrival at its actual M. A first positive prefix
// alone cannot mark outside. Replays read the same immutable completion source.
Status adopt_safe_exit_boundary_completion_in_candidate(CommonStore&,SafeExitState&,
 const SafeExitEntryContext&,const SafeExitDecisionSourceContext&,
 const SourceRef& actual_crossing);
}
