// 三出口共同访问同一委任源，decoder只建私有候选，完整权源由原history门核。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "treaty_mandate_codec_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_mandate_source(CodecVisitor& codec,TreatyMandateSource& row){return treaty_mandate_codec_fields::fields(codec,row);}
Status emit_treaty_mandate_source_projection(ProjectionWriter& out,std::string_view name,const TreatyMandateSource& row){return emit_projection_record<ActorFoundationProjectionArchive>(out,name,row,[](auto& c,auto& v){return treaty_mandate_codec_fields::fields(c,v);});}
Outcome<BoundaryValue> encode_treaty_mandate_source(const TreatyMandateSource& row){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_treaty_mandate_source_projection(writer,"",row);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyMandateSource> decode_treaty_mandate_source(const BoundaryValue& input){TreatyMandateSource out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_mandate_codec_fields::fields(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
}
