#include "care_coverage_codec.hpp"
#include "care_coverage_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_care_binding_record(CodecVisitor& c,CareBinding& v,Integer version){return care_coverage_codec::binding(c,v,version);}
Status emit_care_binding_record_fields(ProjectionWriter& w,const CareBinding& v,Integer version){ActorFoundationProjectionArchive c(w);return care_coverage_codec::binding(c,const_cast<CareBinding&>(v),version);}
Outcome<BoundaryValue> encode_care_binding_record(const CareBinding& v,Integer version){BoundaryValue value;BoundaryProjectionWriter w(value);auto s=w.object("",[&](ProjectionWriter& out){return emit_care_binding_record_fields(out,v,version);});if(s.ok)s=w.finish();return {s.ok,std::move(value),s.error};}
Outcome<CareBinding> decode_care_binding_record(const BoundaryValue& input,Integer version){CareBinding v;ActorFoundationJsonVisitor c(input);auto s=care_coverage_codec::binding(c,v,version);if(s.ok)s=c.finish();return {s.ok,std::move(v),s.error};}
Status visit_care_coverage_satisfied_record(CodecVisitor& c,CareCoverageSatisfied& v,Integer version){return care_coverage_codec::satisfied(c,v,version);}
Status emit_care_coverage_satisfied_record_fields(ProjectionWriter& w,const CareCoverageSatisfied& v,Integer version){ActorFoundationProjectionArchive c(w);return care_coverage_codec::satisfied(c,const_cast<CareCoverageSatisfied&>(v),version);}
Outcome<BoundaryValue> encode_care_coverage_satisfied_record(const CareCoverageSatisfied& v,Integer version){BoundaryValue value;BoundaryProjectionWriter w(value);auto s=w.object("",[&](ProjectionWriter& out){return emit_care_coverage_satisfied_record_fields(out,v,version);});if(s.ok)s=w.finish();return {s.ok,std::move(value),s.error};}
Outcome<CareCoverageSatisfied> decode_care_coverage_satisfied_record(const BoundaryValue& input,Integer version){CareCoverageSatisfied v;ActorFoundationJsonVisitor c(input);auto s=care_coverage_codec::satisfied(c,v,version);if(s.ok)s=c.finish();return {s.ok,std::move(v),s.error};}
Status visit_care_coverage_lost_record(CodecVisitor& c,CareCoverageLost& v,Integer version){return care_coverage_codec::lost(c,v,version);}
Status emit_care_coverage_lost_record_fields(ProjectionWriter& w,const CareCoverageLost& v,Integer version){ActorFoundationProjectionArchive c(w);return care_coverage_codec::lost(c,const_cast<CareCoverageLost&>(v),version);}
Outcome<BoundaryValue> encode_care_coverage_lost_record(const CareCoverageLost& v,Integer version){BoundaryValue value;BoundaryProjectionWriter w(value);auto s=w.object("",[&](ProjectionWriter& out){return emit_care_coverage_lost_record_fields(out,v,version);});if(s.ok)s=w.finish();return {s.ok,std::move(value),s.error};}
Outcome<CareCoverageLost> decode_care_coverage_lost_record(const BoundaryValue& input,Integer version){CareCoverageLost v;ActorFoundationJsonVisitor c(input);auto s=care_coverage_codec::lost(c,v,version);if(s.ok)s=c.finish();return {s.ok,std::move(v),s.error};}
}
