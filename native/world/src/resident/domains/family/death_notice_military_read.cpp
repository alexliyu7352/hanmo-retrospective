// 只引用S2固定亲历证明，当前位置、同军成员和传闻均不成为新增事实来源。
#include "death_notice_military_read.hpp"
#include "resident/domains/military/military_death_witness.hpp"
namespace hanmo::resident {
// 返回值仅复制有限原始身份，保留缺证明与严格历史读取失败，不吞错作已知。
FamilyDeathWitnessRead family_military_death_witness_read(const CommonStore& c,const MilitaryState& military,const NavalState* naval){return [&c,&military,naval](const StableId& observer,const StableId& dead,const StableId& casualty)->Outcome<std::optional<FamilyDeathWitness>>{auto result=query_military_actor_death_witness(c,military,naval,observer,dead,casualty);if(!result.ok)return {false,{},result.error};if(!result.value)return {true,{},{}};const auto& v=*result.value;return {true,FamilyDeathWitness{v.observer_actor_id,v.dead_actor_id,v.casualty_fact_id,v.battle_id,v.unit_id,v.observed_tick},{}};};}
}
