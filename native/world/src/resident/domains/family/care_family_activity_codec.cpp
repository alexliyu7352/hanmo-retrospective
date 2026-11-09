#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_family_activity_codec.hpp"
#include "care_family_activity_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_care_family_activity_record(CodecVisitor& c,CareFamilyActivity& v,Integer version){return care_family_activity_codec::activity(c,v,version);}
Status emit_care_family_activity_record_fields(ProjectionWriter& w,const CareFamilyActivity& v,Integer version){ActorFoundationProjectionArchive c(w);return care_family_activity_codec::activity(c,const_cast<CareFamilyActivity&>(v),version);}
Outcome<BoundaryValue> encode_care_family_activity_record(const CareFamilyActivity& v,Integer version){BoundaryValue value;BoundaryProjectionWriter w(value);auto s=w.object("",[&](ProjectionWriter& out){return emit_care_family_activity_record_fields(out,v,version);});if(s.ok)s=w.finish();return {s.ok,std::move(value),s.error};}
Outcome<CareFamilyActivity> decode_care_family_activity_record(const BoundaryValue& input,Integer version){CareFamilyActivity v;ActorFoundationJsonVisitor c(input);auto s=care_family_activity_codec::activity(c,v,version);if(s.ok)s=c.finish();return {s.ok,std::move(v),s.error};}
}
