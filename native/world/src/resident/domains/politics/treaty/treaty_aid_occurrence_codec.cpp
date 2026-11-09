#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "treaty_aid_occurrence_codec_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_aid_occurrence_binding(CodecVisitor& c,TreatyAidOccurrenceBinding& r){return treaty_aid_occurrence_codec_fields::fields(c,r);}
Status emit_treaty_aid_occurrence_binding_projection(ProjectionWriter& sink,std::string_view name,const TreatyAidOccurrenceBinding& r){
 return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_aid_occurrence_codec_fields::fields(c,v);});
}
Outcome<BoundaryValue> encode_treaty_aid_occurrence_binding(const TreatyAidOccurrenceBinding& r){
 BoundaryValue value;BoundaryProjectionWriter writer(value);auto s=emit_treaty_aid_occurrence_binding_projection(writer,"",r);if(s.ok)s=writer.finish();return {s.ok,std::move(value),s.error};
}
Outcome<TreatyAidOccurrenceBinding> decode_treaty_aid_occurrence_binding(const BoundaryValue& input){
 TreatyAidOccurrenceBinding r;ActorFoundationJsonVisitor reader(input);auto s=treaty_aid_occurrence_codec_fields::fields(reader,r);if(s.ok)s=reader.finish();return {s.ok,std::move(r),s.error};
}
}
