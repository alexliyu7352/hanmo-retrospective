// 具名编队占用与实际出航能力分开；水上停驻的坏档门继续拒绝无船、伪造来源与错图。
#pragma push_macro("main")
#undef main
#define main naval_world_source_cases
#include "naval_world_test.cpp"
#undef main
#pragma pop_macro("main")
namespace {
// 组件适配只返回实际军事纯读结果，不提供真假可切换的通用放行口。
struct LocationServices:TravelServices {
 WorldFixture& f;terrain::Flags flags;StableId player="captain0";
 explicit LocationServices(WorldFixture& value):f(value){}
 Outcome<Location> resolved_location(const StableId& id)const override{return f.common.resolved_location(id);}
 StableId party_id(const StableId&)const override{return {};}
 Integer party_extra_cost(const StableId&)const override{return 0;}
 Outcome<std::optional<ArmyTravelView>> active_army(const StableId&)const override{return {true,std::nullopt,{}};}
 bool talking(const StableId&)const override{return false;}
 Outcome<bool> admission_allowed(const StableId&,const StableId&)const override{return {true,true,{}};}
 const terrain::Flags& terrain_flags()const override{return flags;}
 const TravelScheduleDefinition* schedule(const StableId&)const override{return nullptr;}
 const StableId& player_actor_id()const override{return player;}
 Outcome<std::optional<VesselFleetLocationView>> vessel_location(const StableId& actor)const override{auto s=query_naval_location_source(f.common,f.military,f.state,f.catalog,actor);if(!s.ok)return {false,{},s.error};if(!s.value)return {true,std::nullopt,{}};const auto& p=*s.value;return {true,VesselFleetLocationView{p.fleet_id,p.army_id,p.commander_actor_id,p.formation_source_id,p.world_map_id},{}};}
};
}
int main(){try{WorldFixture f;f.state=take(commit_naval_formation(f.state,f.catalog,take(f.observe()),f.request,"fleet:4","naval_action:5"));take(f.common.append_fact({"naval_action:5","naval_form","military_naval"}));auto* map=f.travel.maps.find("river");map->width=60;map->height=60;map->cell_size=20;map->cells.resize(9);for(auto& cell:map->cells)cell.terrain="water";f.travel.settlements.insert("town",{"town","north_dock",{0,0}});f.common.state().actors.find("captain0")->location=WildernessLocation{"river","town",{30,30}};TravelState state;LocationServices services(f);auto validate=[&](){return validate_travel_domain(f.common,state,f.travel,services);};auto source=take(query_naval_location_source(f.common,f.military,f.state,f.catalog,"captain0"));check(source&&source->formation_source_id=="naval_action:5"&&source->world_map_id=="river","actual formation source is named");check(validate().ok,"intact fleet on real water accepted without active action");f.state.vessels[0].integrity=0;check(validate().ok,"disabled ship remains legally afloat");auto sail=preview_naval_travel(f.common,f.military,f.mc,f.access,f.state,f.catalog,"captain0");check(!sail.ok&&sail.error.original.code=="NavalVesselDisabled","location proof never grants sailing");f.military.offices[0].enabled=false;check(validate().ok,"revoked office does not remove physical ship occupation");
 f.state.fleets[0].source_id="forged";check(!validate().ok,"missing formation fact rejected");f.state.fleets[0].source_id="naval_action:5";f.common.state().facts.find("naval_action:5")->kind="military_action";check(!validate().ok,"wrong fact kind rejected");f.common.state().facts.find("naval_action:5")->kind="naval_form";f.state.vessels[0].fleet_id="foreign";check(!validate().ok,"foreign ship binding rejected");f.state.vessels[0].fleet_id="fleet:4";f.catalog.world_map_id="other";check(!validate().ok,"wrong water map rejected");f.catalog.world_map_id="river";f.state.fleets[0].created_tick=5;check(!validate().ok,"future formation rejected");f.state.fleets[0].created_tick=4;f.state.fleets.push_back(f.state.fleets.front());check(!validate().ok,"ambiguous commander occupancy rejected");f.state.fleets.pop_back();f.state.fleets[0].status="released";check(!validate().ok,"released ship cannot authorize water location");f.state.fleets[0].status="active";f.common.state().actors.find("captain0")->location=WildernessLocation{"river","town",{90,30}};check(!validate().ok,"real ship cannot authorize outside map");
 std::cout<<"NAVAL_LOCATION_SOURCE="<<checks<<" FAILURES=0 SCOPE=COMPONENT_GEOMETRY\n";return 0;}catch(const std::exception& e){std::cerr<<"NAVAL_LOCATION_SOURCE_FAIL "<<e.what()<<'\n';return 1;}}
