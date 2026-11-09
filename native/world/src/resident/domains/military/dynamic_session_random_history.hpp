#pragma once
#include "military_exact_sources.hpp"
namespace hanmo::resident {
// Source-local reconstruction from the retained, uncompressed original rows.
// The boundary is an actual DecisionCommitted or effect-group Common head, not
// a caller-selected time. Later same-M commits are excluded. This does not call
// the professional effect/session validators or make a new business draw.
Outcome<DynamicMilitarySessionRandom> read_dynamic_military_random_before_source(
 const CommonStore&,const MilitaryExactState&,const SourceRef& actual_commit,
 const WorldMoment& actual_now,ExactArithmeticBudget={});
// Stable publication only: every retained original commit is accounted for.
// A future compressed-source profile needs its own real summary producer/read;
// a missing ordinal chain is not reconstructed from today's mutable stream.
Status validate_retained_dynamic_military_random_history(const CommonStore&,
 const MilitaryExactState&,const DynamicMilitarySessionCurrent2&,
 const WorldMoment& actual_now,ExactArithmeticBudget={});
}
