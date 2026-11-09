// 实际原围城timeout与原wall只读接缝复现；不构造完整World，不注入战斗胜负或绕过wall门。
#include "resident/domains/military/battle_world.hpp"
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
 Fixture f;f.t.rules.max_ticks=1;f.s.unit_nodes={{"ua","gate"},{"ud","gate"}};f.s.ai_plans.clear();f.o.units[0].engaged=true;f.o.units[1].engaged=true;
 SiegeBattleRequest request;request.source_id="siege:4:gate";request.node_id="gate";request.battlefield_id="field";request.unit_ids={"ua","ud"};request.origin_nodes={{"ua","approach"},{"ud","inner"}};f.s.pending_nodes={{"gate",request}};
 check(validate_siege(f.s,f.t,f.o).ok,"actual original pending-node input admitted");
 MilitaryState m;m.sieges={f.s};MilitaryContactFact contact;contact.id=request.source_id;contact.siege_id=f.s.id;contact.node_id="gate";contact.battlefield_id="field";contact.unit_ids=request.unit_ids;contact.side_by_army={{"a",0},{"d",1}};m.contact_facts={contact};
 MilitaryUnitRecord ua;ua.id="ua";ua.army_id="a";MilitaryUnitRecord ud;ud.id="ud";ud.army_id="d";m.military_units={ua,ud};MilitaryCatalog cat;cat.siege_topologies={f.t};MilitaryBattleRecord battle;battle.source_id=contact.id;battle.battlefield_id="field";
 auto before=military_damage_sources(m,cat,battle);check(before.ok,"actual original wall source admits active node");
 auto resolved=resolve_siege(f.s,f.t,f.o,"commander",{},5);check(resolved.ok,"actual original half-day resolves");if(!resolved.ok){std::cerr<<resolved.error.original.code<<'\n';return 1;}
 check(resolved.value.siege.phase=="result"&&resolved.value.siege.result&&resolved.value.siege.result->kind=="timeout"&&resolved.value.siege.result->tick==5,"timeout can happen with original ongoing node battle");check(resolved.value.siege.pending_nodes.empty(),"actual original timeout clears node occupancy before later event consumption");
 const SiegeEvent* release=nullptr;for(const auto& event:resolved.value.events)if(event.kind=="siege_release_request")release=&event;
 check(release&&release->reason==std::optional<StableId>{"timeout"}&&release->battle_source_ids==std::optional<std::vector<StableId>>{std::vector<StableId>{contact.id}},"actual release points to exact old contact");
 m.sieges={resolved.value.siege};auto after=military_damage_sources(m,cat,battle);check(!after.ok&&after.error.original.code=="SiegeFortificationNotActive","real wall source rejects before later release processing");
 check(resolved.value.asset_effects.empty()&&f.o.public_goods[0].second[0].second==5,"diagnostic does not spend or invent materials");
 std::cout<<"SIEGE_TIMEOUT_DIAG="<<checks<<" FAILURES="<<failures<<" PRODUCT_ERROR="<<after.error.original.code<<" SCOPE=ORIGINAL_RULES_AND_WALL_READ\n";return failures?1:0;
}
