// 独立military_tactic_learning/1账候选；旧Military1..15不追加尾，实际资源仍归原BattleUnit。
#pragma once
#include "military_tactic_learning_contract.hpp"
namespace hanmo::resident {
struct MilitaryLearnedTacticRule {
 MilitaryLearnedTacticKind kind=MilitaryLearnedTacticKind::rally;
 StableId prerequisite_skill_id;Integer prerequisite_level=0,point_cost=0;
};
struct MilitaryLearnedTacticDefinition {
 StableId id,name_key,description_key;
 MilitaryLearnedTacticKind kind=MilitaryLearnedTacticKind::rally;
 std::vector<StableId> teacher_actor_ids,practice_challenge_ids;
 // 普通实践门槛为显式作者输入，实际得分只读原挑战结算。
 Integer practice_min_score=60;
};
struct MilitaryTacticInitialGrant { StableId id,actor_id,tactic_id; };
struct MilitaryTacticLearningCatalog {
 bool enabled=false;
 std::vector<MilitaryLearnedTacticDefinition> definitions;
 std::vector<MilitaryTacticInitialGrant> initial_grants;
};
struct MilitaryTacticAcquisitionFact {
 StableId id,actor_id,tactic_id,source_id;
 MilitaryTacticLearningSourceKind source_kind=MilitaryTacticLearningSourceKind::teaching;
 Integer tick=0;
 FieldOrder field_order;
};
// 认定本身是本人零时具名决定；分数和报名资格仍从原训练/成长账精确读取。
struct MilitaryTacticPracticeFact { StableId id,actor_id,tactic_id,training_action_id;Integer tick=0;FieldOrder field_order; };
struct MilitaryTacticLearningState { std::vector<MilitaryTacticAcquisitionFact> acquisitions;FieldOrder field_order;std::vector<MilitaryTacticPracticeFact> practices; };
// 新16入战段拟冻结已取得的原本领ID；每轮不再从今天的已学列表回填。
struct MilitaryTacticEntryUnit {
 StableId unit_id,commander_actor_id;
 std::vector<std::pair<StableId,StableId>> tactic_acquisition_ids;
 FieldOrder field_order;
};
struct MilitaryTacticEntryState { Integer entry_tick=0;std::vector<MilitaryTacticEntryUnit> units;FieldOrder field_order; };
}
