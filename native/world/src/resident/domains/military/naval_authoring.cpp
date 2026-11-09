// 舰船作者只给静态规格与原Owner/货舱/泊地引用；真实军团必须通过运行时军令组舰。
#include "naval_authoring.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 船型数值全部显式，不能用默认海军参数吞缺失字段。
NavalHullDefinition hull(const BoundaryValue& v){const StableId p="military_naval/hulls";boundary::fields(v,{"id","name_key","max_integrity","minimum_crew","person_capacity","cargo_capacity","minimum_naval_level","movement","travel_budget","waters"},{},p);NavalHullDefinition h;h.id=boundary::get_string(v,"id",p);h.name_key=boundary::get_string(v,"name_key",p);h.max_integrity=boundary::get_int(v,"max_integrity",p);h.minimum_crew=boundary::get_int(v,"minimum_crew",p);h.person_capacity=boundary::get_int(v,"person_capacity",p);h.cargo_capacity=boundary::get_int(v,"cargo_capacity",p);h.minimum_naval_level=boundary::get_int(v,"minimum_naval_level",p);h.movement=boundary::get_int(v,"movement",p);h.travel_budget=boundary::get_int(v,"travel_budget",p);h.waters=boundary::strings(boundary::field(v,"waters",p),p);return h;}
// 泊地只声明原place所具有的水域能力；坐标和准入仍归原Travel。
NavalBerthDefinition berth(const BoundaryValue& v){const StableId p="military_naval/berths";boundary::fields(v,{"place_id","water_kind"},{},p);return {boundary::get_string(v,"place_id",p),boundary::get_string(v,"water_kind",p)};}
// 初船不允许作者注入动态舰队/战斗/俘获事实或自称未知来源。
NavalVesselDefinition vessel(const BoundaryValue& v){const StableId p="military_naval/vessels";boundary::fields(v,{"id","name_key","hull_id","owner","cargo_inventory_id","berth_id","integrity"},{},p);NavalVesselDefinition d;d.id=boundary::get_string(v,"id",p);d.name_key=boundary::get_string(v,"name_key",p);d.hull_id=boundary::get_string(v,"hull_id",p);d.owner=boundary::decode_owner(boundary::field(v,"owner",p),p);d.cargo_inventory_id=boundary::get_string(v,"cargo_inventory_id",p);d.berth_id=boundary::get_string(v,"berth_id",p);d.integrity=boundary::get_int(v,"integrity",p);return d;}
}
// 先完成局部typed候选，再共用真实资产形状门；引用是否存在由组合World继续核。
Outcome<NavalAuthoringSeed> decode_naval_authoring(const BoundaryValue& v){try{const StableId p="military_naval";boundary::fields(v,{"profile","world_map_id","hulls","berths","vessels"},{},p);NavalAuthoringSeed out;out.catalog.profile=boundary::get_string(v,"profile",p);out.catalog.world_map_id=boundary::get_string(v,"world_map_id",p);if(out.catalog.world_map_id.empty())boundary::fail("NavalWaterMapRequired",p);for(const auto& row:boundary::array(boundary::field(v,"hulls",p),p))out.catalog.hulls.push_back(hull(row));for(const auto& row:boundary::array(boundary::field(v,"berths",p),p))out.catalog.berths.push_back(berth(row));for(const auto& row:boundary::array(boundary::field(v,"vessels",p),p)){auto d=vessel(row);const NavalBerthDefinition* dock=nullptr;for(const auto& b:out.catalog.berths)if(b.place_id==d.berth_id)dock=&b;if(!dock)boundary::fail("NavalBerthRequired",d.id);const NavalHullDefinition* type=nullptr;for(const auto& h:out.catalog.hulls)if(h.id==d.hull_id)type=&h;if(!type||std::find(type->waters.begin(),type->waters.end(),dock->water_kind)==type->waters.end())boundary::fail("NavalHullWaterUnsupported",d.id);NavalVesselRecord r;r.id=d.id;r.hull_id=d.hull_id;r.cargo_inventory_id=d.cargo_inventory_id;r.berth_id=d.berth_id;r.owner=d.owner;r.integrity=d.integrity;r.origin_source="author:"+d.id;out.state.vessels.push_back(std::move(r));out.catalog.vessels.push_back(std::move(d));}if(out.catalog.hulls.empty()||out.catalog.berths.empty()||out.state.vessels.empty())boundary::fail("NavalAuthoringEmpty",p);auto valid=validate_naval_assets(out.state,out.catalog);if(!valid.ok)return {false,{},valid.error};return {true,std::move(out),{}};}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 只读冷配置准入由GD预期、Native准入和存档目录复用同一严格来源。
Outcome<std::optional<NavalAuthoringSeed>> decode_naval_profile(const BoundaryValue& data,const BoundaryValue& scenario){try{
 const auto* block=scenario.find("military_naval");if(!block)return {true,std::nullopt,{}};
 const auto caps=boundary::strings(boundary::field(data,"required_capabilities","data"),"required_capabilities");
 const auto* dynamic=scenario.find("dynamic_world");const auto* skills=scenario.find("skill_profile");
 if(std::find(caps.begin(),caps.end(),"military")==caps.end()||std::find(caps.begin(),caps.end(),"military_naval_v1")==caps.end()||!dynamic||boundary::get_string(*dynamic,"profile","dynamic_world")!="local_society_v1"||!skills||boundary::string(*skills,"skill_profile")!="full_sixteen_v1")return {false,{},common_failure("NavalProfileRequired").error};
 auto decoded=decode_naval_authoring(*block);if(!decoded.ok)return {false,{},decoded.error};
 if(decoded.value.catalog.world_map_id!=boundary::get_string(scenario,"world_map_id","scenario"))return {false,{},common_failure("NavalWaterMapRequired").error};
 return {true,std::move(decoded.value),{}};
}catch(const boundary::DecodeFailure& e){return {false,{},e.error};}}
// 同静态定义仅造原实船；后继编组必须走具名命令与共同ID分配。
NavalAssetState initial_naval_assets(const NavalAssetCatalog& catalog){NavalAssetState state;for(const auto& d:catalog.vessels){NavalVesselRecord r;r.id=d.id;r.hull_id=d.hull_id;r.cargo_inventory_id=d.cargo_inventory_id;r.berth_id=d.berth_id;r.owner=d.owner;r.integrity=d.integrity;r.origin_source="author:"+d.id;state.vessels.push_back(std::move(r));}return state;}

}
