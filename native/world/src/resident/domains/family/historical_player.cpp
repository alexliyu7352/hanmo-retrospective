#include "historical_player.hpp"
#include "voluntary_succession.hpp"
#include "resident/domains/family/family_model.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
Outcome<StableId> family_player_before_fact(const CommonStore& common,const FamilyState& family,Integer tick,const StableId& before_fact_id){
    if(tick<0||tick>common.tick())return {false,{},common_failure("InvalidFamilyHistoryTick").error};
    const auto& order=common.state().facts.order();
    const auto boundary=std::find(order.begin(),order.end(),before_fact_id);
    if(before_fact_id.empty()||boundary==order.end())return {false,{},common_failure("UnknownFamilyHistoryBoundary",before_fact_id).error};
    StableId current=family.founder_actor_id,result=current;
    if(current.empty()||!common.actor(current))return {false,{},common_failure("InvalidFamilyFounder").error};
    for(const auto& row:family.facts.rows())if(const auto* created=std::get_if<CharacterCreationFact>(&row.second);created&&created->actor_id==current){
        const auto at=std::find(order.begin(),order.end(),row.first);
        if(at==order.end()||at>=boundary||created->created_tick>tick)return {false,{},common_failure("FamilyHistoryBeforeCreation",row.first).error};
    }
    std::set<StableId> seen{current};Integer previous_tick=0;std::size_t previous_position=0;bool first=true;
    for(const auto& id:family.successions){
        const auto step=family_succession_step(family,id);
        const auto* head=common.state().facts.find(id);const auto at=std::find(order.begin(),order.end(),id);
        if(!step.ok||!head||(head->kind!="family_succession"&&head->kind!="family_voluntary_succession")||head->domain!="family"||at==order.end()||
           step.value.predecessor_actor_id!=current||!common.actor(step.value.successor_actor_id)||!seen.insert(step.value.successor_actor_id).second||step.value.tick<previous_tick||step.value.tick>common.tick())return {false,{},common_failure("InvalidSuccessionChain",id).error};
        const auto position=static_cast<std::size_t>(at-order.begin());
        if(!first&&position<=previous_position)return {false,{},common_failure("InvalidSuccessionHistoryOrder",id).error};
        current=step.value.successor_actor_id;previous_tick=step.value.tick;previous_position=position;first=false;
        if(at<boundary&&step.value.tick<=tick)result=current;
    }
    if(current!=common.state().metadata.player_actor_id)return {false,{},common_failure("InvalidSuccessionPlayer").error};
    return {true,std::move(result),{}};
}
}
