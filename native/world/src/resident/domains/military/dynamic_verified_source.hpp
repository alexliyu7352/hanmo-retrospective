#pragma once
#include "dynamic_effect.hpp"
namespace hanmo::resident {
class DynamicMilitaryObjectiveOriginalRead;
struct MilitaryTacticalOwnershipContext3;
// One same-world read lease. The selected dynamic_military/2 owner supplies its
// real professional readers; none of these dependencies is an optional success.
struct DynamicMilitaryVerifiedSourceContext {
    const DynamicMilitaryActionContext& actions;
    const MilitaryBattleCatalog& battle_catalog;
    const DynamicMilitaryEffectOriginalRead& effects;
    const DynamicMilitaryLandConsequenceContext& land;
    const DynamicMilitaryObjectiveOriginalRead* objectives=nullptr;
    // Required for the explicit /3 original formation/mission/accepted source
    // kinds. This is not a selector and cannot silently validate missing rows.
    const MilitaryTacticalOwnershipContext3* tactical_ownership=nullptr;
};
// Finite original source registry, independent of the physical section version.
// Unknown kinds fail; technical sources do not imply a public battle report.
Outcome<Integer> read_dynamic_military_source_schema(const SourceRef&);
Outcome<WorldMoment> read_verified_dynamic_military_source_moment(
    const CommonStore&,const MilitaryState&,const MilitaryExactState&,
    const DynamicMilitaryVerifiedSourceContext&,const MomentOwnerSource&);
}
