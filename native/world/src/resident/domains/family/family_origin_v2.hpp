#pragma once
#include "resident/domains/actor_foundation/created_origin.hpp"
namespace hanmo::resident {
// Borrowed original registration sources and trusted phase clock, not a flag to
// skip Family validation. Existing asset, succession and consent validators run.
struct FamilyOriginV2Sources {
 const AuthoredActorFoundationState& foundation;
 const ActorFoundationCatalog& catalog;
 const CurrentMomentRead& clock;
};
Status validate_family_identity_v2(const CommonStore&,const FamilyState&,const FamilyCatalog&,
 const CommonCatalog&,const ActorRecord&,const FamilyOriginV2Sources&);
Status validate_family_creation_sources_v2(const CommonStore&,const FamilyState&,const FamilyCatalog&,
 const CommonCatalog&,const FamilyOriginV2Sources&);
Status validate_family_founder_v2(const CommonStore&,const FamilyState&,const FamilyCatalog&,
 const CommonCatalog&,const FamilyOriginV2Sources&);
}
