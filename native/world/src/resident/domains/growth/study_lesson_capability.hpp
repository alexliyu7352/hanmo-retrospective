#include "resident/domains/growth/growth_model.hpp"
// v1b§5实际逐课能力来源；原StudyLesson仍唯一XP结果，S0仍唯一服务累计。
#pragma once
#include "growth_fixed_execution.hpp"
#include "study_lesson_capability_model.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
#include <optional>
#include <string_view>
namespace hanmo::resident {
// Borrowed build inputs for the original Study before_head scratch only.
// These are not persistent condition models or new source identities.
struct ConditionReceiptRefV2BuildView {std::string_view result_id;};
struct StudyBeforeHeadBuildView {
 std::string_view actor_id;
 Integer condition_revision=0;
 const InitialConditionSeedRef& initial_condition_seed_ref;
 std::optional<ConditionReceiptRefV2BuildView> latest_receipt_ref;
};


class PreparedStudyLessonCapability {
public:struct State;PreparedStudyLessonCapability()=default;
private:std::shared_ptr<State> state_;
 friend Outcome<PreparedStudyLessonCapability> prepare_study_lesson_capability(const CharacterConditionSources&,const GrowthState&,const GrowthFixedStepPermit&);
 friend Outcome<Integer> read_prepared_study_lesson_xp(const PreparedStudyLessonCapability&,const CommonStore&,const GrowthState&,const ActionCoreView&,const GrowthFixedStepPermit&);
 friend Status record_study_lesson_capability_in_candidate(CommonStore&,GrowthState&,const AuthoredActorFoundationState&,const GrowthFixedStepPermit&,PreparedStudyLessonCapability&);
};
Outcome<PreparedStudyLessonCapability> prepare_study_lesson_capability(const CharacterConditionSources&,const GrowthState&,const GrowthFixedStepPermit&);
// 原step只消费sealed前值；返回原baseXP，innate/表现/上限仍在原writer各算一次。
Outcome<Integer> read_prepared_study_lesson_xp(const PreparedStudyLessonCapability&,const CommonStore&,const GrowthState&,const ActionCoreView&,const GrowthFixedStepPermit&);
// 原lesson/Condition都已真正落定后追加实际来源；技术失败同candidate回滚。
Status record_study_lesson_capability_in_candidate(CommonStore&,GrowthState&,const AuthoredActorFoundationState&,const GrowthFixedStepPermit&,PreparedStudyLessonCapability&);
Outcome<GrowthStep> step_growth_study_at_fixed_gate(CommonStore&,GrowthState&,const GrowthCatalog&,const CommonCatalog&,SocialState&,InteractionState&,const GrowthAccess&,const ActionCoreView&,const GrowthFixedStepPermit&,const PreparedStudyLessonCapability&);
Outcome<const LessonCapabilitySource*> read_study_lesson_capability_source(const CharacterConditionSources&,const GrowthState&,const StableId& action_id,Integer ordinal);
Status validate_study_lesson_capability_sources(const CharacterConditionSources&,const GrowthState&);
struct GrowthFixedStudyApplication {GrowthStep result;std::vector<BoundActorConditionWrite> actual_condition_writes;};
Outcome<GrowthFixedStudyApplication> apply_study_fixed_gate_in_candidate(CommonStore&,GrowthState&,AuthoredActorFoundationState&,const CharacterConditionSources&,const GrowthCatalog&,const CommonCatalog&,SocialState&,InteractionState&,const GrowthAccess&,const ActionTimeRegistration&,const GrowthFixedStepPermit&);

}
