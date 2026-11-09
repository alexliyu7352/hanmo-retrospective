#include "care_handover_command_source.hpp"
#include "resident/domains/director/director_command_acceptance.hpp"
#include "resident/kernel/command_types.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
void check(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
bool same_time(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
bool same_catalog(const CatalogIdentity& a,const CatalogIdentity& b){return a.content_id==b.content_id&&a.content_version==b.content_version&&a.content_digest==b.content_digest&&a.ruleset_id==b.ruleset_id&&a.schema_sha256==b.schema_sha256&&a.pack_set_digest==b.pack_set_digest&&a.module_versions==b.module_versions&&a.artifact_sha256==b.artifact_sha256;}
bool same_catalog_field(const LifeCatalogFieldRef& a,const LifeCatalogFieldRef& b){return same_catalog(a.catalog_identity,b.catalog_identity)&&a.scenario_id==b.scenario_id&&a.entity_id==b.entity_id&&a.field_selector==b.field_selector;}
WorldMoment context(const CareHandoverCommandSourceContext& x){
 const auto& a=x.handovers.acceptance;const auto& c=a.goals.common;
 if((a.goals.rules.selected_care_version()!=5&&a.goals.rules.selected_care_version()!=6)||&a.controls.common!=&c||a.controls.foundation!=&a.body.state||a.controls.foundation_catalog!=&a.body.catalog||!a.controls.actual_clock)fail("CareCommandOriginalControlSourcesRequired",{},ErrorKind::unsupported_capability);
 auto now=take(a.goals.clock.read_current_moment());if(!same_time(now,a.body.actual_now)||!same_time(now,take(a.controls.actual_clock->read_current_moment())))fail("CareCommandActualClockMismatch");return now;
}
struct Actual{const CareHandoverExecution* execution=nullptr;const CareHandoverExecutionBoundary1* boundary=nullptr;};
Actual find(const CareHandoverCommandSourceContext& x,const StableId& id){
 Actual result;for(const auto& pair:x.handovers.executions.rows())for(const auto& b:pair.second.handover_execution_boundaries)if(b.boundary_id==id){if(result.boundary||pair.first!=pair.second.handover_id||b.execution_ref!=pair.first)fail("CareCommandOriginalBoundaryMismatch",id);result={&pair.second,&b};}
 if(!result.boundary)fail("CareCommandOriginalBoundaryRequired",id);return result;
}
const CareHandoverActorCommand& original_command(const CareHandoverCommandSourceContext& x,const Actual& actual){
 const auto now=context(x);const auto& e=*actual.execution;const auto& b=*actual.boundary;const auto& common=x.handovers.acceptance.goals.common;
 check(validate_care_handover_execution_shape(e,5));
 const auto* head=common.state().facts.find(e.handover_id);const auto& order=common.state().facts.order();const auto adopted=std::find(order.begin(),order.end(),e.accepted_care_terms_ref);const auto started_at=std::find(order.begin(),order.end(),e.handover_id);if(!head||head->id!=e.handover_id||head->domain!="care_requirements"||head->kind!="care_handover_execution"||std::count(order.begin(),order.end(),e.handover_id)!=1||adopted==order.end()||started_at==order.end()||adopted>=started_at)fail("CareActualCommandExecutionSourceRequired",e.handover_id);

 const auto* command=std::get_if<CareHandoverActorCommand>(&b.cause);
 if(!command||b.kind!=CareHandoverBoundaryKind::started||b.boundary_ordinal!=0||command->command_binding.command_kind!="start_care_handover"||command->accepted_terms_ref!=e.accepted_care_terms_ref||take(compare_world_moments(b.occurred_at,now))>0)fail("CareActualStartedCommandRequired",b.boundary_id);
 const auto* consent=take(read_accepted_caregiver(x.handovers.acceptance,care_acceptance_source(e.accepted_care_terms_ref)));
 if(consent->requirement_id!=e.requirement_id||take(compare_world_moments(consent->accepted_at,b.occurred_at))>0)fail("CareCommandAcceptedTermsMismatch",e.handover_id);
 std::size_t workers=0;for(const auto& frame:b.worker_capability_inputs)if(frame.actor_id==command->actor_id)++workers;if(workers!=1||command->actor_id==consent->subject_actor_id)fail("CareCommandActualWorkerRequired",command->actor_id);
 const auto& binding=command->command_binding;const auto key=command->actor_id+":"+std::to_string(binding.request_seq);const auto* retained=common.state().results.find(key);
 if(!retained||!*retained||!(*retained)->ok||!(*retained)->details||(*retained)->result_id!=binding.result_id||(*retained)->kind!=binding.command_kind||(*retained)->tick!=b.occurred_at.half_day_tick||(*retained)->revision<1||(*retained)->revision>common.revision())fail("CareRetainedActualCommandRequired",b.boundary_id);
 const auto& result=**retained;const auto* started=std::get_if<ActionStarted>(&result.details->data);if(!started||started->action_id!=e.original_action_ref)fail("CareActualStartResultMismatch",b.boundary_id);
 std::size_t matches=0;for(const auto& h:common.state().history)if(h.result_id==binding.result_id){++matches;if(h.actor_id!=command->actor_id||h.kind!=binding.command_kind||h.tick!=result.tick)fail("CareActualCommandHistoryMismatch",b.boundary_id);}if(matches!=1)fail("CareActualCommandHistoryRequired",b.boundary_id);
 // Command and History establish the real action result; S0 is deliberately
 // absent here so initial adoption cannot certify its own authority.
 return *command;
}
LifePlayerActorRoute player_route(const CareHandoverCommandSourceContext& x,const ProfileControlEligibility& control){
 const auto& a=x.handovers.acceptance;LifePlayerActorRoute route;
 std::visit([&](const auto& value){using T=std::decay_t<decltype(value)>;
  if constexpr(std::is_same_v<T,AuthoredProfileController>)route.original_control_source_ref=LifeCatalogFieldRef{a.body.catalog.identity,value.scenario_id,value.actor_id,"player_actor"};
  else{const StableId* id=nullptr;if constexpr(std::is_same_v<T,CreatedProfileController>)id=&value.creation_fact_id;else if constexpr(std::is_same_v<T,SuccessionProfileController>)id=&value.succession_fact_id;else if constexpr(std::is_same_v<T,ContinuationProfileController>)id=&value.activation_id;else id=&value.activation_fact_id;
   const auto* head=a.goals.common.state().facts.find(*id);if(!head||head->id!=*id)fail("CareActualControlSourceRequired",*id);route.original_control_source_ref=SourceRef{head->domain,head->kind,*id};}
 },control.source);return route;
}
LifeNpcActorRoute npc_route(const CareHandoverCommandSourceContext& x,const Actual& actual,const CareHandoverActorCommand& command){
 if(!x.director||!x.director_catalog||!x.director->subject2)fail("CareNpcOriginalDecisionRequired",actual.boundary->boundary_id,ErrorKind::unsupported_capability);
 const DirectorAdoptionResult2* decision=nullptr;for(const auto& row:x.director->subject2->adoption_results.rows())if(row.second.accepted_command&&row.second.accepted_command->result_id==command.command_binding.result_id){if(decision)fail("CareNpcAmbiguousAdoption",actual.boundary->boundary_id);decision=&row.second;}
 if(!decision)fail("CareNpcOriginalDecisionRequired",actual.boundary->boundary_id,ErrorKind::unsupported_capability);
 const auto& common=x.handovers.acceptance.goals.common;auto adopted=take(read_director_accepted_command(common,*x.director,*x.director_catalog,x.handovers.acceptance.body.catalog.identity,{command.actor_id,decision->registration_epoch,decision->decision_seq,command.command_binding.command_kind,command.command_binding.request_seq,command.command_binding.result_id}));
 const auto& source=*adopted.command;if(!same_time(source.accepted_at,actual.boundary->occurred_at)||source.actual_effect.source_schema!=3||!same_source_ref(source.actual_effect.source_ref,care_handover_execution_source(actual.execution->handover_id)))fail("CareNpcActualBusinessSourceMismatch",actual.boundary->boundary_id);
 LifeNpcActorRoute out;out.subject_actor_id=command.actor_id;out.registration_epoch=decision->registration_epoch;out.decision_seq=decision->decision_seq;out.policy_ref=LifeCatalogFieldRef{source.policy_catalog,common.state().metadata.scenario_id,source.policy_id,"director_policy"};return out;
}
LifeActorRoute route(const CareHandoverCommandSourceContext& x,const Actual& actual,const CareHandoverActorCommand& command){
 auto control=take(read_profile_control_before_care_handover_boundary(x.handovers.acceptance.controls,x.handovers.executions,actual.boundary->boundary_id));
 if(control.actor_id==command.actor_id)return player_route(x,control);return npc_route(x,actual,command);
}
bool same_route(const LifeActorRoute& a,const LifeActorRoute& b){
 if(a.index()!=b.index())return false;
 if(const auto* p=std::get_if<LifePlayerActorRoute>(&a)){const auto& q=std::get<LifePlayerActorRoute>(b);const auto& x=p->original_control_source_ref;const auto& y=q.original_control_source_ref;if(x.index()!=y.index())return false;if(const auto* ref=std::get_if<SourceRef>(&x))return same_source_ref(*ref,std::get<SourceRef>(y));if(const auto* field=std::get_if<LifeCatalogFieldRef>(&x))return same_catalog_field(*field,std::get<LifeCatalogFieldRef>(y));return false;}
 const auto& x=std::get<LifeNpcActorRoute>(a);const auto& y=std::get<LifeNpcActorRoute>(b);const auto* px=std::get_if<LifeCatalogFieldRef>(&x.policy_ref);const auto* py=std::get_if<LifeCatalogFieldRef>(&y.policy_ref);
 return x.subject_actor_id==y.subject_actor_id&&x.registration_epoch==y.registration_epoch&&x.decision_seq==y.decision_seq&&px&&py&&same_catalog_field(*px,*py);
}
} // namespace
Outcome<LifeActorRoute> capture_care_handover_command_route(const CareHandoverCommandSourceContext& x,const StableId& id){try{const auto actual=find(x,id);const auto& command=original_command(x,actual);return {true,route(x,actual,command),{}};}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_care_handover_command_source(const CareHandoverCommandSourceContext& x,const CareHandoverExecution& e,const CareHandoverExecutionBoundary1& b){try{
 const auto actual=find(x,b.boundary_id);if(actual.execution!=&e||actual.boundary!=&b)fail("CareCommandRetainedOriginalRequired",b.boundary_id);const auto& command=original_command(x,actual);if(!same_route(command.route_basis,route(x,actual,command)))fail("CareCommandFrozenRouteMismatch",b.boundary_id);return common_success();
}catch(Failure& error){return {false,{},std::move(error.error)};}}
Outcome<CareHandoverStartedBusinessIdentity> read_care_handover_started_business_identity(const CareHandoverCommandSourceContext& x,const MomentOwnerSource& source){try{
 if(source.source_schema!=3||source.source_ref.domain!="care_requirements"||source.source_ref.kind!="care_handover_execution")fail("CareStartedOriginalSourceRequired",source.source_ref.id);
 const auto* e=x.handovers.executions.find(source.source_ref.id);if(!e||e->handover_id!=source.source_ref.id||e->handover_execution_boundaries.empty())fail("CareStartedOriginalExecutionMissing",source.source_ref.id);const auto& b=e->handover_execution_boundaries.front();const Actual actual{e,&b};const auto& command=original_command(x,actual);return {true,{&command.actor_id,&e->original_action_ref,&command.command_binding,&b.occurred_at},{}};
}catch(Failure& error){return {false,{},std::move(error.error)};}}
Status visit_care_handover_command_result_references(const CareHandoverCommandSourceContext& x,CareHandoverCommandResultSink& sink){try{
 context(x);for(const auto& pair:x.handovers.executions.rows())for(const auto& b:pair.second.handover_execution_boundaries)if(const auto* command=std::get_if<CareHandoverActorCommand>(&b.cause)){
  check(validate_care_handover_command_source(x,pair.second,b));check(sink.actual_reference({pair.first,b.boundary_id,command->actor_id,command->command_binding.command_kind,command->command_binding.result_id,command->command_binding.request_seq,b.boundary_ordinal,std::get_if<LifeNpcActorRoute>(&command->route_basis)}));
 }return common_success();
}catch(Failure& error){return {false,{},std::move(error.error)};}}
}
