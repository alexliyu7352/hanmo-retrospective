// 筑城技艺只借人物永久经验史；本读专属于动作结算前的原siege_step，不共享入战后的时序假设。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct MilitarySiegeEngineeringSkillObservation {
 WorldIdentity source_identity;
 StableId actor_id,skill_id,before_fact_id,authored_actor_id;
 Integer observed_tick=0,experience=0,level=0;
 std::vector<StableId> causal_fact_ids;
};
// current用于本半日围城准备或稳定报价；phase观察to但XP截止from。历史只认真实工程准备头，所有同to课程均发生在其后。
class MilitarySiegeEngineeringSkillRead {
public:
 virtual ~MilitarySiegeEngineeringSkillRead()=default;
 virtual Outcome<MilitarySiegeEngineeringSkillObservation> current_siege_engineering_skill(const StableId& actor_id)const=0;
 virtual Outcome<MilitarySiegeEngineeringSkillObservation> siege_engineering_skill_before_preparation(const StableId& actor_id,const StableId& preparation_fact_id)const=0;
};
inline constexpr const char* military_siege_engineering_module="military_siege_engineering";
inline constexpr const char* military_siege_engineering_fact_prefix="military_siege_engineering";
inline constexpr const char* military_siege_engineering_fact_kind="military_siege_engineering_prepared";
}
