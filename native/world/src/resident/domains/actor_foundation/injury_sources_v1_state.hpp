// World根只在显式新游戏初始化持有；冷读不得拿空状态补丢失源。
#pragma once
#include "injury_sources_v1.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
namespace hanmo::resident {
Status validate_character_injury_sources_v1_layout(
    const CatalogIdentity &, const CharacterInjurySourcesStateV1 *);
Outcome<CharacterInjurySourcesStateV1>
make_initial_character_injury_sources_v1(const CommonStore &,
                                         const AuthoredActorFoundationState &,
                                         const ActorFoundationCatalog &);
} // namespace hanmo::resident
