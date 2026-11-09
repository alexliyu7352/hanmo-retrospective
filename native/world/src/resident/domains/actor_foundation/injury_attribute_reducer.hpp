// Original 03 §3.1 arithmetic, with §29's separately retained injury terms.
// This pure evaluator is not a source reader, World admission, or body setter.
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
namespace hanmo::resident {
struct OriginalAttributePointInputs {
 Integer base=0,permanent_growth=0;
 Integer temporary_points=0;
 Integer permanent_injury_points=0,recoverable_injury_points=0;
};
// All terms are in original attribute points. Positive injury terms are
// deductions. Clamp once after composition, never between contributions.
// Callers must separately prove the complete actual source set and its cut.
Outcome<Integer> evaluate_original_attribute_points(const OriginalAttributePointInputs&);
// Same formula for the existing five-field value type. These are mathematical
// inputs, not caller authority to create/replace original contributions.
Outcome<ActorAttributes> evaluate_original_attribute_points(
 const ActorAttributes& base,const ActorAttributes& permanent_growth,
 const ActorAttributes& temporary_points,const ActorAttributes& permanent_injury_points,
 const ActorAttributes& recoverable_injury_points);
// Finite compiled name for the original formula, not an authored algorithm.
// Authored amount may be any nonnegative representable Integer; no default
// amount or probabilistic injury selection is supplied here.
Status validate_original_injury_attribute_reducer(
 const InjuryAttributeReduction&,const CatalogIdentity&);
}
