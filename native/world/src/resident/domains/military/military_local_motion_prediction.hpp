#pragma once
#include "dynamic_session.hpp"
namespace hanmo::resident {
struct MilitaryLocalMotionPredictionContext {
    const CommonStore& common;
    const MilitaryState& military;
    const MilitaryExactState& exact;
    const MovementExactState& movement;
    const DynamicMilitarySessionOriginalRead& sessions;
    const MilitarySpatialOriginalSourceRead& spatial;
    const MilitaryLocalMotionOriginalRead& local_motion;
    const CurrentMomentRead& clock;
    ExactArithmeticBudget budget={};
};
struct MilitaryLocalBodyMotionPrediction {
    const DynamicMilitaryBodyTacticalState* actual_body=nullptr;
    const MilitarySpatialHandoff2* actual_handoff=nullptr;
    const MilitarySpatialChart2* actual_chart=nullptr;
    LocalMotionLinearPrediction motion;
};
struct MilitaryLocalSessionMotionPrediction {
    const DynamicMilitarySessionCurrent2* actual_session=nullptr;
    // All returned bodies share this positive prediction window. The earliest
    // real edge due bounds every body, including genuinely held/blocked ones.
    MomentSpan common_window;
    std::vector<MilitaryLocalBodyMotionPrediction> bodies;
};
// Reads the actual complete session body set; the caller cannot pick just the
// moving bodies, supply centers, or omit a stationary collision obstacle.
// Real entry/heading/binding and local original sources remain mandatory.
// Borrowed rows expire on any mutation of their original owners. A result is
// not a contact, a complete NoContact witness, or an interval adoption token.
Outcome<MilitaryLocalSessionMotionPrediction> predict_actual_military_session_motion(
    const MilitaryLocalMotionPredictionContext&,const StableId& session,
    Integer generation,const WorldMoment& requested_upper);
}
