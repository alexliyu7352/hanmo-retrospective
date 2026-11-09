#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_arrangement_codec.hpp"
#include "care_arrangement_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_accepted_caregiver_record(CodecVisitor& c,AcceptedCaregiver& v,Integer version){return care_arrangement_codec::accepted(c,v,version);}
Status emit_accepted_caregiver_record_fields(ProjectionWriter& w,const AcceptedCaregiver& v,Integer version){ActorFoundationProjectionArchive c(w);return care_arrangement_codec::accepted(c,const_cast<AcceptedCaregiver&>(v),version);}
Outcome<BoundaryValue> encode_accepted_caregiver_record(const AcceptedCaregiver& v,Integer version){BoundaryValue value;BoundaryProjectionWriter out(value);auto s=out.object("",[&](ProjectionWriter& w){return emit_accepted_caregiver_record_fields(w,v,version);});if(!s.ok)return {false,{},s.error};s=out.finish();return {s.ok,std::move(value),s.error};}
Outcome<AcceptedCaregiver> decode_accepted_caregiver_record(const BoundaryValue& input,Integer version){AcceptedCaregiver value;ActorFoundationJsonVisitor c(input);auto s=care_arrangement_codec::accepted(c,value,version);if(s.ok)s=c.finish();return {s.ok,std::move(value),s.error};}
}
