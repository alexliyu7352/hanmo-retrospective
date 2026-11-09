// 仅核地图同地观察叶；真实Main与旅行链另有原件，不以有限夹具冒充完整动态世界。
#include "resident/views/map_presence.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 失败保留具体来源条件，测试异常也返回非零。
void check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
// 使用真实CommonStore解析位置，其余服务只允许地图已经需要的读取。
struct Fixture final:TravelServices,PresentationRead {
    std::shared_ptr<CommonCatalog> definitions=std::make_shared<CommonCatalog>();
    CommonStore common{CommonState{},definitions};TravelCatalog catalog;mutable std::vector<StableId> names;
    Fixture(){
        common.state().metadata.player_actor_id="player";common.state().identity.world_id[0]=17;common.state().identity.execution_epoch=2;common.state().identity.revision=3;common.state().identity.tick=7;
        common.state().next_instance_id=83;common.state().rng.insert("world",{17,71,{}});
        add("player",SettlementLocation{"town","gate"});catalog.settlements.insert("town",{"town","gate",{23,31}});catalog.settlements.insert("far",{"far","far-gate",{90,70}});
    }
    // 只创建本测试的typed数据，不把合成地点登记为作者世界。
    void add(const StableId& id,Location location,bool alive=true){ActorRecord actor;actor.id=id;actor.alive=alive;actor.available=true;actor.location=std::move(location);common.state().actors.insert(id,std::move(actor));}
    Outcome<Location> resolved_location(const StableId& id)const override{return common.resolved_location(id);}
    const StableId& player_actor_id()const override{return common.state().metadata.player_actor_id;}
    Outcome<std::string> display_name(const StableId& id)const override{if(id.rfind("remote",0)==0)throw std::logic_error("remote name read");names.push_back(id);return {true,"Name "+id,{}};}
    Outcome<std::string> text(const StableId&)const override{throw std::logic_error("unexpected text");}
    StableId party_id(const StableId&)const override{throw std::logic_error("unexpected party quote");}
    Integer party_extra_cost(const StableId&)const override{throw std::logic_error("unexpected cost");}
    Outcome<std::optional<ArmyTravelView>> active_army(const StableId&)const override{throw std::logic_error("unexpected army private data");}
    bool talking(const StableId&)const override{throw std::logic_error("unexpected dialogue");}
    Outcome<bool> admission_allowed(const StableId&,const StableId&)const override{throw std::logic_error("unexpected permission");}
    const terrain::Flags& terrain_flags()const override{throw std::logic_error("unexpected flags");}
    const TravelScheduleDefinition* schedule(const StableId&)const override{throw std::logic_error("unexpected future schedule");}
    // 每次只读序列都经正式查询，名字访问可独立追踪。
    Outcome<MapPresencePage> read(const MapPresenceIndex& index,const std::optional<MapPresenceCursor>& cursor={}){return query_map_presence(common,catalog,*this,*this,index,"player",cursor);}
};
// 冷编码仅供完整非变比较，不回灌或作为前端输入。
std::string bytes(const CommonStore& common){auto encoded=render_boundary_json(encode_common_state(common.state()));if(!encoded.ok)throw std::logic_error("cannot encode fixture");return encoded.value;}
// 分页只计同地对象；增加大量远方人物不改变当前页字节、姓名读取或游标。
void paging_privacy(){
    Fixture f;for(int i=0;i<70;++i)f.add("local:"+std::to_string(i),SettlementLocation{"town","gate"});
    f.add("remote-room",SettlementLocation{"town","room"});f.add("dead",SettlementLocation{"town","gate"},false);
    auto index=build_map_presence_index(f.common);const auto before=bytes(f.common);auto first=f.read(index);
    check(first.ok&&first.value.rows.size()==64&&first.value.scanned==65&&first.value.next_cursor->offset==65,"bounded first page excludes observer");
    check(f.names.size()==64&&first.value.rows.front().actor_id=="local:0"&&first.value.rows.back().actor_id=="local:63","only page names read in original order");
    const auto original=render_boundary_json(encode_map_presence(first.value)).value;
    for(int i=0;i<350;++i)f.add("remote:"+std::to_string(i),SettlementLocation{"far","far-gate"});
    index=build_map_presence_index(f.common);auto repeated=f.read(index);
    check(repeated.ok&&render_boundary_json(encode_map_presence(repeated.value)).value==original,"remote population does not affect rows or cursor");
    auto last=f.read(index,first.value.next_cursor);check(last.ok&&last.value.rows.size()==6&&!last.value.next_cursor&&last.value.rows.front().actor_id=="local:64","exact remaining local page");
    auto terminal=*first.value.next_cursor;terminal.offset=71;check(f.read(index,terminal).ok&&f.read(index,terminal).value.rows.empty(),"empty terminal page valid");terminal.offset=72;check(!f.read(index,terminal).ok,"cursor beyond local range rejected");
    auto frozen=bytes(f.common);first.value.rows.front().display_name="changed";check(f.read(index).value.rows.front().display_name=="Name local:0"&&bytes(f.common)==frozen,"returned values own bytes and queries mutate no state or RNG");
    auto cursor=decode_map_presence_cursor(*encode_map_presence(repeated.value).find("next_cursor"));check(cursor.ok&&f.read(index,cursor.value).value.rows.size()==6,"typed cursor codec round trip");
    auto malformed=*encode_map_presence(repeated.value).find("next_cursor");malformed.as<BoundaryValue::Dict>()->emplace_back("remote_actor",BoundaryValue{"remote:0"});check(!decode_map_presence_cursor(malformed).ok,"cursor rejects extra fields");
    auto stale=*repeated.value.next_cursor;stale.identity.execution_epoch++;check(!f.read(index,stale).ok,"cold restore epoch invalidates cursor");
    stale=*repeated.value.next_cursor;stale.location.secondary="room";check(!f.read(index,stale).ok,"cursor cannot request another place");
    check(!query_map_presence(f.common,f.catalog,f,f,index,"local:0").ok,"nonplayer observer rejected");
    f.common.state().identity.revision++;check(!f.read(index).ok,"old index rejected after world revision");
    check(!before.empty(),"nonmutation baseline encoded");
}
// 道路与原野沿原位置合同判断，不将附近或坐标相同的另一来源冒充现场。
void moving_positions(){
    Fixture f;f.common.state().actors.find("player")->location=OnRouteLocation{"road","town",50};
    f.catalog.roads.insert("road",{"road","town","far",2,0,0,{}, {},{{40,40}}});
    f.add("local-road",OnRouteLocation{"road","town",50});f.add("remote-progress",OnRouteLocation{"road","town",51});
    auto page=f.read(build_map_presence_index(f.common));check(page.ok&&page.value.rows.size()==1&&page.value.rows[0].actor_id=="local-road","only exact road progress observable");
    check(page.ok&&page.value.position.road&&page.value.position.road->progress==50&&page.value.position.road->points.size()==3,"road geometry reuses original journey projection");
    f.catalog.world_map_id="map";terrain::TerrainMap map;map.id="map";map.width=100;map.height=100;map.cell_size=10;map.cells.resize(100);f.catalog.maps.insert("map",map);
    f.common.state().actors.find("player")->location=WildernessLocation{"map","town",{5,7}};
    f.add("local-field",WildernessLocation{"map","town",{5,7}});f.add("remote-field",WildernessLocation{"map","town",{6,7}});
    auto field=f.read(build_map_presence_index(f.common));check(field.ok&&field.value.rows.size()==1&&field.value.rows[0].actor_id=="local-field"&&field.value.position.observer_position.x==5,"field point is exact with no invented radius");
    PartyRecord party;party.id="party";party.leader_id="player";party.members={"player","follower"};f.common.state().parties.insert("party",party);f.add("follower",FollowingLocation{"party"});
    auto followed=f.read(build_map_presence_index(f.common));check(followed.ok&&followed.value.rows.size()==2&&followed.value.rows[1].actor_id=="follower","following uses CommonStore resolved position");
    auto stale_index=build_map_presence_index(f.common);f.common.state().actors.find("local-field")->location=WildernessLocation{"map","town",{6,7}};check(!f.read(stale_index).ok,"corrupt same-identity index fails instead of exposing moved actor");
}
}
// 单进程读取有限夹具，不运行另一世界时钟，也不重跑已过正式Main门。
int main(){try{paging_privacy();moving_positions();}catch(const std::exception& error){check(false,error.what());}std::cout<<"MAP_PRESENCE_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
