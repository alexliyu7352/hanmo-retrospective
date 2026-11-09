// Original Growth validation selected by the same immutable catalog profile.
#pragma once
#include "resident/domains/growth/study_pending_release.hpp"
namespace hanmo::resident {
struct WorldBundle;
// Published/restore validation never accepts a private release witness.
Status validate_selected_growth_state(const WorldBundle&);
// The caller retains these actual writer-issued proofs only until capture ends.
Status validate_pending_growth_releases(const WorldBundle&,const StudyPendingReleaseProofs&);
}
