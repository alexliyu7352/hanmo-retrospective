// 人物年龄只读合同；成年不等于同意、健康、空闲或具有财产/任职权限。
#pragma once
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
struct ActorAgeRead { WorldIdentity source_identity;StableId actor_id,before_fact_id;Integer birth_tick=0,observed_tick=0,age_years=0;bool adult=false; };
namespace actor_age_detail {
// 原生日不重抽。历史边界必须真实存在，动态人物须已在该边界前由原身份事实创建。
inline Outcome<ActorAgeRead> read(const CommonStore& common,const CommonCatalog& catalog,const StableId& id,Integer tick,const StableId& boundary,bool historical){
    auto fail=[&](const StableId& code)->Outcome<ActorAgeRead>{return {false,{},common_failure(code,id).error};};constexpr Integer maximum_tick=2147483647,ticks_per_year=720;const auto* actor=common.state().actors.find(id);if(!actor||actor->id!=id)return fail("UnknownAgeActor");if(tick<0||tick>common.tick()||tick>maximum_tick||!actor->birth_tick||*actor->birth_tick< -maximum_tick||*actor->birth_tick>tick)return fail("ActorAgeUnknown");
    const auto& facts=common.state().facts.order();const auto before=historical?std::find(facts.begin(),facts.end(),boundary):facts.end();if(historical){const auto* head=common.state().facts.find(boundary);if(boundary.empty()||before==facts.end()||!head||head->id!=boundary)return fail("ActorAgeFactBoundaryRequired");}else if(!boundary.empty()||tick!=common.tick())return fail("ActorAgeCurrentBoundaryMismatch");
    const bool authored=std::find(catalog.actor_ids.begin(),catalog.actor_ids.end(),id)!=catalog.actor_ids.end();if(!authored){if(!actor->created_tick||*actor->created_tick<0||*actor->created_tick>tick)return fail("ActorAgeCreationHistoryRequired");const auto* created=common.state().facts.find(id);const auto at=std::find(facts.begin(),facts.end(),id);if(!created||created->id!=id||created->kind!="actor_creation"||(created->domain!="family"&&!(catalog.family_birth_identity_enabled&&created->domain=="family_birth"))||at==facts.end()||(historical&&at>=before))return fail("ActorAgeCreationHistoryRequired");}
    const Integer years=(tick-*actor->birth_tick)/ticks_per_year;return {true,{common.state().identity,id,boundary,*actor->birth_tick,tick,years,years>=18},{}};
}
}
// 当前资格只带本次候选读戳；消费方必须再核本人同意和本业务处境。
inline Outcome<ActorAgeRead> read_actor_age(const CommonStore& common,const CommonCatalog& catalog,const StableId& actor){return actor_age_detail::read(common,catalog,actor,common.tick(),{},false);}
// 调用者先验证实际业务payload.tick等于tick；年龄读不猜Common头没有记录的时间。
inline Outcome<ActorAgeRead> read_actor_age_before_fact(const CommonStore& common,const CommonCatalog& catalog,const StableId& actor,Integer tick,const StableId& before_fact_id){return actor_age_detail::read(common,catalog,actor,tick,before_fact_id,true);}
}
