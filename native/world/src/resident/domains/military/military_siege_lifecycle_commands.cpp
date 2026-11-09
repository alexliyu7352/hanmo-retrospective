// 所有参与变化都由原纯规则产生；失败没有可发布半结果，外层World事务仍唯一负责钱货与人身写集。
#include "military_siege_lifecycle_commands.hpp"
#include "military_siege_office_boundary.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
bool same(const MilitarySiegeParticipationSnapshot& a,const MilitarySiegeParticipationSnapshot& b){return a.phase==b.phase&&a.started_tick==b.started_tick&&a.last_tick==b.last_tick&&a.round==b.round&&a.armies==b.armies&&a.unit_nodes==b.unit_nodes&&a.withdrawn_army_ids==b.withdrawn_army_ids;}
Status source(const CommonStore& c,const StableId& id,const StableId& kind){const auto* h=c.state().facts.find(id);return h&&h->id==id&&h->kind==kind&&h->domain=="military"?common_success():common_failure("MilitarySiegeLifecycleCauseRequired",id);}
// 精确原单步才允许to；稳定命令只能写当前半日。
Status clock(const CommonStore& c,Integer tick,const DynamicStepInput* phase){if(!phase)return tick==c.tick()?common_success():common_failure("MilitarySiegeLifecyclePhaseMismatch");return phase->from_tick>=0&&phase->from_tick==c.tick()&&phase->from_tick<std::numeric_limits<Integer>::max()&&phase->to_tick==phase->from_tick+1&&tick==phase->to_tick?common_success():common_failure("MilitarySiegeLifecyclePhaseMismatch");}
Outcome<StableId> append(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeTopology& t,const SiegeRecord* before,const SiegeRecord& after,MilitarySiegeLifecycleKind kind,const StableId& actor,const StableId& cause,Integer tick,std::optional<SiegeStrategicCommand> strategy={},std::optional<StableId> reason={}){
 MilitarySiegeLifecycleFact f;f.id="uncommitted";f.siege_id=after.id;f.topology_id=after.topology_id;f.actor_id=actor;f.cause_id=cause;f.kind=kind;f.tick=tick;f.after=capture_military_siege_participation(after);f.strategic_command=std::move(strategy);f.termination_reason=std::move(reason);const MilitarySiegeLifecycleFact* last=nullptr;
 for(const auto& row:state.facts)if(row.siege_id==after.id)last=&row;
 if(last){auto checked=source(c,last->id,military_siege_lifecycle_kind_name(last->kind));if(!checked.ok)return {false,{},checked.error};}
 if(before){f.before=capture_military_siege_participation(*before);if(!last||!same(last->after,*f.before)||last->topology_id!=t.id)return {false,{},common_failure("MilitarySiegeLifecyclePredecessorRequired",after.id).error};f.previous_fact_id=last->id;}else if(last)return {false,{},common_failure("MilitarySiegeLifecycleAlreadyCreated",after.id).error};
 auto checked=validate_military_siege_lifecycle_shape(f,t);if(!checked.ok)return {false,{},checked.error};auto id=c.next_id("military_siege_lifecycle");if(!id.ok)return id;f.id=id.value;checked=c.append_fact({f.id,military_siege_lifecycle_kind_name(kind),"military"});if(!checked.ok)return {false,{},checked.error};state.facts.push_back(std::move(f));return id;
}
}
Outcome<SiegeRecord> create_military_siege_with_lifecycle(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeCreateRequest& request,const SiegeTopology& t,const SiegeObservation& o,const StableId& actor,const StableId& arrival){
 auto check=clock(c,o.tick,nullptr);if(!check.ok)return {false,{},check.error};check=source(c,arrival,"campaign_arrival");if(!check.ok)return {false,{},check.error};if(actor.empty())return {false,{},common_failure("SiegeCommanderUnauthorized",actor).error};auto made=create_siege(request,t,o);if(!made.ok)return made;auto saved=append(c,state,t,nullptr,made.value,MilitarySiegeLifecycleKind::created,actor,arrival,c.tick());return saved.ok?std::move(made):Outcome<SiegeRecord>{false,{},saved.error};
}
Outcome<SiegeRecord> join_military_siege_with_lifecycle(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& actor,const StableId& army,Integer side,const StableId& arrival){
 auto check=clock(c,o.tick,nullptr);if(!check.ok)return {false,{},check.error};check=source(c,arrival,"campaign_arrival");if(!check.ok)return {false,{},check.error};auto leader=std::find_if(o.armies.begin(),o.armies.end(),[&](const auto& a){return a.id==army;});if(leader==o.armies.end()||leader->commander_actor_id!=actor)return {false,{},common_failure("SiegeCommanderUnauthorized",actor).error};auto made=join_siege(s,t,o,army,side,arrival);if(!made.ok)return made;auto saved=append(c,state,t,&s,made.value,MilitarySiegeLifecycleKind::joined,actor,arrival,c.tick());return saved.ok?std::move(made):Outcome<SiegeRecord>{false,{},saved.error};
}
Outcome<SiegeResolution> step_military_siege_with_lifecycle(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& actor,const SiegeOrders& orders,const DynamicStepInput& phase){
 auto check=clock(c,phase.to_tick,&phase);if(!check.ok)return {false,{},check.error};if(!o.engineering_preparation||o.engineering_preparation->from_tick!=phase.from_tick||o.engineering_preparation->to_tick!=phase.to_tick||o.engineering_preparation->round!=s.round)return {false,{},common_failure("MilitarySiegeEngineeringPreparationRequired",s.id).error};const auto& p=*o.engineering_preparation;check=source(c,p.id,"military_siege_engineering_prepared");if(!check.ok)return {false,{},check.error};auto made=resolve_siege(s,t,o,actor,orders,phase.to_tick);if(!made.ok)return made;auto saved=append(c,state,t,&s,made.value.siege,MilitarySiegeLifecycleKind::advanced,actor,p.id,phase.to_tick);return saved.ok?std::move(made):Outcome<SiegeResolution>{false,{},saved.error};
}
Outcome<SiegeRecord> finish_military_siege_battle_with_lifecycle(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& node,const StableId& battle,const DynamicStepInput* phase){
 // 原节点完成可能在本半日battle phase；完成tick已由真实military_battle载荷及外层阶段先验。
 auto check=source(c,battle,"military_battle");if(!check.ok)return {false,{},check.error};check=clock(c,o.tick,phase);if(!check.ok)return {false,{},check.error};auto made=finish_siege_battle(s,t,o,node,battle);if(!made.ok)return made;auto saved=append(c,state,t,&s,made.value,MilitarySiegeLifecycleKind::battle_finished,{},battle,o.tick);return saved.ok?std::move(made):Outcome<SiegeRecord>{false,{},saved.error};
}
Outcome<SiegeResolution> manage_military_siege_with_lifecycle(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& actor,const SiegeStrategicCommand& command){
 auto check=clock(c,o.tick,nullptr);if(!check.ok)return {false,{},check.error};auto made=strategic_siege(s,t,o,actor,command);if(!made.ok)return made;auto saved=append(c,state,t,&s,made.value.siege,MilitarySiegeLifecycleKind::strategic,actor,{},c.tick(),command);return saved.ok?std::move(made):Outcome<SiegeResolution>{false,{},saved.error};
}
Outcome<SiegeResolution> terminate_military_siege_with_lifecycle(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeRecord& s,const SiegeTopology& t,const SiegeObservation& o,const StableId& reason,const DynamicStepInput* phase){
 auto check=validate_military_siege_tracked_termination_reason(reason);if(!check.ok)return {false,{},check.error};check=clock(c,o.tick,phase);if(!check.ok)return {false,{},check.error};auto made=terminate_siege(s,t,o,reason);if(!made.ok)return made;auto saved=append(c,state,t,&s,made.value.siege,MilitarySiegeLifecycleKind::terminated,{},{},o.tick,{},reason);return saved.ok?std::move(made):Outcome<SiegeResolution>{false,{},saved.error};
}
Status record_departed_military_siege_lifecycle(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeRecord& before,const SiegeRecord& after,const SiegeTopology& t){
 const MilitarySiegeLifecycleFact* strategy=nullptr;for(const auto& f:state.facts)if(f.siege_id==after.id)strategy=&f;
 if(before.id!=after.id||!strategy||strategy->kind!=MilitarySiegeLifecycleKind::strategic||!strategy->strategic_command||strategy->strategic_command->kind!="negotiate"||strategy->strategic_command->terms!="safe_departure"||strategy->tick!=c.tick())return common_failure("MilitarySiegeLifecycleDepartureSourceRequired",after.id);
 const auto cause=strategy->id,actor=strategy->actor_id;auto saved=append(c,state,t,&before,after,MilitarySiegeLifecycleKind::departed,actor,cause,c.tick());return saved.ok?common_success():Status{false,{},saved.error};
}
Outcome<SiegeRecord> close_military_siege_with_lifecycle(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeRecord& s,const SiegeTopology& t,const StableId& actor,const StableId& release){
 auto check=source(c,release,"military_siege_released");if(!check.ok)return {false,{},check.error};if(actor.empty()||s.phase!="result")return {false,{},common_failure("SiegeNotFinished",s.id).error};auto after=s;after.phase="closed";auto saved=append(c,state,t,&s,after,MilitarySiegeLifecycleKind::closed,actor,release,c.tick());return saved.ok?Outcome<SiegeRecord>{true,std::move(after),{}}:Outcome<SiegeRecord>{false,{},saved.error};
}
Status record_integrated_military_siege_lifecycle(CommonStore& c,MilitarySiegeLifecycleState& state,const SiegeRecord& before,const SiegeRecord& after,const SiegeTopology& t,const StableId& transfer,Integer tick){
 auto check=clock(c,tick,nullptr);if(!check.ok)return check;check=source(c,transfer,"military_integration");if(!check.ok)return check;if(before.id!=after.id||!after.result||after.result->kind!="surrendered"||after.result->terms!=std::optional<StableId>{"integrate"}||after.result->tick!=tick)return common_failure("MilitarySiegeLifecycleIntegrationRequired",transfer);auto saved=append(c,state,t,&before,after,MilitarySiegeLifecycleKind::integrated,{},transfer,tick);return saved.ok?common_success():Status{false,{},saved.error};
}
}
