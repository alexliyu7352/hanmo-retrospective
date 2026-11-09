// 人物存在与当时生死的只读来源；不附加年龄、同意、知情、可接触或财产授权。
#pragma once
#include "family_death.hpp"
#include "resident/model/dynamic_contract.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
struct ActorPresenceRead {
    WorldIdentity source_identity;
    StableId actor_id,before_fact_id,creation_fact_id,death_cause_fact_id;
    Integer observed_tick=0,created_tick=0;
    bool alive=false;
};
namespace actor_presence_detail {
// 动态人物和原死因必须是真实共同分配实例；数值仅核分配，不代事实前序。
inline bool allocated(const CommonStore& c,const StableId& id,const char* prefix){
    const auto lead=StableId{prefix}+":";if(id.rfind(lead,0)!=0)return false;const auto digits=id.substr(lead.size());Integer n=0;auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),n);return parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&n>0&&n<c.state().next_instance_id&&std::to_string(n)==digits;
}
// 完整同候选Common及选择的Family/Creation/Birth、Military/Naval史是前置，不能仅用本头验证整世界。
// 业务方先核其实际边界payload.tick；死因回调须借本候选原军事纯读，phase由World同栈绑定。
inline Outcome<ActorPresenceRead> read(const CommonStore& c,const CommonCatalog& catalog,const FamilyDeathOriginRead& deaths,const StableId& actor,Integer tick,const StableId& boundary,const DynamicStepInput* phase,bool historical){
    auto fail=[&](const StableId& code)->Outcome<ActorPresenceRead>{return {false,{},common_failure(code,actor).error};};
    Integer through=c.tick();if(phase){if(c.tick()<0||c.tick()==std::numeric_limits<Integer>::max()||phase->from_tick!=c.tick()||phase->to_tick!=c.tick()+1)return fail("ActorPresencePhaseInvalid");through=phase->to_tick;}
    if(tick<0||tick>through||(!historical&&(tick!=through||!boundary.empty())))return fail("ActorPresenceTimeInvalid");
    const auto* person=c.state().actors.find(actor);if(!person||person->id!=actor)return fail("UnknownPresenceActor");
    const auto& order=c.state().facts.order();const auto limit=historical?std::find(order.begin(),order.end(),boundary):order.end();
    if(historical){const auto* head=c.state().facts.find(boundary);if(boundary.empty()||limit==order.end()||!head||head->id!=boundary)return fail("ActorPresenceBoundaryRequired");}
    ActorPresenceRead out;out.source_identity=c.state().identity;out.actor_id=actor;out.before_fact_id=boundary;out.observed_tick=tick;
    const bool authored=std::find(catalog.actor_ids.begin(),catalog.actor_ids.end(),actor)!=catalog.actor_ids.end();
    if(authored){if(person->created_tick&&*person->created_tick!=0)return fail("ActorPresenceAuthoredOriginMismatch");}
    else{
        const auto* created=c.state().facts.find(actor);const auto at=std::find(order.begin(),order.end(),actor);
        if(!allocated(c,actor,"actor")||!person->created_tick||*person->created_tick<0||*person->created_tick>tick||!created||created->id!=actor||created->kind!="actor_creation"||(created->domain!="family"&&created->domain!="family_birth")||at==order.end()||(historical&&at>=limit))return fail("ActorPresenceCreationSourceRequired");
        out.creation_fact_id=actor;out.created_tick=*person->created_tick;
    }
    // 无本源的旧profile只可证明仍活着的人；今日已死而无永久死因必须明确拒绝。
    if(!deaths){if(!person->alive)return fail("ActorPresenceDeathSourceUnknown");out.alive=true;return {true,std::move(out),{}};}
    auto source=deaths(actor);if(!source.ok)return {false,{},source.error};
    if(!source.value){if(!person->alive)return fail("ActorPresenceDeathSourceUnknown");out.alive=true;return {true,std::move(out),{}};}
    const auto& death=*source.value;const auto* head=c.state().facts.find(death.casualty_fact_id);const auto died=std::find(order.begin(),order.end(),death.casualty_fact_id);
    if(person->alive||death.actor_id!=actor||death.battle_id.empty()||death.tick<out.created_tick||death.tick>through||!allocated(c,death.casualty_fact_id,"military")||!head||head->id!=death.casualty_fact_id||head->kind!="military_casualties"||head->domain!="military"||died==order.end()||(!out.creation_fact_id.empty()&&died<=std::find(order.begin(),order.end(),out.creation_fact_id)))return fail("ActorPresenceDeathSourceInvalid");
    const bool before=!historical||died<limit;
    if((before&&death.tick>tick)||(!before&&death.tick<tick))return fail("ActorPresenceDeathTimeMismatch");
    out.alive=!before;if(before)out.death_cause_fact_id=death.casualty_fact_id;return {true,std::move(out),{}};
}
}
// 当前只提供同候选当时事实，不以出生年份未知限制普通会面资格。
inline Outcome<ActorPresenceRead> read_actor_presence(const CommonStore& c,const CommonCatalog& catalog,const FamilyDeathOriginRead& deaths,const StableId& actor,const DynamicStepInput* phase=nullptr){return actor_presence_detail::read(c,catalog,deaths,actor,phase?phase->to_tick:c.tick(),{},phase,false);}
// 实际头前读，后来死亡保留先前合法联系；FamilyLife较晚登记不能掩盖已经发生的原死因。
inline Outcome<ActorPresenceRead> read_actor_presence_before_fact(const CommonStore& c,const CommonCatalog& catalog,const FamilyDeathOriginRead& deaths,const StableId& actor,Integer tick,const StableId& boundary,const DynamicStepInput* phase=nullptr){return actor_presence_detail::read(c,catalog,deaths,actor,tick,boundary,phase,true);}
}
