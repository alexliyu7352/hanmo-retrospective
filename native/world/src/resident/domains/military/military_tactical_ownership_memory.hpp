#pragma once
#include "military_tactical_ownership_model.hpp"
namespace hanmo::resident {
class CheckpointRetentionCensus;
struct MilitaryTacticalFormationBuildView3;struct MilitaryTacticalMissionBuildView3;
struct MilitaryAcceptedDirectIntentBuildView3;struct DirectForceIntentReceiptBuildView3;
// Implemented by the same original field sequences as the owned-row overloads.
// Build views carry borrowed final buffers; they never retroactively charge them.


// Raw payload traversal, not encoding or a sizeof/serialized-size estimate.
// The actual owning World/candidate holds the original copy reservation.


}
