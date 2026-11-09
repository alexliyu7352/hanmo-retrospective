// 使用原参与快照与Common头顺序核占用前界，不构造World或从当前成员推旧军籍。
#include "resident/domains/military/military_siege_occupation.hpp"
#include <iostream>
using namespace hanmo::resident;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<'\n';}}
struct Fixture {
 CommonState common;MilitarySiegeLifecycleState history;SiegeRecord siege;
 Fixture(){common.identity.tick=6;common.next_instance_id=100;siege.id="siege";siege.topology_id="walls";siege.started_tick=5;siege.last_tick=5;siege.round=1;siege.armies={{"army.a",0},{"army.d",1}};siege.unit_nodes={{"unit.a","gate"},{"unit.d","inner"}};}
 void boundary(const StableId& id){common.facts.insert(id,{id,"personal_recovery_credit","personal_injury_recovery"});}
 void event(Integer number,MilitarySiegeLifecycleKind kind,Integer tick,const std::optional<SiegeStrategicCommand>& command={}){
  MilitarySiegeLifecycleFact f;f.id="military_siege_lifecycle:"+std::to_string(number);f.siege_id=siege.id;f.topology_id=siege.topology_id;f.kind=kind;f.tick=tick;f.after=capture_military_siege_participation(siege);f.strategic_command=command;
  if(!history.facts.empty()){f.previous_fact_id=history.facts.back().id;f.before=history.facts.back().after;}
  history.facts.push_back(f);common.facts.insert(f.id,{f.id,military_siege_lifecycle_kind_name(kind),"military"});
 }
};
bool armies(const Outcome<std::optional<MilitarySiegeOccupationProjection>>& q,std::initializer_list<StableId> ids){if(!q.ok||!q.value||q.value->army_source_ids.size()!=ids.size())return false;std::size_t i=0;for(const auto& id:ids)if(q.value->army_source_ids[i++].first!=id)return false;return true;}
Fixture completed(){Fixture x;x.boundary("recovery:1");x.event(2,MilitarySiegeLifecycleKind::created,5);x.boundary("recovery:3");x.siege.armies.emplace_back("army.b",0);x.siege.unit_nodes.emplace_back("unit.b","gate");x.event(4,MilitarySiegeLifecycleKind::joined,5);x.boundary("recovery:5");x.siege.last_tick=6;x.siege.round=2;x.event(6,MilitarySiegeLifecycleKind::advanced,6);x.boundary("recovery:7");x.siege.withdrawn_army_ids={"army.a"};x.siege.unit_nodes={{"unit.d","inner"},{"unit.b","gate"}};x.event(8,MilitarySiegeLifecycleKind::strategic,6,SiegeStrategicCommand{"withdraw","army.a",{},{},{}});x.boundary("recovery:9");x.siege.withdrawn_army_ids.push_back("army.b");x.siege.unit_nodes={{"unit.d","inner"}};x.siege.phase="result";x.event(10,MilitarySiegeLifecycleKind::strategic,6,SiegeStrategicCommand{"withdraw","army.b",{},{},{}});x.boundary("recovery:11");x.siege.phase="closed";x.event(12,MilitarySiegeLifecycleKind::closed,6);x.boundary("recovery:13");return x;}
}
int main(){
 auto x=completed();auto read=[&](Integer tick,const StableId& before){return read_military_siege_occupation(x.common,x.history,x.siege,tick,before);};
 check(armies(read(5,"recovery:1"),{}),"same tick creation not yet happened");
 auto before_join=read(5,"recovery:3");check(armies(before_join,{"army.a","army.d"}),"before real join");check(before_join.value&&before_join.value->army_source_ids.front().second=="military_siege_lifecycle:2","original creation source");
 auto joined=read(5,"recovery:5");check(armies(joined,{"army.a","army.d","army.b"}),"after real join");check(joined.value&&joined.value->army_source_ids.back().second=="military_siege_lifecycle:4","original join source");
 check(armies(read(6,"recovery:7"),{"army.a","army.d","army.b"}),"same tick before withdrawal");
 check(armies(read(6,"recovery:9"),{"army.d","army.b"}),"active withdrawal releases only its army");
 check(armies(read(6,"recovery:11"),{"army.d","army.b"}),"last attacker awaits actual result acknowledgement");
 check(armies(read(6,"recovery:13"),{}),"closed historical acknowledgement releases all");
 check(armies(read_military_siege_occupation(x.common,x.history,x.siege,6),{}),"closed stable now");
 auto old=read_military_siege_occupation(x.common,{},x.siege,6);check(old.ok&&!old.value,"old untracked history remains explicitly uncovered");
 auto changed=x;changed.history.facts[1].previous_fact_id="military_siege_lifecycle:99";check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,6).ok,"broken predecessor rejected");
 changed=x;changed.common.facts.find("military_siege_lifecycle:4")->kind="military_siege_closed";check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,6).ok,"wrong exact kind rejected");
 changed=x;changed.common.facts.find("military_siege_lifecycle:4")->domain="politics";check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,6).ok,"wrong domain rejected");
 changed=x;changed.history.facts.front().id="military_siege_lifecycle:02";check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,6).ok,"noncanonical source rejected");
 changed=x;changed.history.facts.erase(changed.history.facts.begin());check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,6).ok,"missing creation rejected");
 changed=x;changed.siege.phase="result";check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,6).ok,"today cannot contradict full chain");
 check(!read(5,"recovery:13").ok,"boundary cannot place next tick ahead of old credit");check(!read(6,"missing").ok,"unknown actual boundary rejected");check(!read_military_siege_occupation(x.common,x.history,x.siege,5).ok,"empty boundary only stable current");
 changed=x;changed.common.identity.tick=5;check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,5,"recovery:3").ok,"stable reader rejects uncommitted next tick");
 check(armies(read_military_siege_occupation(changed.common,changed.history,changed.siege,5,"recovery:3",DynamicStepInput{5,6}),{"army.a","army.d"}),"same strict phase replays actual old credit");
 check(armies(read_military_siege_occupation(changed.common,changed.history,changed.siege,6,"recovery:11",DynamicStepInput{5,6}),{"army.d","army.b"}),"same strict phase reads actual next credit");
 check(armies(read_military_siege_occupation(changed.common,changed.history,changed.siege,6,{},DynamicStepInput{5,6}),{}),"strict current observes actual candidate to");check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,5,{},DynamicStepInput{5,6}).ok,"strict current cannot pretend candidate is old from");
 check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,6,"recovery:11",DynamicStepInput{4,6}).ok,"non single phase rejected");check(!read_military_siege_occupation(changed.common,changed.history,changed.siege,7,"recovery:11",DynamicStepInput{5,6}).ok,"future observation rejected");
 // 交涉安全撤离没有withdraw结果等待，离营军团不能沿另一个旧结果复活占用。
 Fixture departure;departure.event(2,MilitarySiegeLifecycleKind::created,5);departure.siege.phase="result";departure.event(3,MilitarySiegeLifecycleKind::strategic,5,SiegeStrategicCommand{"negotiate",{},"safe_departure",{},{}});departure.siege.withdrawn_army_ids={"army.d"};departure.siege.unit_nodes={{"unit.a","gate"}};departure.event(4,MilitarySiegeLifecycleKind::departed,5);check(armies(read_military_siege_occupation(departure.common,departure.history,departure.siege,6),{"army.a"}),"safe departure leaves only nondeparting army occupied");
 std::cout<<"SIEGE_OCCUPATION="<<checks<<" FAILURES="<<failures<<" SCOPE=COMMON_LIFECYCLE_COMPONENT\n";return failures?1:0;
}
