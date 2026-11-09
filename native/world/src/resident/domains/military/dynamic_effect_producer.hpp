// Complete same-M original land group. The enclosing World owns rollback.
#pragma once
#include "dynamic_effect_adoption.hpp"
#include "dynamic_effect_source.hpp"
#include "dynamic_condition_consumer.hpp"
#include "dynamic_objective.hpp"
#include "population_military.hpp"
#include "resident/domains/travel/spatial_location_capture.hpp"
namespace hanmo::resident {
struct DynamicMilitaryNamedConditionContext {
 AuthoredActorFoundationState& foundation;FamilyDeathState& deaths;
 const ActorFoundationCatalog& catalog;const FamilyLifeCauseSources4& life;
 const CurrentReadStampRead& reads;ReadStampAuthority& stamps;
 const DomainReadVersionSource& versions;const CharacterConditionSources& originals;
 GrowthCapabilityRevisionWrite* capability_changes=nullptr;
 // Same-world immediate Life -> Care -> Need consumer; selected Care5/6 requires it.
 CareLifeConditionWrite* care_life=nullptr;
};
struct DynamicMilitaryParticipationApplication {
 std::vector<SourceRef> actual_terminal_refs;
 std::vector<BoundSpatialLocationWrite> actual_spatial_writes;
};
// S1/CLC/custody original consumers own actual participation termination. A
// selected complete composition must supply their real writer and cold reader.
class DynamicMilitaryParticipationConsequenceWrite {
public:virtual ~DynamicMilitaryParticipationConsequenceWrite()=default;
 virtual bool bound_to(const CommonStore&,const MilitaryState&,const MilitaryExactState&)const noexcept=0;
 virtual Outcome<DynamicMilitaryParticipationApplication> consume_actual_military_casualty_in_candidate(const MomentOwnerSource&)=0;
};
struct DynamicMilitaryGroupWriteContext {
 const DynamicMilitaryActionContext& actions;const MilitaryBattleCatalog& battle_catalog;
 const DynamicMilitaryEffectOriginalRead& originals;
 const DynamicMilitaryLandConsequenceContext& land;
 DynamicMilitaryNamedConditionContext& named;
 DynamicMilitaryParticipationConsequenceWrite& participation;
 const DynamicMilitaryObjectiveOriginalRead& objectives;
 PopulationMilitaryWriteContext* population=nullptr;
};
struct DynamicMilitaryGroupApplication {
 std::optional<MomentOwnerSource> effect_group,battle,casualty,objective,terminal;
 std::vector<MomentOwnerSource> actual_recovery_sources;
 DynamicMilitaryConditionApplication named;
 std::vector<SourceRef> population_sources;
 std::vector<BoundSpatialLocationWrite> actual_spatial_writes;
 bool already_applied=false;
};
// The real S0 prefix and expiry stage must already be settled to this M. The
// finite due set is read from original actions here, never caller supplied.
// The original reader reconstructs the before cut of the just-appended group
// and its actual due refs/once keys; it may not substitute a mutable current
// snapshot for a historical group. All calculations precede any roster/body
// writes. The function cannot publish a half group on a later consumer failure.
Outcome<DynamicMilitaryGroupApplication> apply_dynamic_land_effect_group_in_candidate(
 CommonStore&,MilitaryState&,MilitaryExactState&,const DynamicMilitaryGroupWriteContext&,
 const StableId& session,Integer generation);
}
