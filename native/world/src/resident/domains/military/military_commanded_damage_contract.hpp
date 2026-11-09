// 统率与兵种技艺只借同候选的真实人物初源及永久课程史，不建立第二份经验账。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct MilitaryCommandedSkillObservation {
 WorldIdentity source_identity;
 StableId actor_id,skill_id,before_fact_id,authored_actor_id;
 Integer observed_tick=0,leadership=0,experience=0,level=0;
 std::vector<StableId> causal_fact_ids;
};
// current仅供真实contact_install严格单步栈；历史只认原military_rout_entry精确事实前界。
class MilitaryCommandedSkillRead {
public:
 virtual ~MilitaryCommandedSkillRead()=default;
 virtual Outcome<MilitaryCommandedSkillObservation> current_commanded_skill(const StableId& actor_id,const StableId& skill_id)const=0;
 virtual Outcome<MilitaryCommandedSkillObservation> commanded_skill_before_entry(const StableId& actor_id,const StableId& skill_id,const StableId& entry_fact_id)const=0;
};
}
