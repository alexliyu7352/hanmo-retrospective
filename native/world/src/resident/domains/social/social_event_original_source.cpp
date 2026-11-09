#include "social_event_original_source.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include <charconv>
#include <limits>
#include <string_view>
namespace hanmo::resident {namespace {
Status bad(const char*code,const StableId&id={}){return common_failure(code,id,ErrorKind::internal);}
Outcome<Integer> serial(const CommonStore&common,const StableId&id){
 constexpr std::string_view prefix="social:";std::string_view text{id};
 if(text.size()<=prefix.size()||text.substr(0,prefix.size())!=prefix)return {false,{},bad("InvalidOriginalSocialEventId",id).error};
 text.remove_prefix(prefix.size());Integer n=0;const auto read=std::from_chars(text.data(),text.data()+text.size(),n);
 if(text.front()=='0'||read.ec!=std::errc{}||read.ptr!=text.data()+text.size()||n<1||n>=common.state().next_instance_id)return {false,{},bad("InvalidOriginalSocialEventId",id).error};
 return {true,n,{}};
}
Status equal(const WorldMoment&a,const WorldMoment&b,const StableId&id){auto c=compare_world_moments(a,b);if(!c.ok)return {false,{},c.error};return c.value?bad("OriginalSocialEventMomentMismatch",id):common_success();}
Status before(const CommonStore&common,const StableId&first,const StableId&second){bool found=false;for(const auto&row:common.state().facts.rows()){if(row.first==second)return found?common_success():bad("OriginalSocialNewsSourceNotBeforeEvent",second);if(row.first==first)found=true;}return bad("OriginalSocialEventHeaderMissing",second);}
const SocialMemory* memory(const SocialState&s,const StableId&actor,const StableId&fact){const auto*m=s.memories.find(actor);return m?m->find(fact):nullptr;}
}
Status validate_social_event_original_application_shape(const SocialEventOriginalApplication&v){
 const auto&r=v.original_period_opening;if(r.source_schema!=1||r.source_ref.domain!="social"||r.source_ref.kind!="social_period_open"||r.source_ref.id.empty())return bad("OriginalSocialEventOpeningSourceMismatch");
 auto shape=compare_world_moments(v.occurred_at,v.occurred_at);if(!shape.ok)return {false,{},shape.error};auto zero=compare_fractions(v.occurred_at.sub_half_day,CanonicalFraction{});if(!zero.ok)return {false,{},zero.error};return zero.value?bad("OriginalSocialEventRequiresIntegerBoundary"):common_success();
}
Status validate_original_social_event_period(const OriginalSocialEventSources&c,const ExactPeriodContext&p,const WorldMoment&now){
 if(c.social.original_relation_version!=2)return common_failure("OriginalSocialEventSelectionRequired",{},ErrorKind::unsupported_capability);
 auto read=read_social_period_opening_source(c.periods,{{"social","social_period_open",p.period_id},1},c.identity);if(!read.ok)return {false,{},read.error};
 auto same=equal(read.value->opened_at,p.opening_at,p.period_id);if(!same.ok)return same;if(read.value->through_tick!=p.through_tick)return bad("OriginalSocialEventPeriodMismatch",p.period_id);
 return equal(now,{read.value->through_tick,{}},p.period_id);
}
Outcome<const SocialEvent*> read_original_social_calendar_event(const OriginalSocialEventSources&c,const SourceRef&source,Integer schema,const WorldMoment&now){
 if(c.social.original_relation_version!=2||schema!=1||source.domain!="social"||source.kind!="social_event_applied")return {false,{},bad("OriginalSocialEventSourceMismatch",source.id).error};
 const auto*e=c.social.events.find(source.id);if(!e||e->id!=source.id||!e->original_application)return {false,{},bad("OriginalSocialEventApplicationRequired",source.id).error};
 auto n=serial(c.common,e->id);if(!n.ok)return {false,{},n.error};const auto&a=*e->original_application;auto shape=validate_social_event_original_application_shape(a);if(!shape.ok)return {false,{},shape.error};
 auto opening=read_social_period_opening_source(c.periods,a.original_period_opening,c.identity);if(!opening.ok)return {false,{},opening.error};
 if(e->tick!=opening.value->through_tick)return {false,{},bad("OriginalSocialEventCalendarMismatch",e->id).error};shape=equal(a.occurred_at,{opening.value->through_tick,{}},e->id);if(!shape.ok)return {false,{},shape.error};auto future=compare_world_moments(a.occurred_at,now);if(!future.ok)return {false,{},future.error};if(future.value>0)return {false,{},bad("FutureOriginalSocialEvent",e->id).error};
 const auto*h=c.common.state().facts.find(e->id);if(!h||h->id!=source.id||h->domain!=source.domain||h->kind!=source.kind)return {false,{},bad("OriginalSocialEventHeaderMissing",e->id).error};
 if(e->actor_id==e->target_actor_id||!c.common.actor(e->actor_id)||!c.common.actor(e->target_actor_id))return {false,{},bad("OriginalSocialEventParticipantsMismatch",e->id).error};
 if(e->kind=="news"){
  if(e->fact_id.empty()||!e->item_id.empty()||e->quantity||!e->reason.empty())return {false,{},bad("OriginalSocialNewsShapeMismatch",e->id).error};
  const auto*sender=memory(c.social,e->actor_id,e->fact_id);const auto*receiver=memory(c.social,e->target_actor_id,e->fact_id);
  if(!sender||sender->learned_tick>=e->tick||!receiver||receiver->learned_tick!=e->tick||receiver->source_actor_id!=e->actor_id)return {false,{},bad("OriginalSocialNewsKnowledgeMismatch",e->id).error};
  if(const auto*original=c.social.events.find(e->fact_id)){
   if(original->kind=="news")return {false,{},bad("OriginalSocialNewsCannotSourceNews",e->id).error};
   auto previous=read_original_social_calendar_event(c,{"social","social_event_applied",original->id},1,now);if(!previous.ok)return previous;
   shape=before(c.common,original->id,e->id);if(!shape.ok)return {false,{},shape.error};
  }
 }else{
  if((e->kind!="aid"&&e->kind!="theft"&&e->kind!="refusal")||e->item_id.empty()||!e->fact_id.empty()||e->quantity!=(e->kind=="refusal"?0:1)||(e->kind=="refusal"?(e->reason!="selfish"&&e->reason!="distrust"):e->reason!="shortage"))return {false,{},bad("OriginalSocialExchangeShapeMismatch",e->id).error};
  for(const auto*actor:{&e->actor_id,&e->target_actor_id}){const auto*m=memory(c.social,*actor,e->id);if(!m||m->source_actor_id!=*actor||m->learned_tick!=e->tick)return {false,{},bad("OriginalSocialEventWitnessMismatch",e->id).error};}
 }
 return {true,e,{}};
}
Status validate_original_social_event_coverage(const OriginalSocialEventSources&c,const WorldMoment&now){
 std::size_t index=0;Integer last=0;const WorldMoment*previous=nullptr;
 for(const auto&header:c.common.state().facts.rows())if(header.second.domain=="social"&&header.second.kind=="social_event_applied"){
  if(index>=c.social.events.size()||c.social.events.rows()[index].first!=header.first)return bad("OriginalSocialEventHeaderOrderMismatch",header.first);
  auto read=read_original_social_calendar_event(c,{"social","social_event_applied",header.first},1,now);if(!read.ok)return {false,{},read.error};auto n=serial(c.common,header.first);if(!n.ok)return {false,{},n.error};if(n.value<=last)return bad("OriginalSocialEventAllocationOrderMismatch",header.first);
  if(previous){auto order=compare_world_moments(*previous,read.value->original_application->occurred_at);if(!order.ok)return {false,{},order.error};if(order.value>0)return bad("OriginalSocialEventTimeOrderMismatch",header.first);}previous=&read.value->original_application->occurred_at;last=n.value;++index;
 }
 return index==c.social.events.size()?common_success():bad("OriginalSocialEventHeaderCoverageMismatch");
}
Status bind_original_social_calendar_event_in_candidate(CommonStore&common,SocialState&social,const OriginalSocialEventSources&c,const StableId&id,const ExactPeriodContext&period,const WorldMoment&now){
 if(&c.common!=&common||&c.social!=&social)return bad("OriginalSocialEventSameCandidateRequired",id);
 auto valid=validate_original_social_event_period(c,period,now);if(!valid.ok)return valid;auto*e=social.events.find(id);auto n=serial(common,id);if(!n.ok)return {false,{},n.error};
 if(!e||e->id!=id||e->tick!=period.through_tick||e->original_application||social.events.empty()||social.events.rows().back().first!=id||common.state().facts.find(id)||n.value==std::numeric_limits<Integer>::max()||n.value+1!=common.state().next_instance_id)return bad("OriginalSocialEventFreshInsertionRequired",id);
 e->original_application=SocialEventOriginalApplication{{{"social","social_period_open",period.period_id},1},now};
 valid=common.append_fact({id,"social_event_applied","social"});if(!valid.ok)return valid;
 auto read=read_original_social_calendar_event(c,{"social","social_event_applied",id},1,now);return {read.ok,{},read.error};
}
}
