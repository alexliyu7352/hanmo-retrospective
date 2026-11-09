// 作者显式授予仅限原始真人，教师候选与实际习得分开；不按列表自动学会。
#pragma once
#include "military_tactic_learning.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
Outcome<MilitaryTacticLearningCatalog> decode_military_tactic_learning_catalog(const BoundaryValue& policy,const BoundaryValue& data);
}
