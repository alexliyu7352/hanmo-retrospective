// 当前完整候选中的政治签发、本人受命、营内交接三门合一，不因死亡自发授予指挥权。
#pragma once
#include "military_officer_source.hpp"
namespace hanmo::resident {
Outcome<MilitaryOfficerAssignmentQuote> preview_military_officer_assignment(
    const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId&,const MilitaryOfficerAssign&);
Outcome<MilitaryEffect> apply_military_officer_assignment(
    CommonWrite&,MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const StableId&,const MilitaryOfficerAssign&);
bool military_officer_effect_shape(const MilitaryEffect&);
Status validate_military_officer_cached_effect(const MilitaryState&,const MilitaryEffect&,const StableId& actor_id);
Status validate_military_officer_assignments(
    const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&);
}
