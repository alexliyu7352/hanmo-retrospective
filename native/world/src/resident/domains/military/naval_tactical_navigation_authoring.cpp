// 重名、未知工法和非法水流坐标均在准入时拒绝，不用运行默认值补作者缺项。
#include "naval_tactical_navigation_authoring.hpp"
#include "resident/model/common_model.hpp"
#include "naval_json_guard.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
Outcome<NavalTacticalNavigationPolicy> decode_naval_tactical_navigation(const BoundaryValue& value){
 const StableId path="military_naval/combat_policy/tactical_navigation";auto unique=validate_naval_json_unique_keys(value,path);if(!unique.ok)return {false,{},unique.error};
 try{boundary::fields(value,{"profile","quarter_turn_cost","upstream_cost","currents"},{},path);NavalTacticalNavigationPolicy policy;policy.profile=boundary::get_string(value,"profile",path);policy.quarter_turn_cost=boundary::get_int(value,"quarter_turn_cost",path);policy.upstream_cost=boundary::get_int(value,"upstream_cost",path);
  for(const auto& row:boundary::array(boundary::field(value,"currents",path),path+"/currents")){boundary::fields(row,{"q","r","heading"},{},path+"/currents");policy.currents.push_back({{boundary::get_int(row,"q",path),boundary::get_int(row,"r",path)},boundary::get_int(row,"heading",path)});}
  auto checked=validate_naval_tactical_navigation(policy);if(!checked.ok)return {false,{},checked.error};return {true,std::move(policy),{}};
 }catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
}
