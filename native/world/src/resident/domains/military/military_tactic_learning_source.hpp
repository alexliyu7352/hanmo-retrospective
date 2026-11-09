// 早门核原Common序与唯一习得；晚门核Growth成功完成，两个门不可互代。
#pragma once
#include "military_tactic_learning.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
Status validate_military_tactic_learning_structure(
    const CommonStore &, const CommonCatalog &,
    const MilitaryTacticLearningCatalog &, const MilitaryTacticLearningState &,
    const DynamicStepInput *phase = nullptr);
Status validate_military_tactic_learning_sources(
    const CommonStore &, const CommonCatalog &,
    const MilitaryTacticLearningCatalog &, const MilitaryTacticLearningState &,
    const MilitaryTacticLearningSourceRead &,
    const DynamicStepInput *phase = nullptr);
Outcome<MilitaryTacticTeachingTarget>
read_military_tactic_teaching_target(const CommonStore &,
                                     const MilitaryTacticLearningCatalog &,
                                     const StableId &tactic);
// 空边界只当前稳定/严格单步；历史边界只实际 lesson、军事 entry/Field 入场或本域
// learned 头。 Field caller 仍须用同场安装/边界记录反核该头，不以一个同名
// Common 头代替完整来源。
Outcome<std::optional<MilitaryTacticLearningObservation>>
read_military_learned_tactic(const CommonStore &, const CommonCatalog &,
                             const MilitaryTacticLearningCatalog &,
                             const MilitaryTacticLearningState &,
                             const StableId &actor, const StableId &tactic,
                             Integer observed_tick,
                             const StableId &actual_before_fact = {},
                             const DynamicStepInput *phase = nullptr);
// 完整Lesson反向核验只读这一真实closed后的唯一取得，不再进入Lesson验证。
Outcome<MilitaryTacticLearningObservation>
read_military_tactic_teaching_acquisition(const CommonStore &,
                                          const CommonCatalog &,
                                          const MilitaryTacticLearningCatalog &,
                                          const MilitaryTacticLearningState &,
                                          const StableId &exact_closed);
// 原完成源自动触发，不提供接受任意actor/tactic的免费学习命令。失败由外层整候选回滚。
struct MilitaryTacticLearningResult {
  StableId acquisition_fact_id;
  bool replayed = false;
};
Status initialize_military_tactic_learning_in_candidate(
    CommonStore &, const CommonCatalog &, const MilitaryTacticLearningCatalog &,
    MilitaryTacticLearningState &);
Outcome<MilitaryTacticLearningResult>
record_military_tactic_completion_in_candidate(
    CommonStore &, const CommonCatalog &, const MilitaryTacticLearningCatalog &,
    MilitaryTacticLearningState &, MilitaryTacticLearningSourceKind,
    const StableId &exact_source_fact, const MilitaryTacticLearningSourceRead &,
    const DynamicStepInput *phase = nullptr);
} // namespace hanmo::resident
