// 只核有限B显示投影；测试服务若被误用于费用/规划/权限则立即失败，不冒充完整世界实链。
#include "resident/views/journey_motion.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
// 边界断言打印具体来源，不以统计检查数替代Main验收。
void check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
// 只为有限读取提供同一组typed来源；无事务、资源写入或第二份世界算法。
struct Fixture final:CommonRead,TravelServices,PresentationRead {
    CommonState common;TravelState travel;TravelCatalog catalog;terrain::Flags flags;
    Location resolved;StableId party;mutable std::vector<StableId> names_read;mutable int resolutions=0;
    std::unordered_map<StableId,std::string> names;
    // fixture固定已准备目录和当前玩家，不调用规则造假推进。
    Fixture(){common.metadata.player_actor_id="actor.player";common.next_instance_id=17;ActorRecord a;a.id="actor.player";a.location=WildernessLocation{"map.test","town",{0,2}};a.alive=true;a.available=true;common.actors.insert(a.id,a);resolved=a.location;catalog.world_map_id="map.test";terrain::TerrainMap map;map.id=catalog.world_map_id;map.width=200;map.height=200;map.cell_size=10;map.cells.resize(400);catalog.maps.insert(catalog.world_map_id,std::move(map));catalog.settlements.insert("town",{"town","gate",{23,31}});}
    // 这些基础读取只借fixture当前记录，不生成额外人物。
    const ActorCoreView* actor(const StableId& id)const override{return common.actors.find(id);}
    const ActionCoreView* action(const StableId& id)const override{return common.actions.find(id);}
    const std::vector<StableId>& actor_order()const override{return common.actors.order();}
    const std::vector<StableId>& action_order()const override{return common.actions.order();}
    Integer tick()const override{return common.identity.tick;}
    Integer revision()const override{return common.identity.revision;}
    // 有限地图/同行查询不得读取钱包、行囊、成员动作权限或资源版本。
    const WalletView* wallet(const StableId&)const override{throw std::logic_error("unexpected wallet read");}
    const InventoryView* inventory(const StableId&)const override{throw std::logic_error("unexpected inventory read");}
    const std::vector<StableId>* members(const StableId&)const override{throw std::logic_error("unexpected activity members read");}
    Outcome<Integer> inventory_weight(const StableId&)const override{throw std::logic_error("unexpected load read");}
    ReadVersion read_version(const ResourceKey&)const override{throw std::logic_error("unexpected resource version read");}
    Outcome<Integer> spendable(const StableId&)const override{throw std::logic_error("unexpected funds read");}
    // 位置/party/旗标从指定当前来源返回；这些服务不重新寻路。
    Outcome<Location> resolved_location(const StableId&)const override{++resolutions;return {true,resolved,{}};}
    StableId party_id(const StableId&)const override{return party;}
    const terrain::Flags& terrain_flags()const override{return flags;}
    const StableId& player_actor_id()const override{return common.metadata.player_actor_id;}
    // C的互动与军粮由中央按原早退顺序调用，B叶不能预读它们。
    Integer party_extra_cost(const StableId&)const override{throw std::logic_error("unexpected quote");}
    Outcome<std::optional<ArmyTravelView>> active_army(const StableId&)const override{throw std::logic_error("unexpected army read");}
    bool talking(const StableId&)const override{throw std::logic_error("unexpected dialogue read");}
    Outcome<bool> admission_allowed(const StableId&,const StableId&)const override{throw std::logic_error("unexpected permission read");}
    const TravelScheduleDefinition* schedule(const StableId&)const override{throw std::logic_error("unexpected future schedule");}
    // 只允许实际分页选中的同行姓名，合法空名原样保留。
    Outcome<std::string> display_name(const StableId& id)const override{names_read.push_back(id);auto it=names.find(id);if(it==names.end())return {false,{},{{"MissingTestName",id},ErrorKind::internal,{},{},{},{}}};return {true,it->second,{}};}
    Outcome<std::string> text(const StableId&)const override{throw std::logic_error("unexpected dialogue text");}
    // 当前点与余程允许重复坐标，用于证明显示不偷偷裁剪或平滑。
    void travelling(){common.actors.find("actor.player")->action_id="action:view";ActionCoreView a;a.id="action:view";a.kind="travel";a.actor_id="actor.player";a.remaining_ticks=7;common.actions.insert(a.id,a);TerrainTravelParams p;p.map_id="map.test";p.paused=true;p.destination=PointI{129,2};for(Integer i=0;i<129;++i)p.points.push_back({i,2});travel.actions.insert(a.id,std::move(p));}
};
// raw路由、resolved地图点和原野驻足分别核对，不让一个投影改变另一个意义。
void location_and_route(){Fixture f;
    auto raw=query_journey_location(f,"actor.player");check(raw.ok&&raw.value.kind=="Wilderness"&&!raw.value.place_id&&!raw.value.settlement_id&&f.resolutions==0,"raw location does not resolve");
    auto map=query_journey_map_position(f,f.catalog,f,"actor.player");check(map.ok&&map.value.world_map_id=="map.test"&&map.value.observer_position.x==0&&map.value.observer_position.y==2,"map uses actual resolved point");
    auto idle=query_journey_travel(f,f.travel,"actor.player");check(idle.ok&&!idle.value.active&&!idle.value.terrain,"wilderness without action remains idle");
    check(query_journey_route(f,f.travel,"actor.player").value.points.empty(),"idle overlay is empty");
    f.travelling();auto travel=query_journey_travel(f,f.travel,"actor.player");check(travel.ok&&travel.value.active&&travel.value.terrain->paused&&travel.value.terrain->remaining_ticks==7&&std::get<PointI>(travel.value.terrain->destination).x==129,"travel summary uses saved parameters");
    auto first=query_journey_route(f,f.travel,"actor.player",0),second=query_journey_route(f,f.travel,"actor.player",64),last=query_journey_route(f,f.travel,"actor.player",128),end=query_journey_route(f,f.travel,"actor.player",130);
    check(first.ok&&first.value.active&&first.value.total_rows==130&&first.value.points.size()==64&&first.value.next_cursor==64,"route first page bounded64");
    check(first.value.points[0].x==0&&first.value.points[1].x==0&&first.value.points.back().x==62,"current point and repeated first saved point preserved");
    check(second.ok&&second.value.points.size()==64&&second.value.points.front().x==63&&second.value.next_cursor==128,"route middle page has no duplicate page boundary");
    check(last.ok&&last.value.points.size()==2&&last.value.points.front().x==127&&last.value.points.back().x==128&&!last.value.next_cursor,"route final saved suffix preserved");
    check(end.ok&&end.value.active&&end.value.points.empty()&&!end.value.next_cursor,"route empty terminal page valid");
    check(!query_journey_route(f,f.travel,"actor.player",-1).ok&&!query_journey_route(f,f.travel,"actor.player",131).ok,"invalid route cursors rejected");
    first.value.points.front().x=999;check(std::get<WildernessLocation>(f.actor("actor.player")->location).point.x==0&&f.travel.actions.find("action:view")->points.front().x==0,"route return owns values");
    f.travel.actions.find("action:view")->destination=StableId(4097,'x');check(query_journey_travel(f,f.travel,"actor.player").error.original.code=="JourneyViewTextLimit","scalar destination ID obeys finite text limit");
    f.common.actors.find("actor.player")->location=FollowingLocation{"party"};f.resolved=SettlementLocation{"town","gate"};raw=query_journey_location(f,"actor.player");map=query_journey_map_position(f,f.catalog,f,"actor.player");
    check(raw.ok&&raw.value.kind=="Following"&&!raw.value.place_id&&map.ok&&map.value.observer_position.x==23,"raw follower stays raw while map resolves leader");
    check(!query_journey_route(f,f.travel,"actor.player").ok,"shared terrain action on follower does not invent raw overlay");
    f.common.actors.find("actor.player")->location=OnRouteLocation{"road","town",50};f.resolved=f.actor("actor.player")->location;
    check(query_journey_map_position(f,f.catalog,f,"actor.player").error.original.code=="InvalidMapViewSource"&&query_journey_route(f,f.travel,"actor.player").error.original.code=="InvalidMapViewSource","unknown road never becomes zero terrain point");
    check(!query_journey_location(f,"missing").ok&&!query_journey_travel(f,f.travel,"missing").ok,"unknown selected observer rejected");
    f.common.actors.find("actor.player")->location=SettlementLocation{"town",StableId(4097,'x')};check(query_journey_location(f,"actor.player").error.original.code=="JourneyViewTextLimit","raw scalar place ID cannot bypass finite text limit");
    check(f.common.next_instance_id==17&&f.tick()==0&&f.revision()==0,"views allocate no rule IDs or time");
}
// 地图旗标按cells首现序去重，缺值为false，非crossing旗标不泄露到地图DTO。
void crossing_pages(){Fixture f;auto* map=f.catalog.maps.find("map.test");map->cells[0].crossing="bridge";map->cells[0].blocked_flag="gate.z";map->cells[1].crossing="ford";map->cells[1].blocked_flag="gate.a";map->cells[2].blocked_flag="private";map->cells[3]=map->cells[0];f.flags["gate.z"]=true;f.flags["private"]=true;
    for(std::size_t i=0;i<68;++i){map->cells[i+4].crossing="pass";map->cells[i+4].blocked_flag="gate."+std::to_string(i);f.flags[*map->cells[i+4].blocked_flag]=(i%2)==0;}
    auto first=query_journey_crossings(f.catalog,f),last=query_journey_crossings(f.catalog,f,64),end=query_journey_crossings(f.catalog,f,70);
    check(first.ok&&first.value.total_rows==70&&first.value.rows.size()==64&&first.value.next_cursor==64,"crossings bounded and duplicate key counted once");
    check(first.value.rows[0].flag_id=="gate.z"&&first.value.rows[0].closed&&first.value.rows[1].flag_id=="gate.a"&&!first.value.rows[1].closed,"crossings preserve first-seen order and absent false");
    check(first.value.rows[2].flag_id=="gate.0"&&first.value.rows[12].flag_id=="gate.10","crossings not lexicographically sorted");
    check(last.ok&&last.value.rows.size()==6&&last.value.rows[0].flag_id=="gate.62"&&!last.value.next_cursor,"crossings last page exact");
    check(end.ok&&end.value.rows.empty()&&!query_journey_crossings(f.catalog,f,71).ok,"crossing terminal and invalid cursors");
    first.value.rows[0].closed=false;check(f.flags["gate.z"],"crossing values deeply owned");
}
// 真实party字段原序分页；只对当前页调用表现来源，不预读leader/页外姓名。
void companion_pages(){Fixture f;auto none=query_journey_companions(f.common,f,f,"actor.player");check(none.ok&&!none.value.party_present&&f.names_read.empty(),"no party reads no names");
    PartyRecord party;party.id="party";party.leader_id="actor.player";party.members.push_back(party.leader_id);
    for(int i=0;i<68;++i){StableId id="member:"+std::to_string(i);ActorRecord person;person.id=id;f.common.actors.insert(id,person);party.members.push_back(id);party.terms.insert(id,{"role",i+1});f.names[id]=i==0?"":"Name "+std::to_string(i);}
    f.common.parties.insert("party",std::move(party));f.party="party";
    auto first=query_journey_companions(f.common,f,f,"actor.player");check(first.ok&&first.value.party_present&&first.value.total_rows==68&&first.value.rows.size()==64&&first.value.next_cursor==64,"companions bounded64 excluding leader");
    check(first.value.rows[0].display_name.empty()&&first.value.rows[2].actor_id=="member:2"&&first.value.rows[10].actor_id=="member:10","legal empty names and original party order preserved");
    check(f.names_read.size()==64&&f.names_read.front()=="member:0"&&f.names_read.back()=="member:63","only current page names queried");
    auto last=query_journey_companions(f.common,f,f,"actor.player",64);check(last.ok&&last.value.rows.size()==4&&last.value.rows[0].cost_per_tick==65&&!last.value.next_cursor,"companion final page uses actual per-member terms");
    check(query_journey_companions(f.common,f,f,"actor.player",68).ok&&!query_journey_companions(f.common,f,f,"actor.player",69).ok,"companion cursor bounds");
    first.value.rows[1].display_name="changed";check(f.names["member:1"]=="Name 1","companion result owns display name");
    f.common.parties.find("party")->terms.erase("member:67");check(query_journey_companions(f.common,f,f,"actor.player").ok&&!query_journey_companions(f.common,f,f,"actor.player",64).ok,"missing touched terms fail without pre-reading later page");
    f.names["member:0"]=std::string(4097,'x');auto too_long=query_journey_companions(f.common,f,f,"actor.player");check(!too_long.ok&&too_long.error.original.code=="JourneyViewTextLimit","oversized name rejected without truncation");
}
}
// 单进程只核三种只读投影，不调用原半日链、Godot或地图规划器。
int main(){try{location_and_route();crossing_pages();companion_pages();}catch(const std::exception& e){check(false,e.what());}std::cout<<"TRAVEL_MOTION_VIEW_CHECKS="<<checks<<" FAILURES="<<failures<<'\n';return failures?1:0;}
