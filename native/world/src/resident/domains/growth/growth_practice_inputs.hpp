// Actual Practice award adoption into every active original FSL skill input.
#pragma once
#include "growth_permanent_contributions.hpp"
#include "growth_fixed_execution.hpp"
namespace hanmo::resident {
Status adopt_growth_practice_inputs_in_candidate(CommonStore&,GrowthState&,
 const CharacterConditionSources&,ActionElapsedStore&,const PracticeAwardRef&);
// Reverse coverage includes same-M source cuts and original terminal publication.
Status validate_growth_practice_input_coverage(const CharacterConditionSources&,const GrowthState&);
}
