// Original S6 adopted Decision -> original SafeExit entry/path source bridge.
// It reads the existing decision; it does not create another permit or proof.
#pragma once
#include "political_passage_source.hpp"
#include "safe_exit_entry.hpp"
namespace hanmo::resident {
struct SafeExitDecisionSourceContext {
 const CommonStore& common;const PoliticalPassageDecisionState& decisions;
 const PoliticalPassageEntryRead& entries;const PoliticalPassageHistoryRead& history;
};
// Source-only leg for the original history validator. Entry/source/moment are
// checked; policy validity must be checked by the full historical owner once.
Outcome<const PoliticalPassageDecisionSource*> read_safe_exit_crossing_decision_source(
 const SafeExitDecisionSourceContext&,const BoundaryCrossingReceipt&);
Outcome<const PoliticalPassageDecisionSource*> read_safe_exit_crossing_decision(
 const SafeExitDecisionSourceContext&,const BoundaryCrossingReceipt&);
// Null is only an explicitly original non-treaty passage or an already-bound
// exit. A legacy generic treaty reference is not enough to select a term.
Outcome<std::optional<SafeExitEntryTerm>> read_safe_exit_entry_term_from_actual_decision(
 const SafeExitDecisionSourceContext&,const BoundaryCrossingReceipt&);
}
