// v1b§5原lesson_capability_sources的一项访客；外层七根模块由完整Study来源包配对选择。
#pragma once
#include "study_lesson_capability_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include <string_view>
namespace hanmo::resident {
class ProjectionWriter;
Status visit_study_lesson_capability_source(CodecVisitor&,LessonCapabilitySource&);
Status emit_study_lesson_capability_source(ProjectionWriter&,std::string_view,const LessonCapabilitySource&);
Outcome<BoundaryValue> encode_study_lesson_capability_source(const LessonCapabilitySource&);
Outcome<LessonCapabilitySource> decode_study_lesson_capability_source(const BoundaryValue&);
}
