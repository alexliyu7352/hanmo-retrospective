// 新围城生命周期的形状和精确前界；经济、军令授权和战果仍须原各域逐来源验证。
#include "military_siege_lifecycle.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <limits>
namespace hanmo::resident {
namespace {
Status fail(const StableId& code,const StableId& path={}){return common_failure(code,path);}
bool fields(FieldOrder actual,FieldOrder expected){if(actual.empty())return true;std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());return actual==expected;}
bool same(const MilitarySiegeParticipationSnapshot& a,const MilitarySiegeParticipationSnapshot& b){return a.phase==b.phase&&a.started_tick==b.started_tick&&a.last_tick==b.last_tick&&a.round==b.round&&a.armies==b.armies&&a.unit_nodes==b.unit_nodes&&a.withdrawn_army_ids==b.withdrawn_army_ids;}
Status snapshot(const MilitarySiegeParticipationSnapshot& s,const SiegeTopology& topology){
 if((s.phase!="active"&&s.phase!="result"&&s.phase!="closed")||s.started_tick<0||s.last_tick<s.started_tick||s.last_tick>2147483647||s.round!=s.last_tick-s.started_tick+1||s.armies.empty()||s.armies.size()>1024||s.unit_nodes.size()>4096||!fields(s.field_order,{"phase","started_tick","last_tick","round","armies","unit_nodes","withdrawn_army_ids"}))return fail("MilitarySiegeParticipationShape");
 std::set<StableId> armies,units,withdrawn;for(const auto& a:s.armies)if(a.first.empty()||(a.second!=0&&a.second!=1)||!armies.insert(a.first).second)return fail("MilitarySiegeParticipationArmy",a.first);
 for(const auto& u:s.unit_nodes)if(u.first.empty()||!units.insert(u.first).second||!std::any_of(topology.nodes.begin(),topology.nodes.end(),[&](const auto& node){return node.id==u.second;}))return fail("MilitarySiegeParticipationNode",u.first);
 for(const auto& a:s.withdrawn_army_ids)if(!armies.count(a)||!withdrawn.insert(a).second)return fail("MilitarySiegeParticipationWithdrawal",a);
 return common_success();
}
bool canonical(const CommonStore& c,const StableId& id){const StableId prefix="military_siege_lifecycle:";if(id.rfind(prefix,0)!=0)return false;Integer n=0;auto tail=id.substr(prefix.size());auto p=std::from_chars(tail.data(),tail.data()+tail.size(),n);return p.ec==std::errc{}&&p.ptr==tail.data()+tail.size()&&n>0&&n<c.state().next_instance_id&&std::to_string(n)==tail;}
}
MilitarySiegeParticipationSnapshot capture_military_siege_participation(const SiegeRecord& s){return {s.phase,s.started_tick,s.last_tick,s.round,s.armies,s.unit_nodes,s.withdrawn_army_ids,{}};}
StableId military_siege_lifecycle_kind_name(MilitarySiegeLifecycleKind kind){switch(kind){case MilitarySiegeLifecycleKind::created:return "military_siege_created";case MilitarySiegeLifecycleKind::joined:return "military_siege_joined";case MilitarySiegeLifecycleKind::advanced:return "military_siege_advanced";case MilitarySiegeLifecycleKind::battle_finished:return "military_siege_battle_finished";case MilitarySiegeLifecycleKind::strategic:return "military_siege_strategic";case MilitarySiegeLifecycleKind::terminated:return "military_siege_terminated";case MilitarySiegeLifecycleKind::closed:return "military_siege_closed";case MilitarySiegeLifecycleKind::integrated:return "military_siege_integrated";case MilitarySiegeLifecycleKind::departed:return "military_siege_departed";}return {};}
Status validate_military_siege_lifecycle_shape(const MilitarySiegeLifecycleFact& f,const SiegeTopology& topology){
 if(f.id.empty()||f.siege_id.empty()||f.topology_id!=topology.id||f.tick<0||military_siege_lifecycle_kind_name(f.kind).empty()||!fields(f.field_order,{"id","siege_id","topology_id","actor_id","cause_id","previous_fact_id","kind","tick","before","after","strategic_command","termination_reason"}))return fail("MilitarySiegeLifecycleShape",f.id);auto checked=snapshot(f.after,topology);if(!checked.ok)return checked;
 if(f.strategic_command.has_value()!=(f.kind==MilitarySiegeLifecycleKind::strategic)||f.termination_reason.has_value()!=(f.kind==MilitarySiegeLifecycleKind::terminated))return fail("MilitarySiegeLifecycleCauseShape",f.id);
 if(f.strategic_command){const auto& command=*f.strategic_command;
  if(f.actor_id.empty()||!command.unexpected_fields.empty()||(command.kind!="withdraw"&&command.kind!="negotiate"))return fail("MilitarySiegeLifecycleCauseShape",f.id);
  if(command.kind=="withdraw"&&(command.army_id.empty()||!command.terms.empty()||!fields(command.field_order,{"kind","army_id"})))return fail("MilitarySiegeLifecycleCauseShape",f.id);
  if(command.kind=="negotiate"&&(!command.army_id.empty()||(command.terms!="safe_departure"&&command.terms!="integrate"&&command.terms!="truce")||!fields(command.field_order,{"kind","terms"})))return fail("MilitarySiegeLifecycleCauseShape",f.id);
 }
 if(f.termination_reason&&*f.termination_reason!="authority_lost"&&*f.termination_reason!="responsible_actor_lost"&&*f.termination_reason!="truce"&&*f.termination_reason!="cancelled")return fail("MilitarySiegeLifecycleCauseShape",f.id);
 if(f.kind==MilitarySiegeLifecycleKind::created){if(f.before||!f.previous_fact_id.empty()||f.cause_id.empty()||f.actor_id.empty()||f.after.phase!="active"||f.after.started_tick!=f.tick||f.after.last_tick!=f.tick||f.after.round!=1||!f.after.withdrawn_army_ids.empty())return fail("MilitarySiegeLifecycleCreation",f.id);return common_success();}
 if(!f.before||f.previous_fact_id.empty()||(f.cause_id.empty()&&f.kind!=MilitarySiegeLifecycleKind::strategic&&f.kind!=MilitarySiegeLifecycleKind::terminated))return fail("MilitarySiegeLifecyclePredecessorRequired",f.id);checked=snapshot(*f.before,topology);if(!checked.ok)return checked;const auto& b=*f.before;const auto& a=f.after;if(b.phase=="closed"||b.started_tick!=a.started_tick||a.last_tick<b.last_tick||a.last_tick>f.tick)return fail("MilitarySiegeLifecycleTime",f.id);
 if(f.kind==MilitarySiegeLifecycleKind::advanced){if(b.phase!="active"||f.tick!=b.last_tick+1||a.last_tick!=f.tick||a.round!=b.round+1||a.armies!=b.armies||a.withdrawn_army_ids!=b.withdrawn_army_ids)return fail("MilitarySiegeLifecycleStep",f.id);std::set<StableId> old_units,new_units;for(const auto& u:b.unit_nodes)old_units.insert(u.first);for(const auto& u:a.unit_nodes)new_units.insert(u.first);if(old_units!=new_units)return fail("MilitarySiegeLifecycleStepRoster",f.id);}
 else if(a.last_tick!=b.last_tick||a.round!=b.round)return fail("MilitarySiegeLifecycleUnexpectedTime",f.id);
 if(f.kind==MilitarySiegeLifecycleKind::joined){if(b.phase!="active"||a.phase!="active"||f.actor_id.empty()||a.armies.size()!=b.armies.size()+1||a.withdrawn_army_ids!=b.withdrawn_army_ids)return fail("MilitarySiegeLifecycleJoin",f.id);for(const auto& x:b.armies)if(std::find(a.armies.begin(),a.armies.end(),x)==a.armies.end())return fail("MilitarySiegeLifecycleJoin",f.id);for(const auto& x:b.unit_nodes)if(std::find(a.unit_nodes.begin(),a.unit_nodes.end(),x)==a.unit_nodes.end())return fail("MilitarySiegeLifecycleJoin",f.id);}
 else if(a.armies!=b.armies)return fail("MilitarySiegeLifecycleUnexpectedArmy",f.id);
 // 除真实加入和原战略撤离外，任何来源均不能凭快照添兵或静默丢失参与者。
 if(f.kind==MilitarySiegeLifecycleKind::battle_finished){std::set<StableId> old_units,new_units;for(const auto& u:b.unit_nodes)old_units.insert(u.first);for(const auto& u:a.unit_nodes)new_units.insert(u.first);if(b.phase!="active"||a.phase!="active"||old_units!=new_units||a.withdrawn_army_ids!=b.withdrawn_army_ids)return fail("MilitarySiegeLifecycleBattleRoster",f.id);}
 if(f.kind==MilitarySiegeLifecycleKind::strategic||f.kind==MilitarySiegeLifecycleKind::departed){
  if((f.kind==MilitarySiegeLifecycleKind::strategic?b.phase!="active":(b.phase!="result"||a.phase!="result"))||a.withdrawn_army_ids.size()<b.withdrawn_army_ids.size()||!std::equal(b.withdrawn_army_ids.begin(),b.withdrawn_army_ids.end(),a.withdrawn_army_ids.begin()))return fail("MilitarySiegeLifecycleWithdrawal",f.id);
  for(const auto& u:a.unit_nodes)if(std::find(b.unit_nodes.begin(),b.unit_nodes.end(),u)==b.unit_nodes.end())return fail("MilitarySiegeLifecycleUnexpectedUnit",u.first);
 }
 if((f.kind==MilitarySiegeLifecycleKind::terminated||f.kind==MilitarySiegeLifecycleKind::integrated)&&(a.unit_nodes!=b.unit_nodes||a.withdrawn_army_ids!=b.withdrawn_army_ids))return fail("MilitarySiegeLifecycleUnexpectedUnit",f.id);
 if(f.kind==MilitarySiegeLifecycleKind::closed){auto expected=b;expected.phase="closed";if(b.phase!="result"||a.phase!="closed"||f.actor_id.empty()||!same(expected,a))return fail("MilitarySiegeLifecycleClose",f.id);}
 if((f.kind==MilitarySiegeLifecycleKind::terminated||f.kind==MilitarySiegeLifecycleKind::integrated)&&(b.phase!="active"||a.phase!="result"))return fail("MilitarySiegeLifecycleTermination",f.id);
 return common_success();
}
static Outcome<MilitarySiegeParticipationSnapshot> participation_before(const CommonStore& c,const MilitarySiegeLifecycleState& state,const StableId& siege_id,const StableId& before,Integer upper){
 const auto& order=c.state().facts.order();auto end=std::find(order.begin(),order.end(),before);const auto* boundary=c.state().facts.find(before);if(!boundary||boundary->id!=before||end==order.end()||before.rfind("military_siege_engineering:",0)!=0||boundary->kind!="military_siege_engineering_prepared"||boundary->domain!="military")return {false,{},fail("MilitarySiegeParticipationBoundaryRequired",before).error};
 std::vector<const MilitarySiegeLifecycleFact*> entries;for(const auto& f:state.facts)if(f.siege_id==siege_id){auto at=std::find(order.begin(),order.end(),f.id);if(at==order.end())return {false,{},fail("MilitarySiegeLifecycleFactMissing",f.id).error};if(at<end)entries.push_back(&f);}
 std::sort(entries.begin(),entries.end(),[&](const auto* a,const auto* b){return std::find(order.begin(),order.end(),a->id)<std::find(order.begin(),order.end(),b->id);});if(entries.empty())return {false,{},fail("MilitarySiegeParticipationHistoryMissing",siege_id).error};const MilitarySiegeLifecycleFact* previous=nullptr;
 for(const auto* f:entries){const auto* head=c.state().facts.find(f->id);if(!canonical(c,f->id)||!head||head->id!=f->id||head->kind!=military_siege_lifecycle_kind_name(f->kind)||head->domain!="military"||f->tick<0||f->tick>upper)return {false,{},fail("MilitarySiegeLifecycleFactMismatch",f->id).error};if(previous){if(f->previous_fact_id!=previous->id||!f->before||!same(*f->before,previous->after)||f->topology_id!=previous->topology_id||f->tick<previous->tick)return {false,{},fail("MilitarySiegeParticipationHistoryMismatch",f->id).error};}else if(f->kind!=MilitarySiegeLifecycleKind::created||f->before||!f->previous_fact_id.empty())return {false,{},fail("MilitarySiegeParticipationCreationMissing",f->id).error};previous=f;}
 return {true,previous->after,{}};
}
Outcome<MilitarySiegeParticipationSnapshot> military_siege_participation_before(const CommonStore& c,const MilitarySiegeLifecycleState& state,const StableId& siege,const StableId& before){return participation_before(c,state,siege,before,c.tick());}
Outcome<MilitarySiegeParticipationSnapshot> military_siege_participation_before(const CommonStore& c,const MilitarySiegeLifecycleState& state,const StableId& siege,const StableId& before,const DynamicStepInput& phase){if(phase.from_tick<0||phase.from_tick!=c.tick()||phase.from_tick==std::numeric_limits<Integer>::max()||phase.to_tick!=phase.from_tick+1)return {false,{},fail("MilitarySiegeLifecyclePhaseMismatch",before).error};return participation_before(c,state,siege,before,phase.to_tick);}

}
