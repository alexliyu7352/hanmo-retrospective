// 新海军作者严格口只接受已声明内河资产，不把未实现结果降级吞掉。
#include "resident/domains/military/naval_authoring.hpp"
#include "resident/storage/common_codec.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 断言不依赖构建assert开关，具体失败直接留下原日志。
void check(bool v,const char* m){++checks;if(!v)throw std::runtime_error(m);}
// 用最小正式字段形状测试，数值仅是该作者的明确开发参数。
BoundaryValue authored(){return parse_boundary_json(R"({"profile":"afloat_inland_v1","world_map_id":"river","hulls":[{"id":"river_hull","name_key":"name.river_hull","max_integrity":80,"minimum_crew":2,"person_capacity":10,"cargo_capacity":300,"minimum_naval_level":1,"movement":3,"travel_budget":3000,"waters":["river"]}],"berths":[{"place_id":"north_dock","water_kind":"river"}],"vessels":[{"id":"north_boat","name_key":"name.north_boat","hull_id":"river_hull","owner":{"kind":"organization","id":"north"},"cargo_inventory_id":"hold","berth_id":"north_dock","integrity":80}]})").value;}
}
// 作者seed独立于World注册；此门只证strict schema和唯一来源记录。
int main(){try{auto v=authored();auto out=decode_naval_authoring(v);check(out.ok,"finite inland author decodes");check(out.value.state.vessels.size()==1&&out.value.state.fleets.empty()&&out.value.state.vessels[0].origin_source=="author:north_boat","real authored hull gets deterministic explicit origin and no invented fleet");check(out.value.catalog.vessels[0].cargo_inventory_id=="hold","original Common hold reference retained");
 auto bad=v;bad.as<BoundaryValue::Dict>()->emplace_back("sinking",BoundaryValue{true});check(!decode_naval_authoring(bad).ok,"unimplemented sinking author result rejected");bad=v;*bad.find("profile")=BoundaryValue{"sinking_v1"};check(!decode_naval_authoring(bad).ok,"profile cannot imply sinking or rescue");bad=v;auto* ships=bad.find("vessels")->as<BoundaryValue::Array>();ships->push_back(ships->front());check(!decode_naval_authoring(bad).ok,"duplicate real vessel author rejected");bad=v;auto* hulls=bad.find("hulls")->as<BoundaryValue::Array>();*hulls->front().find("waters")=BoundaryValue{BoundaryValue::Array{BoundaryValue{"ocean"}}};check(!decode_naval_authoring(bad).ok,"sea outside first author contract rejected");bad=v;auto* boats=bad.find("vessels")->as<BoundaryValue::Array>();*boats->front().find("berth_id")=BoundaryValue{"missing"};check(!decode_naval_authoring(bad).ok,"real vessel cannot start outside an authored berth");bad=v;boats=bad.find("vessels")->as<BoundaryValue::Array>();boats->front().as<BoundaryValue::Dict>()->emplace_back("fleet_id",BoundaryValue{"fleet:1"});check(!decode_naval_authoring(bad).ok,"author cannot forge a formed army fleet");bad=v;hulls=bad.find("hulls")->as<BoundaryValue::Array>();*hulls->front().find("minimum_naval_level")=BoundaryValue{Integer{5}};check(!decode_naval_authoring(bad).ok,"author cannot invent fifth naval skill level");

 auto data=parse_boundary_json(R"({"required_capabilities":["military","military_naval_v1"]})").value;
 auto scenario=parse_boundary_json(R"({"dynamic_world":{"profile":"local_society_v1"},"skill_profile":"full_sixteen_v1","world_map_id":"river"})").value;
 check(decode_naval_profile(data,scenario).ok&&!decode_naval_profile(data,scenario).value,"old scenario remains absent");
 scenario.as<BoundaryValue::Dict>()->emplace_back("military_naval",v);check(decode_naval_profile(data,scenario).ok,"explicit full profile admitted");
 auto cold=initial_naval_assets(out.value.catalog);check(cold.vessels.size()==1&&cold.fleets.empty(),"initial assets do not create a fleet");
 auto wrong=scenario;*wrong.find("skill_profile")=BoundaryValue{"legacy"};check(!decode_naval_profile(data,wrong).ok,"skill profile cannot be implicit");
 wrong=scenario;*wrong.find("world_map_id")=BoundaryValue{"another"};check(!decode_naval_profile(data,wrong).ok,"same actual map required");
 auto no_cap=data;*no_cap.find("required_capabilities")=BoundaryValue{BoundaryValue::Array{BoundaryValue{"military"}}};check(!decode_naval_profile(no_cap,scenario).ok,"capability cannot be implied from owned boat");
 std::cout<<"NAVAL_AUTHOR_CHECKS="<<checks<<" FAILURES=0 SCOPE=STRICT_COMPONENT\n";return 0;}catch(const std::exception& e){std::cerr<<"NAVAL_AUTHOR_FAIL checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
