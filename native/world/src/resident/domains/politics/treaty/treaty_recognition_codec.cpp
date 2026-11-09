#include "treaty_recognition_codec_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_presentation_source(CodecVisitor& c,TreatyPresentationSource& r){return treaty_recognition_codec_fields::presentation(c,r);}
Status visit_treaty_reply_source(CodecVisitor& c,TreatyReplySource& r){return treaty_recognition_codec_fields::reply(c,r);}
Status emit_treaty_presentation_source_projection(ProjectionWriter& sink,std::string_view name,const TreatyPresentationSource& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_recognition_codec_fields::presentation(c,v);});}
Status emit_treaty_reply_source_projection(ProjectionWriter& sink,std::string_view name,const TreatyReplySource& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_recognition_codec_fields::reply(c,v);});}
Outcome<BoundaryValue> encode_treaty_presentation_source(const TreatyPresentationSource& r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_treaty_presentation_source_projection(writer,"",r);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyPresentationSource> decode_treaty_presentation_source(const BoundaryValue& input){TreatyPresentationSource out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_recognition_codec_fields::presentation(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<BoundaryValue> encode_treaty_reply_source(const TreatyReplySource& r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_treaty_reply_source_projection(writer,"",r);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyReplySource> decode_treaty_reply_source(const BoundaryValue& input){TreatyReplySource out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_recognition_codec_fields::reply(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
}
