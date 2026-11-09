// 地图/行旅只投影已提交typed来源；当前位置、费用与剩余路径都不由显示查询重新结算。
#include "journey_motion.hpp"
#include "hanmo/resident_world_api.hpp"
#include <algorithm>
#include <string_view>
#include <unordered_set>
namespace hanmo::resident {
namespace {
// 显示源错误不回传半页，也不以缺省姓名/坐标掩盖缺失来源。
template<class T> Outcome<T> fail(const StableId& code,const StableId& source={}){return {false,{},{{code,source},ErrorKind::rule,"query","travel",{},source}};}
// 错误原值从所属服务透传，查询不把未绑定或规则错误吞成不存在。
template<class T,class U> Outcome<T> propagate(const Outcome<U>& value){return {false,{},value.error};}
// 每页最多64项，cursor==total是合法空尾页；不在分页前复制完整集合。
template<class Page> bool page_bounds(Page& page,Integer cursor,Integer total){if(cursor<0||cursor>total)return false;page.cursor=cursor;page.total_rows=total;if(total-cursor>static_cast<Integer>(journey_page_limit))page.next_cursor=cursor+static_cast<Integer>(journey_page_limit);return true;}
// 只查本人当前行动头；字段缺失由调用者区分空闲和坏引用。
const ActionCoreView* action(const CommonRead& common,const ActorCoreView& actor){return actor.action_id.empty()?nullptr:common.action(actor.action_id);}
// 附属人物只借其本人真实参与的共同travel位置；不改raw身份，不借无关统帅行动扩大观察。
const ActorCoreView* travel_position_actor(const CommonRead& common,const ActorCoreView& observer,const ActionCoreView& current){
    if(!std::holds_alternative<FollowingLocation>(observer.location)&&!std::holds_alternative<InArmyLocation>(observer.location))return &observer;
    const auto* leader=common.actor(current.actor_id);
    if(!leader||leader->id==observer.id||leader->action_id!=current.id||observer.action_id!=current.id||std::holds_alternative<FollowingLocation>(leader->location)||std::holds_alternative<InArmyLocation>(leader->location))return nullptr;
    if(std::count(current.participants.begin(),current.participants.end(),observer.id)!=1||std::count(current.participants.begin(),current.participants.end(),leader->id)!=1)return nullptr;
    const auto* members=common.members(leader->id);
    if(!members||std::count(members->begin(),members->end(),observer.id)!=1||std::count(members->begin(),members->end(),leader->id)!=1)return nullptr;
    return leader;
}
}
// raw路由字段沿原SceneJourneySidebar._common，不提前解析跟随者或军团领队。
Outcome<JourneyLocationSummary> query_journey_location(const CommonRead& common,const StableId& observer){
    auto field=common.field_battle_bound(observer);if(!field.ok)return {false,{},field.error};if(field.value)return fail<JourneyLocationSummary>("FieldBattleLocationRequired",observer);
    const auto* actor=common.actor(observer);if(!actor)return fail<JourneyLocationSummary>("UnknownActor",observer);
    JourneyLocationSummary result;bool oversized=false;
    std::visit([&](const auto& location){using T=std::decay_t<decltype(location)>;
        if constexpr(std::is_same_v<T,SettlementLocation>){result.kind="Settlement";oversized=location.place_id.size()>4096||location.settlement_id.size()>4096;if(!oversized){result.place_id=location.place_id;result.settlement_id=location.settlement_id;}}
        else if constexpr(std::is_same_v<T,WildernessLocation>)result.kind="Wilderness";
        else if constexpr(std::is_same_v<T,FollowingLocation>)result.kind="Following";
        else if constexpr(std::is_same_v<T,InArmyLocation>)result.kind="InArmy";
        else result.kind="OnRoute";
    },actor->location);
    if(oversized)return fail<JourneyLocationSummary>("JourneyViewTextLimit");return {true,std::move(result),{}};
}
// 只求当前resolved整数坐标；地图显示Vector2转换由D主线程边界负责，绝不反写世界。
Outcome<JourneyMapPosition> query_journey_map_position(const CommonRead& common,const TravelCatalog& catalog,const TravelServices& services,const StableId& observer){
    auto field=common.field_battle_bound(observer);if(!field.ok)return {false,{},field.error};if(field.value)return fail<JourneyMapPosition>("FieldBattleLocationRequired",observer);
    if(!common.actor(observer))return fail<JourneyMapPosition>("UnknownActor",observer);
    if(catalog.world_map_id.size()>4096)return fail<JourneyMapPosition>("JourneyViewTextLimit");
    if(!catalog.world_map_id.empty()&&!catalog.maps.find(catalog.world_map_id))return fail<JourneyMapPosition>("InvalidMapViewSource",observer);
    auto resolved=services.resolved_location(observer);if(!resolved.ok)return propagate<JourneyMapPosition>(resolved);
    JourneyMapPosition result;result.world_map_id=catalog.world_map_id;
    if(const auto* wilderness=std::get_if<WildernessLocation>(&resolved.value)){
        if(wilderness->map_id!=catalog.world_map_id)return fail<JourneyMapPosition>("InvalidMapViewSource",observer);
        result.observer_position=wilderness->point;
    }else if(const auto* settlement=std::get_if<SettlementLocation>(&resolved.value)){
        const auto* definition=catalog.settlements.find(settlement->settlement_id);if(!definition)return fail<JourneyMapPosition>("InvalidMapViewSource",observer);
        result.observer_position=definition->map_xy;
    }else if(const auto* location=std::get_if<OnRouteLocation>(&resolved.value)){
        const auto* edge=catalog.roads.find(location->edge_id);const auto* origin=edge?catalog.settlements.find(edge->origin):nullptr;const auto* target=edge?catalog.settlements.find(edge->destination):nullptr;
        if(!edge||!origin||!target||edge->duration_ticks<1||edge->shape.size()>64||location->progress<0||location->progress>edge->duration_ticks*100)return fail<JourneyMapPosition>("InvalidMapViewSource",observer);
        if(edge->id.size()>4096)return fail<JourneyMapPosition>("JourneyViewTextLimit");
        JourneyRoadPosition road;road.edge_id=edge->id;road.progress=location->progress;road.length=edge->duration_ticks*100;road.raw=std::holds_alternative<OnRouteLocation>(common.actor(observer)->location);road.points.push_back(origin->map_xy);road.points.insert(road.points.end(),edge->shape.begin(),edge->shape.end());road.points.push_back(target->map_xy);result.road=std::move(road);
    }
    else return fail<JourneyMapPosition>("InvalidMapViewSource",observer);
    return {true,std::move(result),{}};
}
// 只有active地形行动读取其保存参数；停在原野但无行动不会伪报正在旅行。
Outcome<JourneyTravelSummary> query_journey_travel(const CommonRead& common,const TravelState& state,const StableId& observer){
    auto field=common.field_battle_bound(observer);if(!field.ok)return {false,{},field.error};if(field.value)return fail<JourneyTravelSummary>("FieldBattleLocationRequired",observer);
    const auto* actor=common.actor(observer);if(!actor)return fail<JourneyTravelSummary>("UnknownActor",observer);
    const auto* current=action(common,*actor);if(!actor->action_id.empty()&&!current)return fail<JourneyTravelSummary>("InvalidJourneyViewSource",observer);
    JourneyTravelSummary result;if(!current||current->kind!="travel")return {true,std::move(result),{}};
    const auto* position_actor=travel_position_actor(common,*actor,*current);if(!position_actor)return fail<JourneyTravelSummary>("InvalidJourneyViewSource",observer);
    if(const auto* road=state.road_actions.find(current->id)){auto location=std::get_if<OnRouteLocation>(&position_actor->location);if(!location)return fail<JourneyTravelSummary>("InvalidJourneyViewSource",observer);if(location->edge_id.size()>4096)return fail<JourneyTravelSummary>("JourneyViewTextLimit");result.active=true;result.road=JourneyRoadSummary{location->edge_id,location->progress,road->paused};return {true,std::move(result),{}};}
    const auto* params=state.actions.find(current->id);if(!params||!std::holds_alternative<WildernessLocation>(position_actor->location))return fail<JourneyTravelSummary>("InvalidJourneyViewSource",observer);
    if(const auto* id=std::get_if<StableId>(&params->destination);id&&id->size()>4096)return fail<JourneyTravelSummary>("JourneyViewTextLimit");
    result.active=true;result.terrain=JourneyTerrainSummary{params->paused,current->remaining_ticks,params->destination};
    return {true,std::move(result),{}};
}
// 当前点占第0项，其余逐项读取已保存后缀；页间不重复边界点，显示组合可直接追加。
Outcome<JourneyRoutePage> query_journey_route(const CommonRead& common,const TravelState& state,const StableId& observer,Integer cursor){
    auto field=common.field_battle_bound(observer);if(!field.ok)return {false,{},field.error};if(field.value)return fail<JourneyRoutePage>("FieldBattleLocationRequired",observer);
    const auto* actor=common.actor(observer);if(!actor)return fail<JourneyRoutePage>("UnknownActor",observer);
    const auto* current=action(common,*actor);if(!actor->action_id.empty()&&!current)return fail<JourneyRoutePage>("InvalidMapViewSource",observer);
    JourneyRoutePage result;
    if(!current||current->kind!="travel"){if(!page_bounds(result,cursor,0))return fail<JourneyRoutePage>("InvalidJourneyCursor");return {true,std::move(result),{}};}
    const auto* position_actor=travel_position_actor(common,*actor,*current);if(!position_actor)return fail<JourneyRoutePage>("InvalidMapViewSource",observer);
    if(const auto* road=state.road_actions.find(current->id)){if(road->edge_index<0||road->edge_index>static_cast<Integer>(road->path.size()))return fail<JourneyRoutePage>("InvalidMapViewSource",observer);auto total=static_cast<Integer>(road->path.size())-road->edge_index;if(!page_bounds(result,cursor,total))return fail<JourneyRoutePage>("InvalidJourneyCursor");result.active=true;result.road=true;result.returning=road->returning;for(Integer i=cursor;i<std::min(total,cursor+static_cast<Integer>(journey_page_limit));++i){const auto& id=road->path[static_cast<std::size_t>(road->edge_index+i)];if(id.size()>4096)return fail<JourneyRoutePage>("JourneyViewTextLimit");result.path.push_back(id);}return {true,std::move(result),{}};}
    const auto* params=state.actions.find(current->id);const auto* location=std::get_if<WildernessLocation>(&position_actor->location);
    if(!params||!location||location->map_id!=params->map_id)return fail<JourneyRoutePage>("InvalidMapViewSource",observer);
    const auto total=static_cast<Integer>(params->points.size())+1;if(!page_bounds(result,cursor,total))return fail<JourneyRoutePage>("InvalidJourneyCursor");
    result.active=true;const auto end=std::min(total,cursor+static_cast<Integer>(journey_page_limit));result.points.reserve(static_cast<std::size_t>(end-cursor));
    for(Integer i=cursor;i<end;++i)result.points.push_back(i==0?location->point:params->points[static_cast<std::size_t>(i-1)]);
    return {true,std::move(result),{}};
}
// 只扫描选中地图的作者cells，首次出现顺序即旧Dictionary写入序；临时key借用目录，不复制世界flags。
Outcome<JourneyCrossingPage> query_journey_crossings(const TravelCatalog& catalog,const TravelServices& services,Integer cursor,Integer tick){
    if(cursor<0)return fail<JourneyCrossingPage>("InvalidJourneyCursor");
    if(catalog.world_map_id.empty()){JourneyCrossingPage result;result.road=true;auto total=static_cast<Integer>(catalog.roads.size());if(!page_bounds(result,cursor,total))return fail<JourneyCrossingPage>("InvalidJourneyCursor");for(Integer i=cursor;i<std::min(total,cursor+static_cast<Integer>(journey_page_limit));++i){const auto& row=catalog.roads.rows()[static_cast<std::size_t>(i)];if(row.first.size()>4096)return fail<JourneyCrossingPage>("JourneyViewTextLimit");result.rows.push_back({row.first,!road_is_open(row.second,tick)});}return {true,std::move(result),{}};}
    const auto* map=catalog.maps.find(catalog.world_map_id);if(!map)return fail<JourneyCrossingPage>("InvalidMapViewSource",catalog.world_map_id);
    const auto& flags=services.terrain_flags();std::unordered_set<std::string_view> seen;JourneyCrossingPage result;Integer total=0;
    for(const auto& cell:map->cells){if(!cell.crossing||!cell.blocked_flag||!seen.insert(*cell.blocked_flag).second)continue;
        if(total>=cursor&&result.rows.size()<journey_page_limit){const auto& id=*cell.blocked_flag;if(id.size()>4096)return fail<JourneyCrossingPage>("JourneyViewTextLimit");const auto found=flags.find(id);result.rows.push_back({id,found==flags.end()?false:found->second});}
        ++total;
    }
    if(!page_bounds(result,cursor,total))return fail<JourneyCrossingPage>("InvalidJourneyCursor");return {true,std::move(result),{}};
}
// party存在性与同行名字分离；只解析本页实际同行，领队与页外成员不产生表现查询。
Outcome<JourneyCompanionPage> query_journey_companions(const CommonState& state,const TravelServices& services,const PresentationRead& presentation,const StableId& observer,Integer cursor){
    if(!state.actors.find(observer))return fail<JourneyCompanionPage>("UnknownActor",observer);
    const auto party_id=services.party_id(observer);JourneyCompanionPage result;
    if(party_id.empty()){if(!page_bounds(result,cursor,0))return fail<JourneyCompanionPage>("InvalidJourneyCursor");return {true,std::move(result),{}};}
    const auto* party=state.parties.find(party_id);if(!party||std::find(party->members.begin(),party->members.end(),observer)==party->members.end())return fail<JourneyCompanionPage>("InvalidJourneyViewSource",observer);
    const auto total=static_cast<Integer>(std::count_if(party->members.begin(),party->members.end(),[&](const StableId& id){return id!=party->leader_id;}));
    if(!page_bounds(result,cursor,total))return fail<JourneyCompanionPage>("InvalidJourneyCursor");result.party_present=true;Integer index=0;
    for(const auto& id:party->members){if(id==party->leader_id)continue;if(index++<cursor)continue;if(result.rows.size()==journey_page_limit)break;
        if(id.size()>4096)return fail<JourneyCompanionPage>("JourneyViewTextLimit");
        const auto* terms=party->terms.find(id);if(!state.actors.find(id)||!terms)return fail<JourneyCompanionPage>("InvalidJourneyViewSource",id);
        auto name=presentation.display_name(id);if(!name.ok)return propagate<JourneyCompanionPage>(name);
        if(name.value.size()>4096)return fail<JourneyCompanionPage>("JourneyViewTextLimit");
        result.rows.push_back({id,std::move(name.value),terms->cost_per_tick});
    }
    return {true,std::move(result),{}};
}
} // 页外世界不会被复制；共同资源/clock/RNG/行动参数均保持只读。
