#pragma once
#include "growth_tactic_lessons.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_growth_tactic_teacher_source_fields(ProjectionWriter&,const GrowthTacticTeacherSource&);
Status emit_growth_tactic_prerequisite_fields(ProjectionWriter&,const GrowthTacticPrerequisite&);
Status emit_growth_tactic_lesson_agreement_fields(ProjectionWriter&,const GrowthTacticLessonAgreement&);
Status emit_growth_tactic_lesson_commit_fields(ProjectionWriter&,const GrowthTacticLessonCommit&);
Status emit_growth_tactic_lesson_start_fields(ProjectionWriter&,const GrowthTacticLessonStart&);
Status emit_growth_tactic_lesson_credit_fields(ProjectionWriter&,const GrowthTacticLessonCredit&);
Status emit_growth_tactic_lesson_closure_fields(ProjectionWriter&,const GrowthTacticLessonClosure&);
Status emit_growth_tactic_lessons_fields(ProjectionWriter&,const GrowthTacticLessonState&);
}
