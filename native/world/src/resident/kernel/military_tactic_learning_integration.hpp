// 同候选借用唯一Military掌握账；教学与训练来源只经具名纯读，不能另存XP或掌握副本。
#pragma once
#include "resident/domains/military/military_tactic_learning_source.hpp"
#include "resident/domains/military/military_tactic_practice.hpp"
#include <memory>
namespace hanmo::resident {
struct WorldBundle;
std::unique_ptr<MilitaryTacticKnowledgeRead> make_military_tactic_knowledge_read(const WorldBundle&,const DynamicStepInput* phase=nullptr);
Outcome<MilitaryTacticPracticeCompletion> preview_military_tactic_practice_world(const WorldBundle&,const StableId&,const MilitaryTacticPracticeClaim&);
Outcome<MilitaryTacticPracticeReceipt> apply_military_tactic_practice_world_in_candidate(WorldBundle&,const StableId&,const MilitaryTacticPracticeClaim&);
// 只在clock_commit后真实本半日closed刚落账的同一完整候选调用；失败由外层World回滚。
Outcome<MilitaryTacticLearningResult> record_military_tactic_teaching_world_in_candidate(WorldBundle&,const StableId& exact_closed,const DynamicStepInput& committed_step);
Status validate_military_tactic_learning_world_structure(const WorldBundle&);
// Family/Birth/Growth、Military/Naval训练与完整Lesson/actual-result均通过后调用，再核反向唯一取得。
Status validate_military_tactic_learning_world_sources(const WorldBundle&);
}
