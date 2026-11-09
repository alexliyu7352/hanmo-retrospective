// 陆战退出回执只引用实际伤亡和原接敌旅程；不添加ActionCompletion存档字段。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// 只看真实已有会战引用的本战场策略；合包中未参战的新定义不启用旧世界路径。
bool military_field_commander_profile_in_use(const MilitaryState&,const MilitaryCatalog&);
struct MilitaryJourneyTermination { StableId action_id,actor_id,battle_id,source_fact_id,reason;Integer tick=0; };
// 在原军事历史校验之后按原casualty精确查询；旧战场/静止军团返回空，不产生新失败原因。
Outcome<std::vector<MilitaryJourneyTermination>> query_military_journey_terminations(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& casualty_fact_id);
// 冷计划来源按source/action/actor三元组核，不能用最新一条或当前位置代替。
Outcome<std::optional<MilitaryJourneyTermination>> query_military_journey_termination(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& casualty_fact_id,const StableId& action_id,const StableId& actor_id);
}
