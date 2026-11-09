// 只运行原围城报价/半日纯规则；真实工程XP、军令来源和World事务由后续闭包验收。
#include "resident/domains/military/military_siege_engineering_rules.hpp"
#include <iostream>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
void check(bool value,const char* name){++checks;if(!value){++failures;std::cerr<<name<<'\n';}}
struct Fixture {
 SiegeTopology t;SiegeObservation o;SiegeRecord s;MilitarySiegeEngineeringPreparation p;
 Fixture(){
  t.id="topology";t.attacker_entry_id="approach";t.defender_entry_id="inner";t.inner_id="inner";
  t.nodes={{"approach","approach","Approach","field",0},{"gate","gate","Gate","field",200},{"inner","inner","Inner","field",0},{"supply","supply","Supply","field",0}};
  t.edges={{"approach","gate",""},{"gate","inner","gate"},{"inner","supply",""}};t.supply_node_ids={"supply"};
  t.rules.hold_ticks=3;t.rules.blockade_units=1;t.rules.starvation_ticks=4;t.rules.max_ticks=100;t.rules.gate_damage=7;t.rules.engine_bonus=9;t.rules.repair_amount=5;t.rules.engine_ticks=2;t.rules.surrender_morale=30;t.rules.wall_defense_bp=20000;t.rules.engine_item_id="ram";t.rules.engine_materials={{"wood",2,{}}};t.rules.repair_materials={{"wood",1,{}}};
  o.tick=4;o.controller_actor_id="commander";o.target_settlement_id="town";o.defender_food=50;
  o.armies={{"a","north","commander","box-a",SettlementLocation{"town","gate"}},{"d","south","defender","box-d",SettlementLocation{"town","gate"}}};
  o.units={{"ua","a",101,80,true,false},{"ud","d",80,80,true,false}};o.public_goods={{"a",{{"wood",5},{"ram",1}}},{"d",{{"wood",5}}}};o.control_sets={{"commander",{"ua"}}};
  auto created=create_siege({"siege","campaign","fort",{"a"},{"d"}},t,o);check(created.ok,"original create");if(created.ok)s=created.value;
  p.id="military_siege_engineering:30";p.siege_id=s.id;p.topology_id=t.id;p.from_tick=4;p.to_tick=5;p.round=1;
  for(const auto& row:o.units){MilitarySiegeEngineeringUnit u;u.unit_id=row.id;u.army_id=row.army_id;u.commander_actor_id=row.id=="ua"?"commander":"defender";u.authored_actor_id=u.commander_actor_id;u.command_source={MilitaryUnitCommandSourceKind::authored,row.id};u.fit_count=row.fit_count;u.experience=300;u.level=2;p.units.push_back(u);}
 }
};
Integer hp(const SiegeRecord& s){for(const auto& row:s.node_hp)if(row.first=="gate")return row.second;return -1;}
}
int main(){
 Fixture f;SiegeOrder attack;attack.action="assault";attack.node_id="gate";
 auto old=preview_siege_order(f.s,f.t,f.o,"commander","ua",attack);check(old.ok&&old.value.damage==9,"old unselected author formula");
 auto original=f.o;check(bind_military_siege_engineering_observation(f.s,f.p,f.o).ok,"bind exact preparation");
 auto quote=preview_siege_order(f.s,f.t,f.o,"commander","ua",attack);check(quote.ok&&quote.value.damage==32,"actual quote gains engineering");
 auto next=resolve_siege(f.s,f.t,f.o,"commander",{{"ua",attack}},5);check(next.ok&&hp(next.value.siege)==168,"real original resolve applies same damage");check(next.ok&&next.value.asset_effects.empty(),"plain assault creates no material");check(hp(f.s)==200&&f.o.public_goods==original.public_goods,"pure rules preserve inputs");
 attack.engine_use=SiegeEngineUse{"box-a","ram",1,{}};quote=preview_siege_order(f.s,f.t,f.o,"commander","ua",attack);check(quote.ok&&quote.value.damage==52,"real ram adds original20");
 next=resolve_siege(f.s,f.t,f.o,"commander",{{"ua",attack}},5);check(next.ok&&hp(next.value.siege)==148&&next.value.asset_effects.empty(),"original nonconsumable ram stays in source");
 auto no_ram=f.o;no_ram.public_goods[0].second[1].second=0;check(!preview_siege_order(f.s,f.t,no_ram,"commander","ua",attack).ok,"qualification cannot create missing ram");
 auto foreign=attack;foreign.engine_use->inventory_id="box-d";check(!preview_siege_order(f.s,f.t,f.o,"commander","ua",foreign).ok,"foreign engine container rejected");
 auto bad=f.p;bad.units[0].fit_count=102;auto output=original;check(!bind_military_siege_engineering_observation(f.s,bad,output).ok&&!output.engineering_preparation,"mismatched soldiers fail before observation write");bad=f.p;bad.units.pop_back();check(!bind_military_siege_engineering_observation(f.s,bad,output).ok,"missing participant cannot bind");bad=f.p;bad.round=2;check(!bind_military_siege_engineering_observation(f.s,bad,output).ok,"stale round rejected");
 Fixture wall;wall.t.nodes.push_back({"wall","wall","Wall","field",60});wall.t.edges.push_back({"wall","gate",""});wall.s.node_hp.push_back({"wall",60});check(bind_military_siege_engineering_observation(wall.s,wall.p,wall.o).ok,"wall case binds");attack.engine_use.reset();quote=preview_siege_order(wall.s,wall.t,wall.o,"commander","ua",attack);check(quote.ok&&quote.value.damage==16,"wall protection follows new base");next=resolve_siege(wall.s,wall.t,wall.o,"commander",{{"ua",attack}},5);check(next.ok&&hp(next.value.siege)==184,"wall protection matches resolve");
 auto ai=make_siege_ai(f.s,f.t,f.o);check(ai.ok,"same engineering observation supports original AI");
 std::cout<<"SIEGE_ENGINEERING_ROUND="<<checks<<" FAILURES="<<failures<<" SCOPE=PURE_ORIGINAL_SIEGE\n";return failures?1:0;
}
