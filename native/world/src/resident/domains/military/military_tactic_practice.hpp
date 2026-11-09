// 本人认定只借原真实付费训练的已结题面；不重抽挑战，不让一次训练领取整套战法。
#pragma once
#include "military_tactic_learning_source.hpp"
#include "resident/domains/military/military_refs.hpp"
namespace hanmo::resident {
struct MilitaryTacticPracticeQualification {
 WorldIdentity source_identity;StableId actor_id,skill_id,before_fact_id;
 Integer observed_tick=0,minimum_experience=0,minimum_level=0;
 std::vector<StableId> causal_fact_ids;
};
class MilitaryTacticPracticeQualificationRead {
public:
 virtual ~MilitaryTacticPracticeQualificationRead()=default;
 virtual Outcome<MilitaryTacticPracticeQualification> before_training(const StableId& actor,const StableId& skill,Integer start_tick,const StableId& actual_training_action)const=0;
};
// 原Military训练及drill完整历史须先验；本口再核精确题面、本人、开工资格和实际Common前序。
Outcome<MilitaryTacticPracticeCompletion> read_military_tactic_practice_completion(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryTacticLearningState&,const MilitaryTacticPracticeQualificationRead&,const StableId& exact_practice,const NavalState* naval=nullptr);
Outcome<MilitaryTacticPracticeCompletion> preview_military_tactic_practice(const CommonStore&,const CommonCatalog&,const MilitaryState&,const MilitaryCatalog&,const MilitaryTacticLearningState&,const MilitaryTacticPracticeQualificationRead&,const StableId& actor,const MilitaryTacticPracticeClaim&,const NavalState* naval=nullptr);
Outcome<MilitaryTacticPracticeReceipt> apply_military_tactic_practice_in_candidate(CommonStore&,const CommonCatalog&,const MilitaryState&,const MilitaryCatalog&,MilitaryTacticLearningState&,const MilitaryTacticPracticeQualificationRead&,const StableId& actor,const MilitaryTacticPracticeClaim&,const NavalState* naval=nullptr);
}
