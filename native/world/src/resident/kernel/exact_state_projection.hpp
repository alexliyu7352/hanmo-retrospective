// Shared exact state emitters borrow the one original field traversal.
#pragma once
#include "world_timeline.hpp"
#include "resident/facility_claims/model.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_world_timeline2_projection(ProjectionWriter&,std::string_view,const WorldTimelineState&);
Status emit_action_elapsed2_projection(ProjectionWriter&,std::string_view,const ActionElapsedStore&);
Status emit_facility_claims2_projection(ProjectionWriter&,std::string_view,const FacilityClaimsState&);
}
