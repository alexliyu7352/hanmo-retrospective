// 仅显示本人已取得与最近真实训练的有限投影；候选报价不授予学习结果。
#pragma once
#include "military_tactic_practice.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> military_tactic_learning_page(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryTacticLearningState&,const MilitaryAccess&,const StableId& actor);
}
