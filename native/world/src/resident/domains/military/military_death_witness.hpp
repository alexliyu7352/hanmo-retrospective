// 只证明原战斗中同部亲历，不产生消息、关系或“全军皆知”的隐含知识。
#pragma once
#include "military_roster_history.hpp"
namespace hanmo::resident {
struct NavalState;
struct MilitaryDeathWitness {
 StableId observer_actor_id,dead_actor_id,casualty_fact_id,battle_id,unit_id;Integer observed_tick=0;
};
// 调用者先验证原军事/Naval历史。observer须当轮后仍存活，后来死亡不抹去旧亲历。
// 当下是否能发信/接收由消息域再查；同军、现位置、旁观另一单位都不能得到本证明。
Outcome<std::optional<MilitaryDeathWitness>> query_military_actor_death_witness(
 const CommonStore&,const MilitaryState&,const NavalState*,const StableId& observer_actor_id,
 const StableId& dead_actor_id,const StableId& casualty_fact_id);
}
