// Original movement execution union, retaining the existing nested wire fields.
#pragma once
#include "movement_exact_model.hpp"
#include "spatial_carrier_ref_codec_fields.hpp"
namespace hanmo::resident::movement_execution_ref_codec_fields {
using spatial_carrier_ref_codec_fields::scalar;
template<class C>Status branch_fields(C& c,PersonalMovementExecution& v){scalar(c,"action_id",v.action_id);scalar(c,"journey_id",v.journey_id);return scalar(c,"segment_ordinal",v.segment_ordinal);}
template<class C>Status branch_fields(C& c,GroupMovementExecution& v){scalar(c,"journey_id",v.journey_id);c.object("carrier",[&](auto& a){return spatial_carrier_ref_codec_fields::fields(a,v.carrier);});return scalar(c,"segment_ordinal",v.segment_ordinal);}
template<class C>Status branch_fields(C& c,CustodyExitMovementExecution& v){scalar(c,"escape_action_id",v.escape_action_id);scalar(c,"exit_terms_id",v.exit_terms_id);return scalar(c,"segment_ordinal",v.segment_ordinal);}
template<class C>Status branch_fields(C& c,ShoreAssaultMovementExecution& v){scalar(c,"landing_operation_id",v.landing_operation_id);scalar(c,"crossing_action_id",v.crossing_action_id);return scalar(c,"segment_ordinal",v.segment_ordinal);}
template<class C>Status fields(C& c,MovementExecutionRef& v){Integer tag=Integer(v.index());c.field("variant",tag);if(c.status().ok&&(tag<0||tag>3))return c.reject("InvalidMovementVariant");if(c.reading()&&c.status().ok){switch(tag){case 0:v=PersonalMovementExecution{};break;case 1:v=GroupMovementExecution{};break;case 2:v=CustodyExitMovementExecution{};break;case 3:v=ShoreAssaultMovementExecution{};break;}}if(c.status().ok)return std::visit([&](auto& b){return branch_fields(c,b);},v);return c.status();}
// A group journey owns the group alternative of this same original union.
// Borrow the branch in place; no temporary deep copy of carrier identity.
template<class C>Status group_only_fields(C& c,GroupMovementExecution& v){Integer tag=1;c.field("variant",tag);if(c.status().ok&&tag!=1)return c.reject("GroupJourneyExecutionBranchRequired");return c.status().ok?branch_fields(c,v):c.status();}

}
