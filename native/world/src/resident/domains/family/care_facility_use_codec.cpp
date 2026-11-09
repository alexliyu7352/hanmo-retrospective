#include "care_facility_use_codec.hpp"
#include "care_facility_use_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_care_facility_use_binding(CodecVisitor& c,CareFacilityUseBinding& v,Integer version){return care_facility_codec::binding(c,v,version);}
Status emit_care_facility_use_binding_fields(ProjectionWriter& w,const CareFacilityUseBinding& v,Integer version){ActorFoundationProjectionArchive c(w);auto s=care_facility_codec::binding(c,const_cast<CareFacilityUseBinding&>(v),version);return s.ok?w.order_fields(v.field_order):s;}
Outcome<BoundaryValue> encode_care_facility_use_binding(const CareFacilityUseBinding& v,Integer version){BoundaryValue out;BoundaryProjectionWriter w(out);auto s=w.object("",[&](ProjectionWriter& x){return emit_care_facility_use_binding_fields(x,v,version);});if(s.ok)s=w.finish();return {s.ok,std::move(out),s.error};}
Outcome<CareFacilityUseBinding> decode_care_facility_use_binding(const BoundaryValue& input,Integer version){CareFacilityUseBinding value;ActorFoundationJsonVisitor c(input);auto s=care_facility_codec::binding(c,value,version);if(s.ok)s=c.finish();return {s.ok,std::move(value),s.error};}
}
