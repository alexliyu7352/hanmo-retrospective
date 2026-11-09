// Structural reverse coverage only; original-source rules remain separately required.
#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
class CommonStore;
struct AuthoredActorFoundationState;
struct ActorFoundationCatalog;
struct CharacterInjurySourcesStateV1;
struct BoundActorConditionWrite;
// Checks the paired selected profile, true Common application order, bidirectional
// receipt links and finite original business once keys. Does NOT authenticate raw
// battle/service outcomes, earned-point replay, episode effects or effective M.
// A successful return is not full LIFE-5 validation or permission to publish.
Status validate_character_injury_source_links_v1(
    const CommonStore&, const AuthoredActorFoundationState&,
    const ActorFoundationCatalog&, const CharacterInjurySourcesStateV1&);
// Hot post-bind check, while the original Common capture is still live. The
// caller owns atomic rollback of the whole candidate on any failure. No actor
// setter or source append is performed here; raw-source admission remains the
// original writer's obligation, before it mutates the candidate.
Status validate_character_injury_bound_source_link_v1(
    const CommonStore&, const AuthoredActorFoundationState&,
    const ActorFoundationCatalog&, const CharacterInjurySourcesStateV1&,
    const BoundActorConditionWrite&);
} // namespace hanmo::resident
