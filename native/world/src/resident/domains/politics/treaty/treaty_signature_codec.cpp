#include "treaty_signature_codec_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_signature_source(CodecVisitor& c,TreatySignatureSource& r){return treaty_signature_codec_fields::fields(c,r);}
Status emit_treaty_signature_source_projection(ProjectionWriter& sink,std::string_view name,const TreatySignatureSource& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_signature_codec_fields::fields(c,v);});}
Outcome<BoundaryValue> encode_treaty_signature_source(const TreatySignatureSource& r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_treaty_signature_source_projection(writer,"",r);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatySignatureSource> decode_treaty_signature_source(const BoundaryValue& input){TreatySignatureSource out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_signature_codec_fields::fields(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
}
