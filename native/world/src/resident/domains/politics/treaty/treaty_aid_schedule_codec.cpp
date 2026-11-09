#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "treaty_aid_schedule_codec_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_aid_schedule(CodecVisitor& c,TreatyAidSchedule& r){return treaty_aid_schedule_codec_fields::fields(c,r);}
Status emit_treaty_aid_schedule_projection(ProjectionWriter& sink,std::string_view name,const TreatyAidSchedule& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_aid_schedule_codec_fields::fields(c,v);});}
Outcome<BoundaryValue> encode_treaty_aid_schedule(const TreatyAidSchedule& r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto s=emit_treaty_aid_schedule_projection(writer,"",r);if(s.ok)s=writer.finish();return {s.ok,std::move(out),s.error};}
Outcome<TreatyAidSchedule> decode_treaty_aid_schedule(const BoundaryValue& input){TreatyAidSchedule out;ActorFoundationJsonVisitor reader(input);auto s=treaty_aid_schedule_codec_fields::fields(reader,out);if(s.ok)s=reader.finish();return {s.ok,std::move(out),s.error};}
}
