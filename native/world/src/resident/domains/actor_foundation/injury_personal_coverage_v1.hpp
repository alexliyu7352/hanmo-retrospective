// Complete original Personal terminal/participant reverse coverage for source1.
#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct CharacterConditionSources; struct CharacterInjurySourcesStateV1;
struct PersonalState; struct PersonalCatalog;
// Every genuine Personal result writes HP/fatigue for each participant, even
// incoming injury0. Proves exactly one linked source1 application per original
// result+actor and authenticates every cited Personal result. Does not replace
// full progress-prefix/episode/effective-attribute validation or a hot writer.
Status validate_personal_injury_source_coverage_v1(
 const CharacterConditionSources&, const CharacterInjurySourcesStateV1&,
 const PersonalState&, const PersonalCatalog&, Integer selected_lifecycle);
}
