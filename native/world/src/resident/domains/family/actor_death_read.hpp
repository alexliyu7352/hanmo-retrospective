// 稳定生命账的具名只读投影；原Military/Naval及FamilyLife完整验证必须先完成。
#pragma once
#include "family_death.hpp"
#include <algorithm>
namespace hanmo::resident {
struct ActorDeathRead { WorldIdentity source_identity;StableId actor_id,death_fact_id,cause_fact_id,battle_id,before_fact_id;Integer death_tick=0,recorded_tick=0,observed_tick=0; };
namespace actor_death_detail {
// 实际生命登记和死因均必须先于消费边界；不接受phase.to或任意未来时钟。
inline Outcome<std::optional<ActorDeathRead>> read(const CommonStore& c,const FamilyDeathState& life,const StableId& actor,Integer tick,const StableId& boundary,bool historical){
    auto fail=[&](const StableId& code)->Outcome<std::optional<ActorDeathRead>>{return {false,{},common_failure(code,actor).error};};const auto* person=c.actor(actor);if(!person)return fail("UnknownDeathActor");if(tick<0||tick>c.tick()||(!historical&&(tick!=c.tick()||!boundary.empty())))return fail("ActorDeathBoundaryMismatch");const auto& order=c.state().facts.order();const auto limit=historical?std::find(order.begin(),order.end(),boundary):order.end();if(historical&&(boundary.empty()||limit==order.end()||!c.state().facts.find(boundary)||c.state().facts.find(boundary)->id!=boundary))return fail("ActorDeathFactBoundaryRequired");
    const FamilyDeathRecord* record=nullptr;StableId death;for(const auto& row:life.deaths.rows())if(row.second.actor_id==actor){if(record)return fail("DuplicateActorDeathSource");record=&row.second;death=row.first;}if(!record)return person->alive?Outcome<std::optional<ActorDeathRead>>{true,std::nullopt,{}}:fail("ActorDeathSourceUnknown");const auto& d=*record;const auto* head=c.state().facts.find(death);const auto* cause=c.state().facts.find(d.cause_fact_id);const auto at=std::find(order.begin(),order.end(),death),original=std::find(order.begin(),order.end(),d.cause_fact_id);if(person->alive||!head||head->id!=death||head->kind!="family_death"||head->domain!="family_life"||d.kind!="family_death"||d.cause_kind!="military_casualty"||!cause||cause->id!=d.cause_fact_id||cause->kind!="military_casualties"||cause->domain!="military"||at==order.end()||original==order.end()||original>=at||d.world_id!=c.state().identity.world_id||d.scenario_id!=c.state().metadata.scenario_id||d.death_tick<0||d.recorded_tick<d.death_tick||d.recorded_tick>c.tick())return fail("InvalidActorDeathSource");
    if(historical&&at>=limit){if(d.recorded_tick<tick)return fail("ActorDeathHistoricalTimeMismatch");return {true,std::nullopt,{}};}if(d.recorded_tick>tick)return fail("ActorDeathHistoricalTimeMismatch");return {true,ActorDeathRead{c.state().identity,actor,death,d.cause_fact_id,d.battle_id,boundary,d.death_tick,d.recorded_tick,tick},{}};
}
}
// 当前只借已完成clock_commit及正式死亡登记的稳定World，缺登记不能用alive代替。
inline Outcome<std::optional<ActorDeathRead>> read_actor_death(const CommonStore& c,const FamilyDeathState& life,const StableId& actor){return actor_death_detail::read(c,life,actor,c.tick(),{},false);}
// 消费方先核自身真实业务payload.tick；此口只证明那一时点之前已有原死亡登记。
inline Outcome<std::optional<ActorDeathRead>> read_actor_death_before_fact(const CommonStore& c,const FamilyDeathState& life,const StableId& actor,Integer tick,const StableId& before_fact){return actor_death_detail::read(c,life,actor,tick,before_fact,true);}
}
