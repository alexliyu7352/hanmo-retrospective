// 舟战只能借成长域的具名技艺来源；该合同不存XP账、不授船权、不从当前等级补过去。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
// 经验与等级须由同候选成长规则一致推导；作者初态和全部实际成长因果由提供者完整核验。
// authored_actor_id可为空，仅当真实人物创建来源已经包含在causal_fact_ids中。
struct NavalGunnerySkillObservation {
 WorldIdentity source_identity;
 StableId actor_id,skill_id="naval",before_fact_id,authored_actor_id;
 Integer observed_tick=0,experience=0,level=0;
 std::vector<StableId> causal_fact_ids;
};
// 缺乏完整历史必须失败，不能返回今天数值、作者初值或空成功作为降级读。
// 唯一热入口是actions_step之后、clock_commit之前的contact_install，构造适配必须核原DynamicStepInput。
// 没有零时命令或其他phase准备；同tick成长前后只能沿该固定原phase推导。
class NavalGunnerySkillRead {
public:
 virtual ~NavalGunnerySkillRead()=default;
 // 当前军议准备只读此候选的真实船长水战技艺；before_fact_id必须为空。
 virtual Outcome<NavalGunnerySkillObservation> current_naval_skill(const StableId& captain_actor_id)const=0;
 // 仅naval_gunnery_preparation / military_naval / naval_skill:原头前经验史；
 // 真实contact→battle头→紧邻准备头，观察tick由本域准备记录明确给出。
 // 不接受任意tick，观察时刻由该真实Common事实与原phase推导，S2再次核本域载荷。
 virtual Outcome<NavalGunnerySkillObservation> naval_skill_before(
  const StableId& captain_actor_id,const StableId& preparation_fact_id)const=0;
};
}
