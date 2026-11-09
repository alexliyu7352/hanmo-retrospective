#include "resident/domains/military/military_model.hpp"
// 本读口不分配ID、不写人物/钱货，也不使用今日缺Action来猜历史；冷门按原Common实序绑定。
#include "military_siege_timeout_source.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace hanmo::resident {
namespace {
using Result=Outcome<MilitarySiegeTimeoutRelease>;
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& x:rows)if(x.id==id)return &x;return nullptr;}
Result fail(const StableId& code,const StableId& id){return {false,{},common_failure(code,id).error};}
std::optional<std::size_t> position(const CommonState& c,const StableId& id){const auto& order=c.facts.order();const auto at=std::find(order.begin(),order.end(),id);return at==order.end()?std::nullopt:std::optional<std::size_t>{std::size_t(at-order.begin())};}
bool head(const CommonState& c,const StableId& id,const StableId& kind){const auto* h=c.facts.find(id);return h&&h->id==id&&h->kind==kind&&h->domain=="military"&&position(c,id).has_value();}
bool canonical(const CommonState& c,const StableId& id,const StableId& prefix){if(id.rfind(prefix+":",0)!=0)return false;const auto tail=id.substr(prefix.size()+1);Integer value=0;const auto read=std::from_chars(tail.data(),tail.data()+tail.size(),value);return read.ec==std::errc{}&&read.ptr==tail.data()+tail.size()&&value>0&&value<c.next_instance_id&&std::to_string(value)==tail;}
bool same(const MilitarySiegeParticipationSnapshot& a,const MilitarySiegeParticipationSnapshot& b){return a.phase==b.phase&&a.started_tick==b.started_tick&&a.last_tick==b.last_tick&&a.round==b.round&&a.armies==b.armies&&a.unit_nodes==b.unit_nodes&&a.withdrawn_army_ids==b.withdrawn_army_ids;}
Outcome<Integer> upper(const CommonState& c,const DynamicStepInput* p){if(!p)return {true,c.identity.tick,{}};if(p->from_tick<0||p->from_tick!=c.identity.tick||p->from_tick==std::numeric_limits<Integer>::max()||p->to_tick!=p->from_tick+1)return {false,{},common_failure("MilitarySiegeTimeoutPhaseMismatch").error};return {true,p->to_tick,{}};}
// 先核真实到期advanced和准备，既供旧节点战释放，也供同一步被取消的未安装request识别。
struct TimeoutCore {const MilitarySiegeLifecycleFact* advanced=nullptr;const MilitarySiegeEngineeringPreparation* preparation=nullptr;const SiegeEvent* release=nullptr;};
Outcome<TimeoutCore> core_fail(const StableId& code,const StableId& id){return {false,{},common_failure(code,id).error};}
Outcome<TimeoutCore> timeout_core(const CommonState& c,const MilitaryState& m,const SiegeRecord& actual,const SiegeTopology& board,Integer limit){
 const auto* siege=&actual;const auto* topology=&board;
 // 原围城到期是守方胜；节点战仅被停止，其原battle结果仍是无胜方-1。
 if(!siege->result||siege->result->kind!="timeout"||siege->result->winner_side!=1||siege->result->tick<0||siege->result->tick>limit||(siege->phase!="result"&&siege->phase!="closed")||!siege->pending_nodes.empty()||siege->last_tick!=siege->result->tick||topology->rules.max_ticks<1||siege->started_tick<0||siege->result->tick<siege->started_tick||siege->result->tick-siege->started_tick<topology->rules.max_ticks)return core_fail("MilitarySiegeTimeoutResultRequired",siege->id);
 const MilitarySiegeLifecycleFact* previous=nullptr;const MilitarySiegeLifecycleFact* ended=nullptr;std::optional<std::size_t> prior;
 for(const auto& f:m.siege_lifecycle.facts)if(f.siege_id==siege->id){const auto at=position(c,f.id);if(!at||!canonical(c,f.id,"military_siege_lifecycle")||!head(c,f.id,military_siege_lifecycle_kind_name(f.kind))||f.topology_id!=siege->topology_id||f.tick<0||f.tick>limit||(prior&&*at<=*prior))return core_fail("MilitarySiegeTimeoutLifecycleRequired",f.id);auto valid=validate_military_siege_lifecycle_shape(f,*topology);if(!valid.ok)return {false,{},valid.error};
  if(previous){if(f.previous_fact_id!=previous->id||!f.before||!same(*f.before,previous->after)||f.tick<previous->tick)return core_fail("MilitarySiegeTimeoutLifecycleRequired",f.id);}else if(f.kind!=MilitarySiegeLifecycleKind::created||f.before||!f.previous_fact_id.empty())return core_fail("MilitarySiegeTimeoutLifecycleRequired",f.id);
  if(f.kind==MilitarySiegeLifecycleKind::advanced&&f.after.phase=="result"){if(ended||f.tick!=siege->result->tick||!f.before||f.before->phase!="active"||f.before->last_tick+1!=f.tick)return core_fail("MilitarySiegeTimeoutLifecycleRequired",f.id);ended=&f;}
  previous=&f;prior=at;
 }
 if(!ended||!previous||!same(previous->after,capture_military_siege_participation(*siege)))return core_fail("MilitarySiegeTimeoutLifecycleRequired",siege->id);
 const auto* preparation=find(m.siege_engineering.preparations,ended->cause_id);const auto prepared=position(c,ended->cause_id),advanced=position(c,ended->id);
 if(!preparation||!prepared||!advanced||!canonical(c,preparation->id,"military_siege_engineering")||!head(c,preparation->id,"military_siege_engineering_prepared")||*prepared>=*advanced||preparation->siege_id!=siege->id||preparation->topology_id!=siege->topology_id||preparation->from_tick!=ended->before->last_tick||preparation->to_tick!=ended->tick||preparation->round!=ended->before->round)return core_fail("MilitarySiegeTimeoutPreparationRequired",ended->id);
 const SiegeEvent* release=nullptr;for(const auto& e:siege->last_events)if(e.kind=="siege_release_request"){if(release)return core_fail("MilitarySiegeTimeoutReleaseRequired",siege->id);release=&e;}
 if(release&&(release->reason!=std::optional<StableId>{"timeout"}||!release->battle_source_ids))return core_fail("MilitarySiegeTimeoutReleaseRequired",siege->id);
 if(release){std::set<StableId> ids;for(const auto& id:*release->battle_source_ids)if(id.empty()||!ids.insert(id).second)return core_fail("MilitarySiegeTimeoutReleaseRequired",id);}
 return {true,{ended,preparation,release},{}};
}
// 已安装contact必须有实际旧节点战和严格准备前序，不能把本次被取消的新request当旧战斗。
Result source(const CommonState& c,const MilitaryState& m,const MilitaryCatalog& cat,const MilitaryBattleRecord& b,Integer limit){
 const auto* contact=find(m.contact_facts,b.source_id);const auto* siege=contact&&contact->siege_id?find(m.sieges,*contact->siege_id):nullptr;const auto* topology=siege?find(cat.siege_topologies,siege->topology_id):nullptr;
 if(!contact||!siege||!topology||!contact->node_id||!contact->battle_id||*contact->battle_id!=b.id||contact->battlefield_id!=b.battlefield_id||contact->tick<0||contact->tick!=b.created_tick||contact->id!=siege->id+":"+std::to_string(contact->tick)+":"+*contact->node_id||!head(c,contact->id,"military_contact"))return fail("MilitarySiegeTimeoutContactRequired",b.id);
 auto core=timeout_core(c,m,*siege,*topology,limit);if(!core.ok)return {false,{},core.error};const auto* ended=core.value.advanced;const auto* preparation=core.value.preparation;const auto prepared=position(c,preparation->id),contact_at=position(c,contact->id);
 if(!contact_at||*contact_at>=*prepared||contact->tick>preparation->from_tick)return fail("MilitarySiegeTimeoutPreparationRequired",ended->id);
 const auto* release=core.value.release;if(!release||std::count(release->battle_source_ids->begin(),release->battle_source_ids->end(),contact->id)!=1)return fail("MilitarySiegeTimeoutReleaseRequired",b.id);
 for(const auto& id:contact->unit_ids)if(std::count(ended->before->unit_nodes.begin(),ended->before->unit_nodes.end(),std::make_pair(id,*contact->node_id))!=1)return fail("MilitarySiegeTimeoutNodeRequired",id);
 return {true,{siege->id,b.id,contact->id,ended->id,preparation->id,ended->tick},{}};
}
// 原resolve在本步创建而又因到期取消的request不会落Contact/Common头；只接受确切本步节点ID。
Outcome<StableId> uninstalled_request_node(const CommonState& c,const MilitaryState& m,const SiegeRecord& siege,const SiegeTopology& topology,const TimeoutCore& core,const StableId& id){
 if(c.facts.find(id)||find(m.contact_facts,id))return {false,{},common_failure("MilitarySiegeTimeoutBattleReleaseMissing",id).error};const SiegeNode* node=nullptr;for(const auto& n:topology.nodes)if(id==siege.id+":"+std::to_string(core.advanced->tick)+":"+n.id){if(node)return {false,{},common_failure("MilitarySiegeTimeoutCancelledRequestMismatch",id).error};node=&n;}
 if(!node)return {false,{},common_failure("MilitarySiegeTimeoutCancelledRequestMismatch",id).error};std::set<Integer> sides;
 for(const auto& position:core.advanced->after.unit_nodes)if(position.second==node->id){const MilitarySiegeEngineeringUnit* unit=nullptr;for(const auto& u:core.preparation->units)if(u.unit_id==position.first){if(unit)return {false,{},common_failure("MilitarySiegeTimeoutCancelledRequestMismatch",id).error};unit=&u;}if(!unit||unit->fit_count<=0)continue;for(const auto& army:core.advanced->after.armies)if(army.first==unit->army_id&&std::find(core.advanced->after.withdrawn_army_ids.begin(),core.advanced->after.withdrawn_army_ids.end(),army.first)==core.advanced->after.withdrawn_army_ids.end())sides.insert(army.second);}
 if(sides!=std::set<Integer>{0,1})return {false,{},common_failure("MilitarySiegeTimeoutCancelledRequestMismatch",id).error};return {true,node->id,{}};
}
}
bool military_siege_timeout_in_scope(const MilitaryState& m,const MilitaryCatalog& cat,const MilitaryBattleRecord& b){const auto* contact=find(m.contact_facts,b.source_id);const auto* siege=contact&&contact->siege_id?find(m.sieges,*contact->siege_id):nullptr;return siege&&cat.siege_engineering_policy&&std::find(cat.siege_engineering_policy->topology_ids.begin(),cat.siege_engineering_policy->topology_ids.end(),siege->topology_id)!=cat.siege_engineering_policy->topology_ids.end();}
Outcome<std::optional<MilitarySiegeTimeoutRelease>> read_military_siege_timeout_release(const CommonState& c,const MilitaryState& m,const MilitaryCatalog& cat,const MilitaryBattleRecord& b,const DynamicStepInput& phase){
 if(!military_siege_timeout_in_scope(m,cat,b))return {true,std::nullopt,{}};auto limit=upper(c,&phase);if(!limit.ok)return {false,{},limit.error};const auto* contact=find(m.contact_facts,b.source_id);const auto* siege=find(m.sieges,*contact->siege_id);if(siege->phase=="active")return {true,std::nullopt,{}};
 if(!siege->result||siege->result->kind!="timeout")return {true,std::nullopt,{}};if(b.phase!="planning"||b.last_tick!=phase.from_tick||b.created_tick>phase.from_tick||siege->result->tick!=phase.to_tick)return {false,{},common_failure("MilitarySiegeTimeoutCurrentBoundary",b.id).error};auto read=source(c,m,cat,b,limit.value);return read.ok?Outcome<std::optional<MilitarySiegeTimeoutRelease>>{true,std::move(read.value),{}}:Outcome<std::optional<MilitarySiegeTimeoutRelease>>{false,{},read.error};
}
Status validate_military_siege_timeout_releases(const CommonState& c,const MilitaryState& m,const MilitaryCatalog& cat,const DynamicStepInput* phase){
 auto limit=upper(c,phase);if(!limit.ok)return {false,{},limit.error};std::set<StableId> consumed;
 for(const auto& b:m.battles)if(military_siege_timeout_in_scope(m,cat,b)&&b.result&&b.result->reason=="timeout"){
  auto read=source(c,m,cat,b,limit.value);if(!read.ok)return {false,{},read.error};const auto* fact=find(m.battle_facts,b.id);const auto at=position(c,b.id),cause=position(c,read.value.lifecycle_source_id);
  if(!fact||!at||!cause||*cause>=*at||!head(c,fact->id,"military_battle")||b.phase!="closed"||!b.applied||!fact->final||fact->phase!="completed"||fact->session_id!=b.id||fact->contact_source_id!=b.source_id||fact->source_id!=b.source_id||!fact->result||fact->result->reason!="timeout"||fact->result->winner_side!=-1||fact->winner_side!=-1||fact->tick!=read.value.tick||fact->start_tick!=fact->tick||b.completed_tick!=fact->tick||!consumed.insert(b.source_id).second)return common_failure("MilitarySiegeTimeoutBattleSourceRequired",b.id);
 }
 for(const auto& siege:m.sieges)if(cat.siege_engineering_policy&&std::find(cat.siege_engineering_policy->topology_ids.begin(),cat.siege_engineering_policy->topology_ids.end(),siege.topology_id)!=cat.siege_engineering_policy->topology_ids.end()&&siege.result&&siege.result->kind=="timeout"){
  const auto* topology=find(cat.siege_topologies,siege.topology_id);if(!topology)return common_failure("MilitarySiegeTimeoutResultRequired",siege.id);auto core=timeout_core(c,m,siege,*topology,limit.value);if(!core.ok)return {false,{},core.error};if(!core.value.release)continue;std::set<StableId> nodes;
  for(const auto& id:*core.value.release->battle_source_ids){StableId node;if(consumed.count(id)){const auto* contact=find(m.contact_facts,id);if(!contact||contact->siege_id!=std::optional<StableId>{siege.id}||!contact->node_id)return common_failure("MilitarySiegeTimeoutBattleReleaseMissing",id);node=*contact->node_id;}else{auto cancelled=uninstalled_request_node(c,m,siege,*topology,core.value,id);if(!cancelled.ok)return {false,{},cancelled.error};node=cancelled.value;}if(!nodes.insert(node).second)return common_failure("MilitarySiegeTimeoutReleaseNodeRepeated",node);}
 }
 return common_success();
}
}
