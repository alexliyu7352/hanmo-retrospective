#include "resident/domains/growth/growth_model.hpp"
#include "social_relation_original_sources.hpp"
namespace hanmo::resident {namespace {
Status required(const char*code){return common_failure(code,{},ErrorKind::unsupported_capability);}
Status invalid(const char*code,const StableId&id={}){return common_failure(code,id,ErrorKind::internal);}
std::size_t count(const SocialState&s,const OriginalSocialRelationCause&cause){std::size_t n=0;for(const auto&row:s.relation_results.rows())if(same_original_social_relation_cause(row.second.original_cause,cause))++n;return n;}
// One finite original effect enumeration is shared by the writer and reverse
// coverage. It does not create a second rule/delta or infer generic news effects.
template<class Emit>Status original_event_causes(const SocialState&s,const SocialEvent&e,Emit&&emit){
 Status valid=common_success();
 if(e.kind=="theft"){valid=emit(SocialExchangeRelationCause{e.id,OriginalSocialExchangeRole::theft_victim});if(valid.ok)valid=emit(SocialExchangeRelationCause{e.id,OriginalSocialExchangeRole::theft_actor});}
 else if(e.kind=="aid"){valid=emit(SocialExchangeRelationCause{e.id,OriginalSocialExchangeRole::aid_receiver});if(valid.ok)valid=emit(SocialExchangeRelationCause{e.id,OriginalSocialExchangeRole::aid_helper});}
 else if(e.kind=="refusal")valid=emit(SocialExchangeRelationCause{e.id,OriginalSocialExchangeRole::refusal});
 else if(e.kind=="news"){const auto*original=s.events.find(e.fact_id);if(original&&original->actor_id!=e.target_actor_id&&(original->kind=="theft"||original->kind=="aid"))valid=emit(SocialNewsRelationCause{e.id});}
 return valid;
}
Status before_result(const CommonStore&common,const OriginalSocialRelationBusinessSourceView&cause,const FactRecord&result){
 if(!cause.id)return invalid("InvalidOriginalRelationCause");
 bool seen=false;for(const auto&head:common.state().facts.rows()){
  if(head.first==*cause.id){if(head.second.kind!=cause.kind||head.second.domain!=cause.domain)return invalid("OriginalRelationCauseHeaderMismatch",head.first);seen=true;}
  if(head.first==result.id)return seen&&&head.second==&result?common_success():invalid("OriginalRelationCauseNotBeforeResult",result.id);
 }return invalid("OriginalRelationResultHeaderRequired",result.id);
}
}
Outcome<OriginalSocialRelationApplication> NativeOriginalSocialRelationSourceRead::read_original_application(const OriginalSocialRelationCause&cause,const WorldMoment&now)const{
 if(const auto*exchange=std::get_if<SocialExchangeRelationCause>(&cause)){
  if(!events_||!bound_to_events(*events_))return {false,{},required("OriginalSocialCalendarSourceRequired").error};
  auto read=read_original_social_calendar_event(*events_,{"social","social_event_applied",exchange->event_id},1,now);if(!read.ok)return {false,{},read.error};const auto&e=*read.value;
  const StableId*from=nullptr;const StableId*to=nullptr;OriginalSocialRelationRule rule=OriginalSocialRelationRule::theft_victim;
  switch(exchange->role){
   case OriginalSocialExchangeRole::theft_victim:if(e.kind=="theft"){from=&e.target_actor_id;to=&e.actor_id;rule=OriginalSocialRelationRule::theft_victim;}break;
   case OriginalSocialExchangeRole::theft_actor:if(e.kind=="theft"){from=&e.actor_id;to=&e.target_actor_id;rule=OriginalSocialRelationRule::theft_actor;}break;
   case OriginalSocialExchangeRole::refusal:if(e.kind=="refusal"){from=&e.target_actor_id;to=&e.actor_id;rule=OriginalSocialRelationRule::refusal;}break;
   case OriginalSocialExchangeRole::aid_receiver:if(e.kind=="aid"){from=&e.target_actor_id;to=&e.actor_id;rule=OriginalSocialRelationRule::aid_receiver;}break;
   case OriginalSocialExchangeRole::aid_helper:if(e.kind=="aid"){from=&e.actor_id;to=&e.target_actor_id;rule=OriginalSocialRelationRule::aid_helper;}break;
  }
  if(!from)return {false,{},invalid("OriginalSocialExchangeRoleMismatch",e.id).error};
  return {true,{from,to,&e.original_application->occurred_at,rule},{}};
 }
 if(const auto*news=std::get_if<SocialNewsRelationCause>(&cause)){
  if(!events_||!bound_to_events(*events_))return {false,{},required("OriginalSocialCalendarSourceRequired").error};
  auto read=read_original_social_calendar_event(*events_,{"social","social_event_applied",news->news_event_id},1,now);if(!read.ok)return {false,{},read.error};const auto&e=*read.value;const auto*original=social_.events.find(e.fact_id);
  if(e.kind!="news"||!original||e.target_actor_id==original->actor_id||(original->kind!="theft"&&original->kind!="aid"))return {false,{},invalid("OriginalSocialNewsHasNoRelationEffect",e.id).error};
  return {true,{&e.target_actor_id,&original->actor_id,&e.original_application->occurred_at,original->kind=="theft"?OriginalSocialRelationRule::heard_theft:OriginalSocialRelationRule::heard_aid},{}};
 }
 if(const auto*task=std::get_if<SocialTaskReportRelationCause>(&cause)){
  auto read=read_original_successful_task_report(common_,tasks_,task_catalog_,identity_,task->actual_terminal.source_ref,task->actual_terminal.source_schema,now);
  if(!read.ok)return {false,{},read.error};const auto&r=read.value;
  if(r.task->id!=task->task_id||r.completed->settlement_id_after!=task->settlement_id)return {false,{},invalid("OriginalRelationTaskIdentityMismatch",task->task_id).error};
  return {true,{&r.started->accepted_action.context.provider_actor_id,&r.started->actor_id,&r.completed->finished_at,OriginalSocialRelationRule::successful_task_report},{}};
 }
 if(const auto*work=std::get_if<SocialGrowthServiceRelationCause>(&cause)){
  if(!work_services_||!bound_to_work_services(*work_services_))return {false,{},required("OriginalGrowthServiceCompletionSourceRequired").error};
  auto read=read_original_completed_work_service(*work_services_,work->actual_terminal.source_ref,work->actual_terminal.source_schema,now);if(!read.ok)return {false,{},read.error};const auto&r=read.value;
  if(r.activity->view.action_id!=work->action_id)return {false,{},invalid("OriginalRelationWorkServiceIdentityMismatch",work->action_id).error};
  return {true,{&r.activity->view.provider_actor_id,&r.activity->view.actor_id,&r.publication->occurred_at,OriginalSocialRelationRule::completed_work_service},{}};
 }
 return {false,{},invalid("InvalidOriginalRelationCause").error};
}
Status NativeOriginalSocialRelationSourceRead::validate_original_application_before_result(const OriginalSocialRelationCause&cause,const FactRecord&result,const WorldMoment&now)const{
 auto source=read_original_application(cause,now);if(!source.ok)return {false,{},source.error};
 auto business=original_social_relation_business_source(cause);if(!business.ok)return {false,{},business.error};
 return before_result(common_,business.value,result);
}
Status NativeOriginalSocialRelationSourceRead::validate_original_application_coverage(const WorldMoment&now)const{
 if(!social_.events.empty()||events_){
  if(!events_||!bound_to_events(*events_))return required("OriginalSocialCalendarSourceRequired");
  auto valid=validate_original_social_event_coverage(*events_,now);if(!valid.ok)return valid;
  for(const auto&row:social_.events.rows()){
   const auto&e=row.second;
   auto one=[&](const OriginalSocialRelationCause&cause)->Status{auto actual=read_original_application(cause,now);if(!actual.ok)return {false,{},actual.error};return count(social_,cause)==1?common_success():invalid("OriginalSocialEventRelationCoverageMismatch",e.id);};
   valid=original_event_causes(social_,e,one);
   if(!valid.ok)return valid;
  }
 }else for(const auto&head:common_.state().facts.rows())if(head.second.domain=="social"&&head.second.kind=="social_event_applied")return invalid("OrphanOriginalSocialEventHeader",head.first);
 for(const auto&row:growth_.life_activities.rows())if(row.second.view.kind==LifeActivityKind::service&&row.second.view.activity=="work"&&row.second.view.phase==LifeActivityPhase::completed){
  if(!work_services_||!bound_to_work_services(*work_services_))return required("OriginalGrowthServiceCompletionSourceRequired");
  const GrowthTerminalPublication*publication=nullptr;for(const auto&original:growth_.terminal_publications.rows())if(original.second.action_id==row.first){if(publication)return invalid("DuplicateOriginalWorkServicePublication",row.first);publication=&original.second;}
  if(!publication)return invalid("OriginalWorkServicePublicationRequired",row.first);
  const SocialGrowthServiceRelationCause cause{row.first,{{"growth","growth_result_terminal",publication->publication_id},1}};
  auto actual=read_original_application(cause,now);if(!actual.ok)return {false,{},actual.error};
  if(count(social_,cause)!=1)return invalid("OriginalWorkServiceRelationCoverageMismatch",row.first);
 }
 if(!tasks_.exact_execution){for(const auto&row:tasks_.tasks.rows())if(row.second.reported_tick&&!row.second.contract_id)return required("OriginalTaskReportExactSourceRequired");return common_success();}
 for(const auto&row:tasks_.exact_execution->terminals.rows()){
  const auto&end=row.second;const auto*start=tasks_.exact_execution->starts.find(end.start_source_id);
  if(!start)return invalid("OriginalRelationTaskStartRequired",row.first);
  const auto*task=tasks_.tasks.find(start->accepted_action.task_id);
  if(start->action_kind!="report_offer"||!task||task->contract_id||task->record_kind!=TaskRecordKind::legacy_quantity||task->exact_quantity||!end.effect.success||end.task_status_after!="success")continue;
  const SourceRef terminal{"tasks","task_action_finished",row.first};auto actual=read_original_successful_task_report(common_,tasks_,task_catalog_,identity_,terminal,1,now);if(!actual.ok)return {false,{},actual.error};
  if(count(social_,SocialTaskReportRelationCause{task->id,end.settlement_id_after,{terminal,1}})!=1)return invalid("OriginalTaskRelationCoverageMismatch",row.first);
 }return common_success();
}
Status NativeOriginalSocialCalendarRelationWrite::validate_period_candidate(const ExactPeriodContext&p,const WorldMoment&now)const{
 if(!bound_to(common_,social_))return invalid("OriginalSocialCalendarSameCandidateRequired");
 auto valid=validate_original_social_event_period(events_,p,now);if(!valid.ok)return valid;
 return validate_original_social_relations({common_,social_,catalog_,sources_,now});
}
Status NativeOriginalSocialCalendarRelationWrite::apply_original_calendar_event(const StableId&id,const ExactPeriodContext&p,const WorldMoment&now){
 if(!bound_to(common_,social_))return invalid("OriginalSocialCalendarSameCandidateRequired",id);
 auto valid=bind_original_social_calendar_event_in_candidate(common_,social_,events_,id,p,now);if(!valid.ok)return valid;
 const auto&e=*social_.events.find(id);
 auto apply=[&](const OriginalSocialRelationCause&cause)->Status{auto result=apply_original_social_relation_in_candidate(common_,social_,{common_,social_,catalog_,sources_,now},cause,now);return {result.ok,{},result.error};};
 valid=original_event_causes(social_,e,apply);
 return valid;
}
Status NativeOriginalTaskReportRelationWrite::apply_original_task_report(const SourceRef&terminal,Integer schema,const WorldMoment&at){
 if(!bound_to(common_,social_,tasks_))return invalid("OriginalTaskRelationSameCandidateRequired");
 auto original=read_original_successful_task_report(common_,tasks_,tasks_catalog_,catalog_.identity,terminal,schema,at);if(!original.ok)return {false,{},original.error};
 const auto&read=original.value;const OriginalSocialRelationCause cause=SocialTaskReportRelationCause{read.task->id,read.completed->settlement_id_after,{terminal,schema}};
 const OriginalSocialRelationContext context{common_,social_,catalog_,sources_,at};auto applied=apply_original_social_relation_in_candidate(common_,social_,context,cause,at);return {applied.ok,{},applied.error};
}
Status NativeOriginalGrowthServiceRelationWrite::validate_completion_candidate(const StableId&id,const WorldMoment&at)const{
 if(!bound_to(common_,social_,work_services_.growth))return invalid("OriginalWorkServiceRelationSameCandidateRequired",id);
 auto source=validate_original_work_service_completion_candidate(work_services_,id,at);if(!source.ok)return source;
 return validate_original_social_relations({common_,social_,catalog_,sources_,at});
}
Status NativeOriginalGrowthServiceRelationWrite::apply_completed_work_service(const SourceRef&boundary,Integer schema,const WorldMoment&at){
 if(!bound_to(common_,social_,work_services_.growth))return invalid("OriginalWorkServiceRelationSameCandidateRequired",boundary.id);
 auto read=read_original_work_service_at_terminal_boundary(work_services_,boundary,schema,at);if(!read.ok)return {false,{},read.error};const auto&r=read.value;
 const OriginalSocialRelationCause cause=SocialGrowthServiceRelationCause{r.activity->view.action_id,{{"growth","growth_result_terminal",r.publication->publication_id},1}};
 auto applied=apply_original_social_relation_in_candidate(common_,social_,{common_,social_,catalog_,sources_,at},cause,at);return {applied.ok,{},applied.error};
}
}
