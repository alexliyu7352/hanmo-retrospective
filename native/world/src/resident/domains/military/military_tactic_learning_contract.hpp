// 军战本领与XP分开：本读只证明原实际取得，不把图鉴/作者军种列表或达级当习得。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
enum class MilitaryLearnedTacticKind {
  rally,
  guard,
  suppress,
  flank,
  reform,
  march,
  feint,
  support
};
enum class MilitaryTacticLearningSourceKind {
  authored_initial,
  teaching,
  practice
};
struct MilitaryTacticLearningObservation {
  // Transient original read payload only; not a persisted tactic/history row.
  CheckpointMemoryCharge copied_payload;
  WorldIdentity source_identity;
  StableId actor_id, tactic_id, acquisition_fact_id, source_fact_id,
      initial_grant_id, before_fact_id;
  MilitaryTacticLearningSourceKind source_kind =
      MilitaryTacticLearningSourceKind::teaching;
  Integer acquired_tick = 0, observed_tick = 0;
};
// 目录只提供封闭本领目标和原03基础资格；学费/工期仍由Growth真实教学政策决定。
struct MilitaryTacticTeachingTarget {
  WorldIdentity source_identity;
  StableId tactic_id, kind, prerequisite_skill_id;
  Integer prerequisite_level = 0;
  std::vector<StableId> teacher_actor_ids;
};
// 当前调用仅完整已验候选；历史只认实际lesson/entry头前，未知边界硬拒。
class MilitaryTacticKnowledgeRead {
public:
  virtual ~MilitaryTacticKnowledgeRead() = default;
  virtual Outcome<MilitaryTacticTeachingTarget>
  tactic_definition(const StableId &tactic) const = 0;
  virtual Outcome<std::optional<MilitaryTacticLearningObservation>>
  current_tactic(const StableId &actor, const StableId &tactic) const = 0;
  virtual Outcome<std::optional<MilitaryTacticLearningObservation>>
  tactic_before(const StableId &actor, const StableId &tactic,
                Integer observed_tick,
                const StableId &actual_before_fact) const = 0;
};
// 教学费用、真实课时/接受与资格完整归原Growth；S2只在其已核成功原件之后登记本领。
struct MilitaryTacticTeachingCompletion {
  WorldIdentity source_identity;
  StableId source_fact_id, started_fact_id, action_id, tactic_id, student_id,
      teacher_id, place_id, agreement_id, teacher_say_source;
  StableId prerequisite_skill_id, teacher_acquisition_fact_id,
      teacher_initial_grant_id;
  // 零时同tick迟答不假定已在前；此为原初+永久Study可证下界，只作报名资格。
  Integer started_tick = 0, completed_tick = 0, qualification_observed_tick = 0,
          student_minimum_experience = 0, student_minimum_level = 0;
  std::vector<StableId> qualification_causal_fact_ids;
};
struct MilitaryTacticPracticeCompletion {
  WorldIdentity source_identity;
  StableId source_fact_id, action_id, tactic_id, actor_id, challenge_id,
      prerequisite_skill_id;
  Integer started_tick = 0, completed_tick = 0, score = 0,
          qualification_observed_tick = 0, actor_minimum_experience = 0,
          actor_minimum_level = 0;
  std::vector<StableId> qualification_causal_fact_ids;
};
// 本人选择原已完成操练进行单张认定；不含任意受益人物或免费分数。
struct MilitaryTacticPracticeClaim {
  StableId training_action_id, tactic_id;
  bool input_valid = true;
};
struct MilitaryTacticPracticeReceipt {
  StableId practice_fact_id, acquisition_fact_id, training_action_id, tactic_id;
  bool replayed = false;
  FieldOrder field_order;
};
class MilitaryTacticLearningSourceRead {
public:
  virtual ~MilitaryTacticLearningSourceRead() = default;
  virtual Outcome<MilitaryTacticTeachingCompletion>
  completed_tactic_teaching(const StableId &exact_source_fact) const = 0;
  virtual Outcome<MilitaryTacticPracticeCompletion>
  completed_tactic_practice(const StableId &exact_source_fact) const = 0;
};
} // namespace hanmo::resident
