#pragma once
#include "governance_work_lifecycle_sources.hpp"
namespace hanmo::resident {
// Original deadline cause and optional actual WorkResult define the cut. The
// caller cannot supply an arbitrary historical boundary. All original Work,
// source-period coverage, accepted deadline and safe-stop checks remain.
Status validate_governance_work_deadline_stop_boundary(
    const GovernanceWorkLifecycleSourceContext &, const WorkEpisode &,
    const MomentOwnerSource &, const WorldMoment &, const ReadStamp &);
} // namespace hanmo::resident
