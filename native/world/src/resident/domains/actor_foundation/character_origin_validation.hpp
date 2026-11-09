// Complete Origin2 coverage, independent of later Condition/FSL effects.
#pragma once
#include "created_origin.hpp"
namespace hanmo::resident {
Status validate_complete_character_origin_sources(const CommonStore&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const FamilyCatalog*,const CommonCatalog*,const WorldMoment& actual_now,const BornCharacterSourceRead* born_sources=nullptr);
}
