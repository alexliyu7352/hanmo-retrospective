// JSON/binary/无树导出共用同一replies字段表，不从二进制猜JSON形状。
#include "treaty_reply_record_codec_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_reply_record(CodecVisitor& c,TreatyReplyRecord& r){return treaty_reply_record_codec_fields::fields(c,r);}
Status emit_treaty_reply_record_projection(ProjectionWriter& w,std::string_view name,const TreatyReplyRecord& r){return emit_projection_record<ActorFoundationProjectionArchive>(w,name,r,[](auto& c,auto& v){return treaty_reply_record_codec_fields::fields(c,v);});}
Outcome<BoundaryValue> encode_treaty_reply_record(const TreatyReplyRecord& r){BoundaryValue out;BoundaryProjectionWriter w(out);auto s=emit_treaty_reply_record_projection(w,"",r);if(s.ok)s=w.finish();return {s.ok,std::move(out),s.error};}
Outcome<TreatyReplyRecord> decode_treaty_reply_record(const BoundaryValue& in){TreatyReplyRecord out;ActorFoundationJsonVisitor c(in);auto s=treaty_reply_record_codec_fields::fields(c,out);if(s.ok)s=c.finish();return {s.ok,std::move(out),s.error};}
}
