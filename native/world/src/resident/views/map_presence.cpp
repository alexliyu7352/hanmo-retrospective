// 精确同地观察只投影当前已经存在的人物；查询不推进时间、不分配ID、不更改知情记录。
#include "map_presence.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// 跟随/军伍先由唯一CommonStore解析；解析后仍非物理位置则不创建伪造锚点。
std::optional<MapPresenceLocationKey> location_key(const Location& location){
    if(const auto* value=std::get_if<SettlementLocation>(&location))return MapPresenceLocationKey{0,value->settlement_id,value->place_id,0,0};
    if(const auto* value=std::get_if<OnRouteLocation>(&location))return MapPresenceLocationKey{1,value->edge_id,value->origin,value->progress,0};
    if(const auto* value=std::get_if<WildernessLocation>(&location))return MapPresenceLocationKey{2,value->map_id,value->origin,value->point.x,value->point.y};
    return {};
}
// 运行时代次也参与比较，跨读档相同revision不能复用旧页。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 错误保持明确的查询域，不把无来源或旧页伪装成合法空结果。
Outcome<MapPresencePage> failure(const StableId& code){return {false,{},common_failure(code).error};}
}

MapPresenceIndex build_map_presence_index(const CommonStore& common){
    MapPresenceIndex index;index.identity=common.state().identity;
    for(const auto& id:common.actor_order()){
        const auto* person=common.actor(id);if(!person||!person->alive)continue;
        const auto resolved=common.resolved_location(id);if(!resolved.ok)continue;
        const auto key=location_key(resolved.value);if(key)index.actors[*key].push_back(id);
    }
    return index;
}

Outcome<MapPresencePage> query_map_presence(const CommonStore& common,const TravelCatalog& catalog,const TravelServices& services,const PresentationRead& names,const MapPresenceIndex& index,const StableId& observer,const std::optional<MapPresenceCursor>& cursor){
    if(observer.empty()||observer!=common.state().metadata.player_actor_id||observer!=services.player_actor_id())return failure("MapPresenceObserverRequired");
    const auto* actor=common.actor(observer);if(!actor||!actor->alive)return failure("MapPresenceObserverUnavailable");
    const auto resolved=common.resolved_location(observer);if(!resolved.ok)return {false,{},resolved.error};
    const auto key=location_key(resolved.value);if(!key)return failure("InvalidMapPresenceLocation");
    if(observer.size()>4096||key->primary.size()>4096||key->secondary.size()>4096)return failure("MapPresenceTextLimit");
    const auto identity=common.state().identity;
    if(!same_identity(index.identity,identity))return failure("StaleMapPresenceIndex");
    Integer offset=0;
    if(cursor){
        if(!same_identity(cursor->identity,identity)||cursor->observer_actor_id!=observer||!(cursor->location==*key))return failure("StaleMapPresenceCursor");
        offset=cursor->offset;
    }
    const std::vector<StableId> empty;const auto found=index.actors.find(*key);const auto& people=found==index.actors.end()?empty:found->second;
    if(people.size()>static_cast<std::size_t>(std::numeric_limits<Integer>::max()))return failure("MapPresenceLimit");
    const auto total=static_cast<Integer>(people.size());if(offset<0||offset>total)return failure("InvalidMapPresenceCursor");
    const auto position=query_journey_map_position(common,catalog,services,observer);if(!position.ok)return {false,{},position.error};
    MapPresencePage page;page.identity=identity;page.observer_actor_id=observer;page.location=*key;page.position=position.value;page.cursor=offset;
    while(offset<total&&page.scanned<map_presence_scan_limit&&page.rows.size()<static_cast<std::size_t>(map_presence_page_limit)){
        const auto& id=people[static_cast<std::size_t>(offset++)];++page.scanned;
        if(id==observer)continue;
        const auto* person=common.actor(id);const auto at=common.resolved_location(id);const auto current=at.ok?location_key(at.value):std::nullopt;
        if(!person||!person->alive||!current||!(*current==*key))return failure("InvalidMapPresenceIndex");
        if(id.size()>4096)return failure("MapPresenceTextLimit");
        const auto name=names.display_name(id);if(!name.ok)return {false,{},name.error};
        if(name.value.size()>4096)return failure("MapPresenceTextLimit");
        page.rows.push_back({id,name.value,common.tick()});
    }
    if(offset<total)page.next_cursor=MapPresenceCursor{identity,observer,*key,offset};
    return {true,std::move(page),{}};
}
}
