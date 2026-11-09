#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "study_lesson_capability_codec.hpp"
#include "study_lesson_capability_codec_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
using namespace study_lesson_codec;
Status visit_study_lesson_capability_source(CodecVisitor& c,LessonCapabilitySource& value){return row(c,value);}
Status emit_study_lesson_capability_source(ProjectionWriter& sink,std::string_view name,const LessonCapabilitySource& value){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,[](auto& c,auto& v){return row(c,v);});}
Outcome<BoundaryValue> encode_study_lesson_capability_source(const LessonCapabilitySource& value){BoundaryValue out;BoundaryProjectionWriter sink(out);auto result=emit_study_lesson_capability_source(sink,"",value);if(result.ok)result=sink.finish();return {result.ok,std::move(out),result.error};}
Outcome<LessonCapabilitySource> decode_study_lesson_capability_source(const BoundaryValue& value){LessonCapabilitySource out;ActorFoundationJsonVisitor c(value);auto result=row(c,out);if(result.ok)result=c.finish();return {result.ok,std::move(out),result.error};}
}
