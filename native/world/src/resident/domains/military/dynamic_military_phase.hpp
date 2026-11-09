#pragma once
#include "dynamic_effect_producer.hpp"
#include "dynamic_session_due.hpp"
#include "dynamic_verified_source.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
// The one military provider delegates this finite dynamic land sub-leaf. It is
// not another provider registration and does not enable a missing Naval owner.
// Prediction coalesces an actual M, naming an original retained source. The
// writer closes every expiry at M before any recovery/effect before-snapshot,
// then judges the remaining original no-attack objective gates.
Outcome<std::vector<ExactEventPrediction>> predict_exact_dynamic_military_events(
 const CommonStore&,const MilitaryState&,const MilitaryExactState&,
 const DynamicMilitaryVerifiedSourceContext&,const WorldMoment& current,
 const WorldMoment& upper);
// The enclosing military/S1 interval owner has already selected the earliest
// actual motion/collision bound and supplied the original geometric prefix.
// Only original tactical prepare/recovery service is written here, with the
// complete source-proven participation for each positive span.
Status provide_exact_dynamic_military_tactical_interval(const CommonStore&,
 MilitaryExactState&,const DynamicMilitaryActionContext&,const MomentSpan&);
Outcome<ExactEventApplied> apply_exact_dynamic_military_event(CommonStore&,
 MilitaryState&,MilitaryExactState&,const DynamicMilitaryGroupWriteContext&,
 const ExactEventPrediction&,ExactPhaseWriteContext&);
// Full retained tactical/session/effect/objective source closure for this leaf.
// Original spatial, CLC, duty and other selected military owners remain their
// own mandatory validators; effect-consumer coverage delegates to real owners.
Status validate_exact_dynamic_military_phase_sources(const CommonStore&,
 const MilitaryState&,const MilitaryExactState&,
 const DynamicMilitaryVerifiedSourceContext&,const WorldMoment&);
Status validate_exact_dynamic_military_applied_source(const CommonStore&,
 const MilitaryState&,const MilitaryExactState&,
 const DynamicMilitaryVerifiedSourceContext&,const ExactEventApplied&);
}
