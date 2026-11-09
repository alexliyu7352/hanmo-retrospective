#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "safe_exit_codec_fields.hpp"
namespace hanmo::resident {
Status visit_safe_exit_binding(CodecVisitor& c,SafeExitBinding& r){return safe_exit_codec_fields::binding(c,r);}
Status visit_safe_exit_line(CodecVisitor& c,SafeExitLine& r){return safe_exit_codec_fields::line(c,r);}
Status emit_safe_exit_binding_projection(ProjectionWriter& sink,std::string_view name,const SafeExitBinding& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return safe_exit_codec_fields::binding(c,v);});}
Status emit_safe_exit_line_projection(ProjectionWriter& sink,std::string_view name,const SafeExitLine& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return safe_exit_codec_fields::line(c,v);});}
Outcome<BoundaryValue> encode_safe_exit_binding(const SafeExitBinding& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_safe_exit_binding_projection(sink,"",r);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<SafeExitBinding> decode_safe_exit_binding(const BoundaryValue& input){SafeExitBinding out;ActorFoundationJsonVisitor c(input);auto valid=safe_exit_codec_fields::binding(c,out);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<BoundaryValue> encode_safe_exit_line(const SafeExitLine& r){BoundaryValue out;BoundaryProjectionWriter sink(out);auto valid=emit_safe_exit_line_projection(sink,"",r);if(valid.ok)valid=sink.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<SafeExitLine> decode_safe_exit_line(const BoundaryValue& input){SafeExitLine out;ActorFoundationJsonVisitor c(input);auto valid=safe_exit_codec_fields::line(c,out);if(valid.ok)valid=c.finish();return {valid.ok,std::move(out),valid.error};}
}
