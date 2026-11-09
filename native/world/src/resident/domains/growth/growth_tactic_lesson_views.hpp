// 只展示本人正在听到的原教师提议及本人真实教学履历；不给远处教师能力清单。
#pragma once
#include "growth_tactic_lesson_commands.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> growth_tactic_lesson_view(const CommonStore&,const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const GrowthTacticLessonCatalog&,const GrowthTacticLessonState&,const GrowthTacticLessonSourceAccess&,const StableId& observer,Integer page);
}
