// Original Birth4 character creation stage. Only the enclosing due candidate may
// publish after its actual S1/S5/care/load results have all been adopted.
#pragma once
#include "born_character_source.hpp"
#include "family_birth_creation.hpp"
#include "resident/domains/actor_foundation/born_origin.hpp"
#include "resident/domains/actor_foundation/condition_capture.hpp"
#include "resident/domains/travel/birth_placement.hpp"
namespace hanmo::resident {
class CharacterConditionWorldRead;struct GrowthPermanentState;
struct BornCharacterCreationContext {
 CommonStore& common;const CommonCatalog& common_catalog;
 const FamilyCatalog& family_catalog;const FamilyLineageCatalog& lineage;
 const InteractionCatalog& interactions;const BirthArrangementCatalog& catalog;
 FamilyBirthState& births;InteractionState& interaction;SocialState& social;
 AuthoredActorFoundationState& foundation;const ActorFoundationCatalog& foundation_catalog;
 const BirthArrangementAccess& access;BirthPlacementWrite& placement;
 const CurrentMomentRead& clock;const ReadStampAuthority& stamps;
 const DomainReadVersionSource& versions;
 GrowthPermanentState* permanent_growth=nullptr;
 const CharacterConditionWorldRead* condition_world=nullptr;
};
struct BornCharacterCreated {
 BirthFactRef birth_ref;StableId child_actor_id;
 MomentOwnerSource actual_birth_source,location_result;
 std::optional<StableId> transport_binding_ref;
 BoundActorConditionCreation condition_creation;
 std::vector<BoundSpatialLocationWrite> location_writes;
};
// Fresh stage only. Re-entry is handled by the outer immutable due result before
// this call. An incomplete earlier stage is an error, never sampled again.
Outcome<BornCharacterCreated> create_born_character_at_actual_placement_in_candidate(
 BornCharacterCreationContext&,const ScheduledBirthSource&,const ReadStamp&);
// Stable original identity + actual S1 result, without population/care recursion.
Status validate_born_character_identity_and_placement(const BornCharacterSourceContext&,
 const AuthoredActorFoundationState&,const ActorFoundationCatalog&,
 const BirthPlacementRead&,const BirthFactRef&);
}
