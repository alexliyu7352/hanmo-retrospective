// v1b Origin2: the original Birth result owns sampling; this one immutable
// allocation owns its actual initial base attributes and skill XP.
#pragma once
#include "created_origin.hpp"
#include "resident/domains/family/born_character_source.hpp"
namespace hanmo::resident {
struct BornOriginView {
 const CharacterOriginRecordV2* origin=nullptr;
 const InitialCapabilityAllocation* allocation=nullptr;
 const InitialConditionSeed* condition=nullptr;
 BornCharacterSourceView original;
};
Outcome<BornOriginView> read_born_character_origin_v2(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const BornCharacterSourceRead&,const StableId& actor,const WorldMoment& actual_now);
// Original child and its two Common source heads already exist. Called once in
// that same private candidate before dependent placement/population/care. A
// later failure discards all these writes with the original child and draws.
Status record_born_character_origin_in_candidate(CommonStore&,AuthoredActorFoundationState&,const ActorFoundationCatalog&,const BornCharacterSourceRead&,const BirthFactRef&,const CurrentMomentRead&);
}
