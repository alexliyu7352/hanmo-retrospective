// growth_tactic_lessons/1独立尾；旧Growth visitor与所有旧profile保持原字段序和字节。
#pragma once
#include "growth_tactic_lessons.hpp"
namespace hanmo::resident {
class CodecVisitor;
Status validate_growth_tactic_lesson_shape(const GrowthTacticLessonState&);
Status validate_growth_tactic_lesson_agreement_shape(const GrowthTacticLessonAgreement&);
Outcome<GrowthTacticLessonCatalog> decode_growth_tactic_lesson_catalog(const BoundaryValue&);
Outcome<GrowthTacticLessonState> decode_growth_tactic_lessons(const BoundaryValue&);
BoundaryValue encode_growth_tactic_lessons(const GrowthTacticLessonState&);
Status visit_growth_tactic_lessons_checkpoint(CodecVisitor&,GrowthTacticLessonState&);
}
