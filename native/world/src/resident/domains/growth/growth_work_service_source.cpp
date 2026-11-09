#include "resident/domains/growth/growth_model.hpp"
#include "growth_work_service_source.hpp"
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>v){if(!v.ok)throw Failure{std::move(v.error)};return std::move(v.value);}
template<class T,class F>Outcome<T> checked(F fn){try{return {true,fn(),{}};}catch(Failure&e){return {false,{},std::move(e.error)};}}
const ActionTimeRegistration& selected(const GrowthWorkServiceSources&c){
 if(!c.catalog.regional_life||!c.catalog.complete_exact_period_sources||!c.catalog.complete_study_sources)fail("OriginalWorkServiceExactSourcesRequired",{},ErrorKind::unsupported_capability);
 const ActionTimeRegistration*result=nullptr;for(const auto&r:c.registrations)if(r.owner_domain=="growth"&&r.action_kind=="service"){if(result)fail("DuplicateOriginalWorkServiceRegistration");result=&r;}
 if(!result)fail("OriginalWorkServiceRegistrationRequired",{},ErrorKind::unsupported_capability);return *result;
}
const LifeActivityRecord& work(const GrowthWorkServiceSources&c,const StableId&id){
 const auto*row=c.growth.life_activities.find(id);const auto*h=c.common.state().facts.find(id);
 if(!row||!h||h->id!=id||h->domain!="growth"||h->kind!="life_activity"||row->view.fact_id!=id||row->view.action_id!=id||row->view.kind!=LifeActivityKind::service||row->view.activity!="work"||row->context.provider_actor_id!=row->view.provider_actor_id||row->view.actor_id==row->view.provider_actor_id)fail("OriginalWorkServiceSourceRequired",id);
 return *row;
}
}
Status validate_original_work_service_completion_candidate(const GrowthWorkServiceSources&c,const StableId&id,const WorldMoment&now){return checked<Done>([&]{
 const auto&reg=selected(c);const auto&row=work(c,id);const auto*action=c.common.action(id);const auto*payload=c.growth.actions.find(id);
 if(row.view.phase!=LifeActivityPhase::active||!action||!payload||action->kind!="service"||action->actor_id!=row.view.actor_id||payload->request.activity!="work")fail("OriginalWorkServiceActiveSourceRequired",id);
 GrowthActionTimeRead time(c.common,c.growth,c.catalog,c.shared,c.elapsed,c.scale,c.identity,now,c.budget,c.interaction,c.interaction_catalog);
 auto proof=take(time.read_action_time_owner(reg,id));if(proof.phase!=ActionTimeOwnerPhase::working)fail("OriginalWorkServiceRunningSourceRequired",id);
 return Done{};
});}
Outcome<OriginalCompletedWorkService> read_original_completed_work_service(const GrowthWorkServiceSources&c,const SourceRef&ref,Integer schema,const WorldMoment&now){return checked<OriginalCompletedWorkService>([&]{
 if(schema!=1||ref.domain!="growth"||ref.kind!="growth_result_terminal")fail("OriginalWorkServiceTerminalSchemaMismatch",ref.id);
 const auto&reg=selected(c);GrowthActionTimeRead time(c.common,c.growth,c.catalog,c.shared,c.elapsed,c.scale,c.identity,now,c.budget,c.interaction,c.interaction_catalog);
 auto actual=take(read_growth_terminal_publication(c.common,c.growth,time,reg,ref.id));const auto&pub=*actual.publication;
 const auto&row=work(c,pub.action_id);const auto*boundary=c.growth.fixed_service_boundaries.find(pub.terminal_source_ref.id);
 const auto*head=c.common.state().facts.find(ref.id);
 if(row.view.phase!=LifeActivityPhase::completed||!row.view.success||!row.view.code.empty()||actual.action_kind!="service"||actual.actor_id!=row.view.actor_id||pub.original_result_ref.kind!=LifeFixedOutcomeRef::Kind::life_activity||pub.original_result_ref.id!=pub.action_id||!boundary||boundary->kind!=LifeFixedBoundaryKind::completed||boundary->action_ref!=pub.action_id||!head||head->domain!=ref.domain||head->kind!=ref.kind||head->id!=ref.id)fail("OriginalSuccessfulWorkServiceRequired",ref.id);
 return OriginalCompletedWorkService{&row,&pub,head};
});}
Outcome<OriginalCompletedWorkService> read_original_work_service_at_terminal_boundary(const GrowthWorkServiceSources&c,const SourceRef&boundary,Integer schema,const WorldMoment&now){return checked<OriginalCompletedWorkService>([&]{
 if(schema!=1||boundary.domain!="growth"||boundary.kind!="growth_fixed_service_boundary")fail("OriginalWorkServiceBoundarySchemaMismatch",boundary.id);
 const GrowthTerminalPublication*publication=nullptr;for(const auto&row:c.growth.terminal_publications.rows())if(row.second.terminal_source_schema==schema&&same_source_ref(row.second.terminal_source_ref,boundary)){if(publication)fail("DuplicateOriginalWorkServicePublication",boundary.id);publication=&row.second;}
 if(!publication)fail("OriginalWorkServicePublicationRequired",boundary.id);
 return take(read_original_completed_work_service(c,{"growth","growth_result_terminal",publication->publication_id},1,now));
});}
}
