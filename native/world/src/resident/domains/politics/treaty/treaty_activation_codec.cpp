#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "treaty_activation_codec_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_activation_source(CodecVisitor& c,TreatyActivationSource& r){return treaty_activation_codec_fields::fields(c,r);}
Status emit_treaty_activation_source_projection(ProjectionWriter& sink,std::string_view name,const TreatyActivationSource& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_activation_codec_fields::fields(c,v);});}
Outcome<BoundaryValue> encode_treaty_activation_source(const TreatyActivationSource& r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_treaty_activation_source_projection(writer,"",r);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyActivationSource> decode_treaty_activation_source(const BoundaryValue& input){TreatyActivationSource out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_activation_codec_fields::fields(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
}
