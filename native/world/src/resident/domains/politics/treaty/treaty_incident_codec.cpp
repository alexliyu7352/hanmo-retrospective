#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "treaty_incident_codec_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_incident(CodecVisitor& c,TreatyIncident& r){return treaty_incident_codec_fields::incident(c,r);}
Status emit_treaty_incident_projection(ProjectionWriter& sink,std::string_view name,const TreatyIncident& r){
 return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_incident_codec_fields::incident(c,v);});
}
Outcome<BoundaryValue> encode_treaty_incident(const TreatyIncident& r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto s=emit_treaty_incident_projection(writer,"",r);if(s.ok)s=writer.finish();return {s.ok,std::move(out),s.error};}
Outcome<TreatyIncident> decode_treaty_incident(const BoundaryValue& input){TreatyIncident out;ActorFoundationJsonVisitor reader(input);auto s=treaty_incident_codec_fields::incident(reader,out);if(s.ok)s=reader.finish();return {s.ok,std::move(out),s.error};}
}
