// 仅纯水战占用/守恒风险门：不把域输入当已成立World接敌或俘获来源。
#include "resident/domains/military/naval_tactics.hpp"
#include <iostream>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class T>T take(Outcome<T> r){if(!r.ok)throw std::runtime_error(r.error.original.code+":"+r.error.original.path);return std::move(r.value);}
// 两个真实船编队的纯域观察，所有兵员和船主分别保存在原模型内。
struct Fixture {
 NavalAssetCatalog catalog;NavalAssetState assets;BattlefieldDefinition board;NavalTacticalFrame frame;
 Fixture(){NavalHullDefinition hull;hull.id="river";hull.max_integrity=80;hull.minimum_crew=2;hull.person_capacity=12;hull.cargo_capacity=100;hull.movement=3;hull.travel_budget=1000;hull.waters={"river"};catalog.hulls={hull};
  board.id="river_board";for(Integer q=0;q<6;++q)for(Integer r=0;r<3;++r)board.cells.push_back({q,r,"water"});board.spawns={{0,{0,0}},{1,{4,1}}};board.exits={{0,{0,0}},{1,{5,1}}};frame.id="naval_battle:10";frame.source_id="contact:9";frame.battlefield_id=board.id;frame.controller_actor_id="captain0";frame.controlled_unit_ids={"unit0","unit1"};
  for(int side=0;side<2;++side){NavalFleetRecord fleet;fleet.id="fleet:"+std::to_string(side+1);fleet.army_id="army"+std::to_string(side);fleet.commander_actor_id="captain"+std::to_string(side?2:0);fleet.source_id="naval_action:"+std::to_string(side+3);fleet.office_id="office"+std::to_string(side);fleet.appointment_source="military:5";fleet.formation_source="military:6";fleet.formation_berth_id="dock";assets.fleets.push_back(fleet);}
  for(int i=0;i<3;++i){auto n=std::to_string(i);Integer side=i==2?1:0;NavalVesselRecord vessel;vessel.id="ship"+n;vessel.hull_id=hull.id;vessel.cargo_inventory_id="hold"+n;vessel.berth_id="dock";vessel.origin_source="author:ship"+n;vessel.owner.kind=Owner::Kind::organization;vessel.owner.id="org"+std::to_string(side);vessel.integrity=80;vessel.fleet_id=assets.fleets[side].id;assets.vessels.push_back(vessel);NavalVesselAssignment a;a.vessel_id=vessel.id;a.unit_id="unit"+n;assets.fleets[side].assignments.push_back(a);BattleUnit u;u.unit_id=a.unit_id;u.army_id=assets.fleets[side].army_id;u.commander_actor_id="captain"+n;u.troop_type_id="infantry";u.side=side;u.position={i*2,side};u.heading=side?3:0;u.morale=70;u.member_ids={"soldier"+n+"a","soldier"+n+"b"};u.fit=u.member_ids;frame.units.push_back(u);frame.bindings.push_back({vessel.id,u.unit_id,vessel.fleet_id});}
 }
 NavalTacticalOrders holds()const{NavalTacticalOrders out;for(const auto& u:frame.units)out.push_back({u.unit_id,{"hold",{},u.position,u.heading}});return out;}
};
}
// 微步争格和失败都不能重复、搬走或清空原兵员，也不变更原船产权。
int main(){try{Fixture f;check(validate_naval_tactical_frame(f.assets,f.catalog,f.board,f.frame).ok,"original bound fleets validate");auto plans=f.holds();plans[0].second={"move",{},{1,0},0};plans[1].second={"move",{},{1,0},0};auto collision=take(resolve_naval_maneuvers(f.assets,f.catalog,f.board,f.frame,plans));check(collision.frame.units[0].position==f.frame.units[0].position&&collision.frame.units[1].position==f.frame.units[1].position,"simultaneous contenders both stop without array winner");
 f.assets.vessels[0].integrity=0;f.frame.units[2].position={1,0};plans=f.holds();plans[0].second={"board","unit2",{0,0},0};auto board=take(resolve_naval_maneuvers(f.assets,f.catalog,f.board,f.frame,plans));check(board.boarding_requests.size()==1,"disabled afloat hull still permits adjacent surviving crew to board");check(board.frame.units[0].fit==f.frame.units[0].fit&&f.assets.vessels[0].integrity==0&&f.assets.vessels[0].owner.id=="org0","boarding request changes no casualty, hull or owner");
 f.frame.units[0].morale=0;plans=f.holds();auto routed=take(resolve_naval_maneuvers(f.assets,f.catalog,f.board,f.frame,plans));check(routed.frame.units[0].fit==f.frame.units[0].fit&&routed.frame.units[0].deserted.empty(),"routed crew stays on original disabled ship");auto moves=take(naval_movement_options(f.assets,f.catalog,f.board,f.frame,"unit2"));bool overlaps=false;for(const auto& p:moves)overlaps=overlaps||p.position==f.frame.units[0].position;check(!overlaps,"disabled routed hull remains occupied");
 auto malformed=f.frame;malformed.units[1].fit[0]=malformed.units[0].fit[0];malformed.units[1].member_ids=malformed.units[1].fit;check(!validate_naval_tactical_frame(f.assets,f.catalog,f.board,malformed).ok,"same person cannot power two ships");plans=f.holds();plans[1].second={"move",{},{0,0},0};check(!resolve_naval_maneuvers(f.assets,f.catalog,f.board,f.frame,plans).ok,"invalid overlap rejects whole round without partial candidate");check(f.frame.units[0].fit.size()==2&&f.frame.units[1].position==HexPoint{2,0},"failed round left original people and positions intact");
 std::cout<<"NAVAL_TACTICS_CHECKS="<<checks<<" FAILURES=0 SCOPE=PURE_OCCUPANCY\n";return 0;}catch(const std::exception& e){std::cerr<<"NAVAL_TACTICS_FAIL checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
