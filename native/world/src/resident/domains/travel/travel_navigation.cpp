// 到指定已知场所的路线复用作者通道和S1准入；不瞬移，也不替调用域接受公职或改行动。
#include "travel_navigation.hpp"
#include <deque>
#include <map>
namespace hanmo::resident {
namespace {
// 失败保留原业务码，错误不被转成“已到达”或默许跨城通道。
template<class T> Outcome<T> failure(const StableId& code){return {false,{},common_failure(code).error};}
// 按作者通道顺序做同城最短步数搜索；未来准入只读当前资格，最终一步仍须真实preview。
Outcome<std::optional<StableId>> local_step(const TravelCatalog& catalog,const TravelServices& services,const std::vector<StableId>& members,const StableId& town,const StableId& start,const StableId& goal,bool admission){
    std::deque<StableId> queue{start};std::map<StableId,StableId> first{{start,{}}};
    while(!queue.empty()){
        const auto current=queue.front();queue.pop_front();const auto* place=catalog.places.find(current);
        if(!place||place->settlement_id!=town)return failure<std::optional<StableId>>("UnknownPlace");
        for(const auto& next:place->passages){
            if(first.count(next))continue;const auto* target=catalog.places.find(next);
            if(!target||target->settlement_id!=town)return failure<std::optional<StableId>>("NoLocalPassage");
            first[next]=current==start?next:first.at(current);
            bool allowed=true;
            if(admission)for(const auto& member:members){auto checked=services.admission_allowed(member,next);if(!checked.ok)return {false,{},checked.error};if(!checked.value){allowed=false;break;}}
            if(!allowed)continue;
            if(next==goal)return {true,first.at(next),{}};
            queue.push_back(next);
        }
    }
    return {true,std::nullopt,{}};
}
}

Outcome<TravelNextStep> next_travel_command(const CommonRead& common,const TravelState& state,const TravelCatalog& catalog,const TravelServices& services,const StableId& actor_id,const StableId& target_place_id){
    const auto* target=catalog.places.find(target_place_id);if(!target)return failure<TravelNextStep>("UnknownPlace");
    const auto* actor=common.actor(actor_id);if(!actor)return failure<TravelNextStep>("UnknownActor");
    if(!actor->alive||!actor->available||!actor->action_id.empty())return failure<TravelNextStep>("ParticipantBusy");
    const auto* town=catalog.settlements.find(target->settlement_id);if(!town)return failure<TravelNextStep>("InvalidDestination");
    TravelNextStep result;result.target_place_id=target_place_id;
    const auto* here=std::get_if<SettlementLocation>(&actor->location);
    if(here&&here->settlement_id==target->settlement_id&&here->place_id==target_place_id)return {true,std::move(result),{}};
    if(here){
        const auto* origin=catalog.settlements.find(here->settlement_id);if(!origin)return failure<TravelNextStep>("InvalidDestination");
        const auto goal=here->settlement_id==target->settlement_id?target_place_id:origin->entry_place;
        if(here->place_id!=goal){
            const auto* members=common.members(actor_id);if(!members)return failure<TravelNextStep>("ParticipantBusy");
            for(const auto& id:*members){const auto* person=common.actor(id);if(!person||!person->alive||!person->available||!person->action_id.empty())return failure<TravelNextStep>("ParticipantBusy");}
            const auto structural=local_step(catalog,services,*members,here->settlement_id,here->place_id,goal,false);
            if(!structural.ok)return {false,{},structural.error};if(!structural.value)return failure<TravelNextStep>("NoLocalPassage");
            const auto next=local_step(catalog,services,*members,here->settlement_id,here->place_id,goal,true);
            if(!next.ok)return {false,{},next.error};if(!next.value)return failure<TravelNextStep>("AudienceDenied");
            const auto preview=preview_move_local(common,catalog,services,actor_id,*next.value);if(!preview.ok)return {false,{},preview.error};
            result.command=TravelNextCommand{MoveLocalCommand{*next.value}};return {true,std::move(result),{}};
        }
    }
    const auto quote=plan_travel(common,state,catalog,services,actor_id,TravelDestination{target->settlement_id});
    if(!quote.ok)return {false,{},quote.error};
    TravelCommand command;command.destination=target->settlement_id;
    result.command=TravelNextCommand{std::move(command)};result.travel_quote=quote.value;
    return {true,std::move(result),{}};
}
}
