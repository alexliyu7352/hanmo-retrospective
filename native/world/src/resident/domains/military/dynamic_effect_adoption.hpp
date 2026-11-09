// The fixed session current is the only tactical balance/status/RNG writer.
#pragma once
#include "dynamic_effect.hpp"
namespace hanmo::resident {
struct DynamicLandCurrentAdoption {
 SourceRef effect_group_source;
 std::vector<SourceRef> actual_posture_closures;
 bool already_applied=false;
};
// Called by the complete same-M producer after storing its real calculated group.
// This is the A/E current-adoption leg, not permission to publish a half group.
// The original F roster/condition/population/CLC barrier remains mandatory.
Outcome<DynamicLandCurrentAdoption> adopt_dynamic_land_effect_current_in_candidate(
 CommonStore&,const MilitaryState&,MilitaryExactState&,
 const MilitaryBattleCatalog&,const DynamicMilitaryActionCatalog2&,
 const DynamicMilitaryEffectOriginalRead&,const DynamicMilitarySessionOriginalRead&,
 const CurrentMomentRead&,const SourceRef& actual_group,ExactArithmeticBudget={});
}
