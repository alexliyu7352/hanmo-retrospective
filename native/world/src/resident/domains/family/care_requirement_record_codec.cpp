#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_requirement_record_codec.hpp"
#include "care_requirement_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_care_requirement_record(CodecVisitor& c,CareRequirement& v){return care_requirement_codec::requirement(c,v);}
Status visit_care_requirement_transition_record(CodecVisitor& c,CareRequirementTransition& v){return care_requirement_codec::transition(c,v);}
Status visit_care_temporal_event_record(CodecVisitor& c,CareTemporalEvent& v){return care_requirement_codec::temporal_event(c,v);}
Status emit_care_requirement_record_fields(ProjectionWriter& w,const CareRequirement& v){ActorFoundationProjectionArchive c(w);return care_requirement_codec::requirement(c,const_cast<CareRequirement&>(v));}
Status emit_care_requirement_transition_record_fields(ProjectionWriter& w,const CareRequirementTransition& v){ActorFoundationProjectionArchive c(w);return care_requirement_codec::transition(c,const_cast<CareRequirementTransition&>(v));}
Status emit_care_temporal_event_record_fields(ProjectionWriter& w,const CareTemporalEvent& v){ActorFoundationProjectionArchive c(w);return care_requirement_codec::temporal_event(c,const_cast<CareTemporalEvent&>(v));}
Outcome<CareRequirement> decode_care_requirement_record(const BoundaryValue& input){CareRequirement value;ActorFoundationJsonVisitor c(input);auto status=care_requirement_codec::requirement(c,value);if(status.ok)status=c.finish();return {status.ok,std::move(value),status.error};}
Outcome<CareRequirementTransition> decode_care_requirement_transition_record(const BoundaryValue& input){CareRequirementTransition value;ActorFoundationJsonVisitor c(input);auto status=care_requirement_codec::transition(c,value);if(status.ok)status=c.finish();return {status.ok,std::move(value),status.error};}
Outcome<CareTemporalEvent> decode_care_temporal_event_record(const BoundaryValue& input){CareTemporalEvent value;ActorFoundationJsonVisitor c(input);auto status=care_requirement_codec::temporal_event(c,value);if(status.ok)status=c.finish();return {status.ok,std::move(value),status.error};}
}
