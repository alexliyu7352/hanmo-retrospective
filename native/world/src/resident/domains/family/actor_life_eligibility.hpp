// 本人历史生命资格与死亡登记通知分开；真实死因先后决定是否还能作出决定。
#pragma once
#include "actor_death_read.hpp"
namespace hanmo::resident {
// 本人在旧业务边界是否在世按真实死因先后判定，不能把死因后/登记前窗口冒成可签字。
// 仅稳定World，原军史/FamilyLife须先验；出生/成年等身份准入仍由调用域独立核。
inline Outcome<bool> read_actor_alive_before_fact(const CommonStore& c,const FamilyDeathState& life,const StableId& actor,Integer tick,const StableId& boundary){
    auto bad=[&](const StableId& code)->Outcome<bool>{return {false,false,common_failure(code,actor).error};};
    const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),boundary);const auto* head=c.state().facts.find(boundary);
    if(tick<0||tick>c.tick()||boundary.empty()||!head||head->id!=boundary||at==order.end())return bad("ActorAliveFactBoundaryRequired");
    auto death=read_actor_death(c,life,actor);if(!death.ok)return {false,false,death.error};if(!death.value)return {true,true,{}};
    const auto cause=std::find(order.begin(),order.end(),death.value->cause_fact_id);if(cause==order.end())return bad("InvalidActorDeathSource");
    const bool died=cause<at;if((died&&death.value->death_tick>tick)||(!died&&death.value->death_tick<tick))return bad("ActorAliveHistoricalTimeMismatch");
    return {true,!died,{}};
}

}
