#include "naval_repair_participation.hpp"
#include "naval_repair_exact_detail.hpp"
#include <limits>
namespace hanmo::resident {using namespace naval_repair_exact_detail;namespace {
void bound(const ExactNavalRepairParticipationContext& x,const NewNavalRepairTimeSource2& f){const auto& h=x.history;const auto& s=x.spatial;
 if(&h.common!=&x.bodies.common||&h.common!=&s.common||&x.bodies.state!=&s.foundation||&x.bodies.catalog!=&s.actors||!x.social.bound_to(h.common)||h.access.common_catalog!=&s.common_catalog||x.bodies.military!=&h.military||x.bodies.military_exact!=&h.military_exact||x.bodies.elapsed!=&h.elapsed)fail("CrossCandidateExactNavalRepairParticipation",f.timing.action_ref);
 if(h.actions.naval_repair_time_sources.find(f.timing.action_ref)!=&f)fail("CrossCandidateExactNavalRepairSource",f.timing.action_ref);
 validate_source_shape(h.common,h.naval,h.actions,{h.military_catalog,h.access,h.naval_catalog,h.scale,h.clock,h.budget},f);
 if(!same_moment(x.bodies.actual_now,take(h.clock.read_current_moment()),h.budget))fail("ExactNavalRepairFreshBodyReadRequired",f.timing.action_ref);
}
ActorSpatialActualFactPrefix prefix_before(const CommonStore& c,const SourceRef& actual){header(c,actual);const auto& order=c.state().facts.order();const auto at=std::find(order.begin(),order.end(),actual.id);if(at==order.end())fail("ExactNavalRepairOriginalHeaderRequired",actual.id);const auto size=std::size_t(at-order.begin());if(size>std::size_t(std::numeric_limits<Integer>::max()))fail("ExactNavalRepairSourcePrefixOverflow",actual.id);ActorSpatialActualFactPrefix prefix;prefix.common_fact_count=Integer(size);if(size){const auto* last=c.state().facts.find(order[size-1]);if(!last)fail("ExactNavalRepairOriginalHeaderRequired",order[size-1]);prefix.last_common_fact=SourceRef{last->domain,last->kind,last->id};}return prefix;}
ActorSpatialLocationView position_before(const ExactNavalRepairParticipationContext& x,const StableId& actor,const SourceRef& ref,const WorldMoment& at){const auto& s=x.spatial;const auto prefix=prefix_before(s.common,ref);return take(read_actor_spatial_location_at_actual_fact_prefix(s.common,s.common_catalog,s.movement,s.carriers,s.registrations,s.foundation,s.actors,s.spatial,actor,at,prefix,x.military_handoffs,s.budget,s.family_catalog,s.births,s.military_actions));}
bool needs_available(const NavalRepairTerms& t,const StableId& actor){return actor==t.actor_id||actor==t.captain_actor_id||std::find(t.crew_ids.begin(),t.crew_ids.end(),actor)!=t.crew_ids.end();}
void at_source(const ExactNavalRepairParticipationContext& x,const NewNavalRepairTimeSource2& f,const SourceRef& ref,const WorldMoment& at){
 const auto& t=f.original_terms;const MovementAnchor accepted=OriginalMovementAnchor{t.anchor,f.timing.start_fact_ref};
 for(const auto& actor:t.participants){
  const auto state=take(read_character_availability_before_fact_v2(x.bodies,actor,ref,at,x.history.clock));if(!state.present||!state.alive||(needs_available(t,actor)&&!state.available))fail("NavalRepairPersonUnavailable",actor);
  const auto social=take(x.social.read_actor_social_occupancy_before_source(actor,ref,at));if(social.original_sources.empty()||social.value.party_id)fail("NavalRepairPersonOccupied",actor);
  const auto point=position_before(x,actor,ref,at);const auto* anchor=take(require_actor_ordinary_movement_anchor(point));if(point.actual_location_sources.size()!=1||!take(same_actual_movement_position(*anchor,accepted,x.history.budget)))fail("NavalRepairPersonNotAtOriginalAnchor",actor);
 }
}
class NoPartyDuringRepair final:public ActorSocialOccupancyChangeVisitor {
 const MomentSpan& span_;ExactArithmeticBudget budget_;
public:NoPartyDuringRepair(const MomentSpan& span,ExactArithmeticBudget budget):span_(span),budget_(budget){}
 Status changed(const ActorSocialOccupancyTransition& source)override{return status([&]{if(take(compare_world_moments(source.occurred_at,span_.to,budget_))<0&&source.after.party_id)fail("NavalRepairPartyChangedDuringService",source.id);});}
};
}
Status validate_exact_naval_repair_people_at_acceptance(const ExactNavalRepairParticipationContext& x,const NewNavalRepairTimeSource2& f){return status([&]{bound(x,f);at_source(x,f,f.timing.start_fact_ref,f.timing.started_at);});}
Status validate_exact_naval_repair_people_service_span(const ExactNavalRepairParticipationContext& x,const NewNavalRepairTimeSource2& f,const MomentSpan& span){return status([&]{
 bound(x,f);const auto& h=x.history;const auto& s=x.spatial;const auto now=take(h.clock.read_current_moment());
 if(take(compare_world_moments(span.from,span.to,h.budget))>=0||take(compare_world_moments(span.from,f.timing.started_at,h.budget))<0||take(compare_world_moments(span.to,now,h.budget))>0||(f.timing.completion_or_interruption_at&&take(compare_world_moments(span.to,*f.timing.completion_or_interruption_at,h.budget))>0))fail("ExactNavalRepairActualSpanRequired",f.timing.action_ref);
 const MovementAnchor expected=OriginalMovementAnchor{f.original_terms.anchor,f.timing.start_fact_ref};
 for(const auto& actor:f.original_terms.participants){
  const auto body=take(read_character_availability_span_after_fact_v2(x.bodies,actor,f.timing.start_fact_ref,f.timing.started_at,span,h.clock));if(!body.alive_for_full_span||(needs_available(f.original_terms,actor)&&!body.available_for_full_span))fail("NavalRepairPersonUnavailableDuringService",actor);
  const auto start=take(x.social.read_actor_social_occupancy_before_source(actor,f.timing.start_fact_ref,f.timing.started_at));if(start.original_sources.size()!=1||start.value.party_id)fail("NavalRepairPersonOccupied",actor);
  const auto end=f.timing.result_ref?take(x.social.read_actor_social_occupancy_before_source(actor,*f.timing.result_ref,*f.timing.completion_or_interruption_at)):take(x.social.read_actual_actor_social_occupancy(actor,now));if(end.original_sources.size()!=1)fail("NavalRepairPartyHistoryRequired",actor);
  NoPartyDuringRepair changes(span,h.budget);take(x.social.visit_actor_social_occupancy_changes(actor,f.timing.started_at,start.original_sources.front(),f.timing.completion_or_interruption_at.value_or(now),end.original_sources.front(),now,changes));
  const auto accepted=position_before(x,actor,f.timing.start_fact_ref,f.timing.started_at);if(accepted.actual_location_sources.size()!=1||!take(same_actual_movement_position(*take(require_actor_ordinary_movement_anchor(accepted)),expected,h.budget)))fail("NavalRepairPersonNotAtOriginalAnchor",actor);
  const auto stationary=take(read_actor_stationary_location_span(s.common,s.common_catalog,s.movement,s.carriers,s.registrations,s.foundation,s.actors,s.spatial,actor,accepted.actual_location_sources.front().source,span,x.military_handoffs,s.budget,s.family_catalog,s.births,s.military_actions));
  if(!take(same_actual_movement_position(stationary.actual_anchor,expected,h.budget)))fail("NavalRepairPersonMovedDuringService",actor);
 }
});}
Status validate_exact_naval_repair_people_at_success(const ExactNavalRepairParticipationContext& x,const NewNavalRepairTimeSource2& f){return status([&]{bound(x,f);if(!f.original_result||!f.original_result->success||!f.timing.result_ref||!f.timing.completion_or_interruption_at)fail("ExactNavalRepairSuccessfulResultRequired",f.timing.action_ref);at_source(x,f,*f.timing.result_ref,*f.timing.completion_or_interruption_at);});}
Status validate_exact_naval_repair_people_at_current_completion(const ExactNavalRepairParticipationContext& x,const NewNavalRepairTimeSource2& f){return status([&]{
 bound(x,f);const auto& h=x.history;const auto& s=x.spatial;const auto now=take(h.clock.read_current_moment());if(f.original_result)fail("ExactNavalRepairActiveCompletionRequired",f.timing.action_ref);
 const auto& order=h.common.state().facts.order();if(order.size()>std::size_t(std::numeric_limits<Integer>::max()))fail("ExactNavalRepairSourcePrefixOverflow",f.timing.action_ref);ActorSpatialActualFactPrefix prefix;prefix.common_fact_count=Integer(order.size());if(!order.empty()){const auto* head=h.common.state().facts.find(order.back());if(!head)fail("ExactNavalRepairOriginalHeaderRequired",order.back());prefix.last_common_fact=SourceRef{head->domain,head->kind,head->id};}
 const MovementAnchor expected=OriginalMovementAnchor{f.original_terms.anchor,f.timing.start_fact_ref};
 for(const auto& actor:f.original_terms.participants){const auto* current=h.common.actor(actor);if(!current||!current->alive||(needs_available(f.original_terms,actor)&&!current->available)||current->action_id!=f.timing.action_ref)fail("NavalRepairPersonUnavailable",actor);
  const auto social=take(x.social.read_actual_actor_social_occupancy(actor,now));if(social.original_sources.empty()||social.value.party_id)fail("NavalRepairPersonOccupied",actor);
  const auto point=take(read_actor_spatial_location_at_actual_fact_prefix(s.common,s.common_catalog,s.movement,s.carriers,s.registrations,s.foundation,s.actors,s.spatial,actor,now,prefix,x.military_handoffs,s.budget,s.family_catalog,s.births,s.military_actions));if(point.actual_location_sources.size()!=1||!take(same_actual_movement_position(*take(require_actor_ordinary_movement_anchor(point)),expected,h.budget)))fail("NavalRepairPersonNotAtOriginalAnchor",actor);
 }
});}

}
