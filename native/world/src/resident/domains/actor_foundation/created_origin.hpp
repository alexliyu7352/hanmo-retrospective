// Actual original character creation adopts Origin/2; no fabricated birthday.
#pragma once
#include "authored_origin.hpp"
#include "resident/domains/family/character_creation_source.hpp"
#include "resident/kernel/original_calendar.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct CharacterCreationV2Write {
 AuthoredActorFoundationState& state;
 const ActorFoundationCatalog& catalog;
 const OriginalCalendarRead& calendar;
 const CurrentMomentRead& clock;
};
struct CreatedOriginView {
 const CharacterOriginRecordV2* origin=nullptr;
 const RegistrationSourceV2* registration=nullptr;
 const CreatedRegistrationSource* creation=nullptr;
 const InitialConditionSeed* condition=nullptr;
 const CharacterCreationAppearance* appearance=nullptr;
};
// Does not traverse Condition receipts or call capability readers. Independent
// draft/author/identity/registration inputs are established before seed replay.
Outcome<CreatedOriginView> read_created_character_origin_v2(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const FamilyCatalog&,const CommonCatalog&,const StableId&,const WorldMoment&);
// Called only inside the original successful initial-creation candidate, after
// the actor, private resources and its actual actor_creation head are applied.
Status record_created_character_origin_in_candidate(CommonStore&,CharacterCreationV2Write&,const FamilyCatalog&,const CommonCatalog&,const StableId& registration_id,const StableId& actor_id,const CharacterCreationDraft&);
}
namespace hanmo::resident {
class BornCharacterSourceRead;class CharacterAvailabilityProvenanceSink;struct CommonActualFactPrefix;
struct CharacterInitialValuesV2 {
 const CharacterOriginRecordV2* origin=nullptr;
 const ActorAttributes* base_attributes=nullptr;
 const ActorSkills* initial_skills=nullptr;
 const InitialConditionSeed* condition=nullptr;
 bool alive=false,available=false;
};
Outcome<CharacterInitialValuesV2> read_character_initial_values_v2(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const FamilyCatalog*,const CommonCatalog*,const StableId&,const WorldMoment&,const BornCharacterSourceRead* born_sources=nullptr);
struct CharacterInitialValuesAtFactV2 {CharacterInitialValuesV2 initial;bool present=false;};
// The business owner first validates its actual boundary payload/M. This
// shared source-only leg proves Origin2 and the real Common append cut, without
// reading current skills as historical qualifications or traversing Death4.
Outcome<CharacterInitialValuesAtFactV2> read_character_initial_values_before_fact_v2(
 const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,
 const FamilyCatalog*,const CommonCatalog*,const StableId& actor,
 const SourceRef& actual_boundary,const WorldMoment& actual_at,const WorldMoment& actual_now,
 const BornCharacterSourceRead* born_sources=nullptr,
 CharacterAvailabilityProvenanceSink* provenance=nullptr);
// Actual-applied state query: nested has no claimed WorldMoment. The genuine
// outer endpoint alone pairs its Common prefix with outer_at. Returned values
// borrow the original roots, not either prefix or any local source view.
Outcome<CharacterInitialValuesAtFactV2> read_character_initial_values_at_applied_prefix_v2(
 const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,
 const FamilyCatalog*,const CommonCatalog*,const StableId& actor,
 const CommonActualFactPrefix& nested,const CommonActualFactPrefix& outer,
 const WorldMoment& outer_at,const WorldMoment& actual_now,
 const BornCharacterSourceRead* born_sources=nullptr);
struct CharacterInitialPlacementV2 {StableId actor_id;Location location;WorldMoment created_at;SourceRef actual_creation_source;Integer source_schema=0;};
Outcome<CharacterInitialPlacementV2> read_created_character_initial_placement_v2(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const FamilyCatalog&,const CommonCatalog&,const StableId&,const WorldMoment&);
}

namespace hanmo::resident {
Outcome<CharacterCreationMode> read_character_creation_mode_v2(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const FamilyState&,const FamilyCatalog&,const CommonCatalog&,const WorldMoment&);
}
