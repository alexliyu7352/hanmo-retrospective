// 原03八个BA的基础资格/指挥消耗固定；资料解锁和教师资格仍必须有具名实际来源。
#pragma once
#include "military_tactic_learning_model.hpp"
namespace hanmo::resident {
Outcome<MilitaryLearnedTacticRule> military_learned_tactic_rule(MilitaryLearnedTacticKind);
StableId military_learned_tactic_kind_name(MilitaryLearnedTacticKind);
Outcome<MilitaryLearnedTacticKind> military_learned_tactic_kind(const StableId&);
Status validate_military_tactic_learning_catalog(const MilitaryTacticLearningCatalog&);
Status validate_military_tactic_learning_shape(const MilitaryTacticLearningState&);
}
