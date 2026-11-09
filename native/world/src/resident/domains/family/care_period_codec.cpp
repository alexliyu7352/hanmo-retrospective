#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "care_period_codec.hpp"
#include "care_period_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_care_period_record(CodecVisitor& c,CarePeriod& v,Integer version){return care_period_codec::period(c,v,version);}
Status emit_care_period_record_fields(ProjectionWriter& w,const CarePeriod& v,Integer version){ActorFoundationProjectionArchive c(w);return care_period_codec::period(c,const_cast<CarePeriod&>(v),version);}
Outcome<BoundaryValue> encode_care_period_record(const CarePeriod& v,Integer version){BoundaryValue value;BoundaryProjectionWriter w(value);auto s=w.object("",[&](ProjectionWriter& out){return emit_care_period_record_fields(out,v,version);});if(s.ok)s=w.finish();return {s.ok,std::move(value),s.error};}
Outcome<CarePeriod> decode_care_period_record(const BoundaryValue& input,Integer version){CarePeriod v;ActorFoundationJsonVisitor c(input);auto s=care_period_codec::period(c,v,version);if(s.ok)s=c.finish();return {s.ok,std::move(v),s.error};}
Status visit_care_period_source_record(CodecVisitor& c,CarePeriodSource& v,Integer version){return care_period_codec::original_source(c,v,version);}
Status emit_care_period_source_record_fields(ProjectionWriter& w,const CarePeriodSource& v,Integer version){ActorFoundationProjectionArchive c(w);return care_period_codec::original_source(c,const_cast<CarePeriodSource&>(v),version);}
Outcome<BoundaryValue> encode_care_period_source_record(const CarePeriodSource& v,Integer version){BoundaryValue value;BoundaryProjectionWriter w(value);auto s=w.object("",[&](ProjectionWriter& out){return emit_care_period_source_record_fields(out,v,version);});if(s.ok)s=w.finish();return {s.ok,std::move(value),s.error};}
Outcome<CarePeriodSource> decode_care_period_source_record(const BoundaryValue& input,Integer version){CarePeriodSource v;ActorFoundationJsonVisitor c(input);auto s=care_period_codec::original_source(c,v,version);if(s.ok)s=c.finish();return {s.ok,std::move(v),s.error};}
}
