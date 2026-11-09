// Original single-formation exit application. No world placement mutation.
#pragma once
#include "field_exit_projection.hpp"
namespace hanmo::resident {
struct MilitaryState;
class FieldSegmentApplicationRead;
class CheckpointMemoryCharge;
struct FieldRoadExitSourceRead {
  const FieldBattleExitRecord *application = nullptr;
  FieldRouteExitProjectionRead projection;
};
Outcome<FieldRoadExitSourceRead> read_field_road_exit_source(
    const CommonStore &, const MilitaryState &, const FieldBattleWorldState &,
    const TravelCatalog &, const FieldBattleCatalog &, const SourceRef &,
    const FieldSegmentApplicationRead &, ExactArithmeticBudget = {},
    const FieldExactContactReadContext *exact_contacts = nullptr);
// Road/off-road branch only. Gate/village settlement branches have their own
// actual P3-qualified origin and cannot call this as a destination fallback.
// The caller must consume the returned original application through the S1
// same-candidate placement/capture writer and original continuation before
// publishing the World. Failure discards the whole candidate.
Outcome<SourceRef> record_field_road_exit_in_candidate(
    CommonStore &, const MilitaryState &, FieldBattleWorldState &,
    const TravelCatalog &, const FieldBattleCatalog &, const StableId &battle,
    const StableId &formation, const FieldSegmentApplicationRead &,
    CheckpointMemoryCharge &, ExactArithmeticBudget = {},
    const FieldExactContactReadContext *exact_contacts = nullptr);
} // namespace hanmo::resident
