#pragma once
#include "military_lifecycle.hpp"
namespace hanmo::resident {
struct OriginalArmyResupplyCompletion {bool success=false;MilitaryLogisticsResult result;};
bool same_original_logistics_terms(const MilitaryLogisticsTerms&,const MilitaryLogisticsTerms&);
Status validate_original_army_resupply_escrow(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId& action_id,const MilitaryLogisticsTerms&);
Status reserve_original_army_resupply_in_candidate(CommonWrite&,const StableId& action_id,const MilitaryLogisticsTerms&);
Outcome<OriginalArmyResupplyCompletion> complete_original_army_resupply_in_candidate(CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const ActionCoreView&,const MilitaryLogisticsTerms&);
}
