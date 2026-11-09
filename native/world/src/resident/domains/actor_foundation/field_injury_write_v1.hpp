// Actual field casualty source -> single candidate body mutation + original
// Condition/source1/episode consequences. Failure requires whole-World
// rollback.
#pragma once
#include "condition_capture.hpp"
#include "injury_sources_v1.hpp"
namespace hanmo::resident {
struct CharacterInjuryReplaySourcesV1;
class FieldSegmentApplicationRead;
struct GrowthPermanentState;
class GrowthCapabilityRevisionWrite;
Outcome<BoundActorConditionWrite> apply_field_segment_injury_in_candidate_v1(
    CommonStore &, AuthoredActorFoundationState &,
    CharacterInjurySourcesStateV1 &, const CharacterInjuryReplaySourcesV1 &,
    const SourceRef &casualty_source, const StableId &actor_id,
    const FieldSegmentApplicationRead &, const GrowthPermanentState &,
    GrowthCapabilityRevisionWrite &);
} // namespace hanmo::resident
