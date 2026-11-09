#include "care_separation.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
void check(Status status){if(!status.ok)throw Failure{std::move(status.error)};}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
const FactRecord& header(const CommonStore& common,const SourceRef& ref){const auto* head=common.state().facts.find(ref.id);const auto& order=common.state().facts.order();if(!head||head->id!=ref.id||head->kind!=ref.kind||head->domain!=ref.domain||std::count(order.begin(),order.end(),ref.id)!=1)fail("CareSeparationActualSourceRequired",ref.id);return *head;}
std::size_t index(const CommonStore& common,const SourceRef& ref){header(common,ref);const auto& order=common.state().facts.order();return static_cast<std::size_t>(std::find(order.begin(),order.end(),ref.id)-order.begin());}
void terms(const CareBinding& binding,const AcceptedCaregiver& accepted){
 check(validate_care_binding_shape(binding));check(validate_care_arrangement_terms_shape(accepted.terms));
 if(binding.accepted_ref!=accepted.accepted_id||binding.terms_ref!=accepted.accepted_id||binding.child_actor_id!=accepted.subject_actor_id||binding.carer_actor_id!=accepted.provider.carer_actor_id)fail("CareSeparationAcceptedPeopleMismatch",binding.binding_id);
 if(!accepted.terms.allowed_dependency_refs.empty())fail("CareSeparationOriginalDependencyReadRequired",binding.binding_id,ErrorKind::unsupported_capability);
}
bool at_accepted_place(const ActorSpatialHeadSourceReadContext& x,const AcceptedCaregiver& accepted,const ActorSpatialLocationView& child,const ActorSpatialLocationView& carer){
 const auto* a=std::get_if<SettlementLocation>(&child.location_snapshot);const auto* b=std::get_if<SettlementLocation>(&carer.location_snapshot);
 if(!a||!b)return false;
 const auto* ca=x.common_catalog.places.find(a->place_id);const auto* cb=x.common_catalog.places.find(b->place_id);
 if(!ca||!cb||ca->settlement_id!=a->settlement_id||cb->settlement_id!=b->settlement_id)fail("CareSeparationOriginalPlaceRequired",accepted.accepted_id);
 if(a->place_id!=b->place_id||a->settlement_id!=b->settlement_id||std::find(accepted.terms.allowed_place_ids.begin(),accepted.terms.allowed_place_ids.end(),a->place_id)==accepted.terms.allowed_place_ids.end())return false;
 const auto* aa=ordinary_actor_movement_anchor(child);const auto* ab=ordinary_actor_movement_anchor(carer);
 if(!aa||!ab)fail("CareSeparationOriginalTacticalHistoryRequired",accepted.accepted_id,ErrorKind::unsupported_capability);
 return take(same_actual_movement_position(*aa,*ab,x.budget));
}
ActorSpatialLocationView point(const ActorSpatialHeadSourceReadContext& x,const StableId& actor,const WorldMoment& at,std::size_t count,const MilitarySpatialHandoffState* military){
 const auto& order=x.common.state().facts.order();if(count>order.size()||count>static_cast<std::uint64_t>(std::numeric_limits<Integer>::max()))fail("CareSeparationActualCutOverflow",actor);
 const FactRecord* last=nullptr;if(count){last=x.common.state().facts.find(order[count-1]);if(!last||last->id!=order[count-1])fail("CareSeparationActualCutRequired",actor);}auto memory=CheckpointMemoryCharge(0);
 ActorSpatialActualFactPrefix prefix;prefix.common_fact_count=static_cast<Integer>(count);if(last)prefix.last_common_fact=SourceRef{last->domain,last->kind,last->id};
 return take(read_actor_spatial_location_at_actual_fact_prefix(x.common,x.common_catalog,x.movement,x.carriers,x.registrations,x.foundation,x.actors,x.spatial,actor,at,prefix,military,x.budget,x.family_catalog,x.births,x.military_actions));
}
bool ordinary_change(const SourceRef& ref){
 // Initial insertion cannot separate an already completed binding. The current
 // dependent result branches are birth and same-place Care adoption; future
 // dependency movement must be handled by its own original read branch.
 return is_actor_spatial_head_source_kind(ref)&&ref.domain!="actor_foundation"&&ref.domain!="family"&&ref.kind!="travel_dependent_binding_change";
}
}
bool NativeCareActualSeparationRead::bound_to(const CommonStore& common)const noexcept{return &common==&spatial_.common;}
Outcome<std::optional<CareActualSeparationView>> NativeCareActualSeparationRead::read_actual_separation(const CareBinding& binding,const AcceptedCaregiver& accepted,const MomentOwnerSource& source)const{try{
 const auto& x=spatial_;const auto& head=header(x.common,source.source_ref);
 if(!ordinary_change(source.source_ref))fail("CareSeparationActualOrdinaryTransitionRequired",head.id,ErrorKind::unsupported_capability);
 auto child=take(read_actor_spatial_location_transition_at_source(x,binding.child_actor_id,source,military_));
 auto carer=take(read_actor_spatial_location_transition_at_source(x,binding.carer_actor_id,source,military_));
 if(!child&&!carer)return {true,std::nullopt,{}};
 terms(binding,accepted);
 if(child&&carer&&!same(child->occurred_at,carer->occurred_at))fail("CareSeparationOriginalMomentMismatch",head.id);
 const auto& at=child?child->occurred_at:carer->occurred_at;
 if((!child||!child->actual_position_changed)&&(!carer||!carer->actual_position_changed))return {true,std::nullopt,{}};
 const auto cut=index(x.common,source.source_ref);
 std::optional<ActorSpatialLocationView> other;
 if(!child)other=point(x,binding.child_actor_id,at,cut,military_);
 else if(!carer)other=point(x,binding.carer_actor_id,at,cut,military_);
 const auto& cb=child?child->before:*other;const auto& ca=child?child->after:*other;
 const auto& pb=carer?carer->before:*other;const auto& pa=carer?carer->after:*other;
 if(!at_accepted_place(x,accepted,cb,pb))fail("CareSeparationEarlierCoverageLossMissing",binding.binding_id);
 if(at_accepted_place(x,accepted,ca,pa))return {true,std::nullopt,{}};
 return {true,CareActualSeparationView{&head,source.source_schema,at},{}};
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Outcome<std::optional<CareActualSeparationView>> NativeCareActualSeparationRead::read_first_actual_separation(const CareBinding& binding,const AcceptedCaregiver& accepted,const SourceRef& completed,const MomentSpan& span,const std::optional<SourceRef>& end)const{try{
 terms(binding,accepted);const auto& common=spatial_.common;const auto start=index(common,completed);const auto stop=end?index(common,*end):common.state().facts.order().size();if(start>=stop)fail("CareSeparationActualCoverageCutMismatch",binding.binding_id);
 take(make_moment_span(span.from,span.to,spatial_.budget));const auto now=take(spatial_.spatial.current_moment());if(take(compare_world_moments(span.to,now,spatial_.budget))>0||(!end&&!same(span.to,now)))fail("CareSeparationActualCoverageClockMismatch",binding.binding_id);
 std::optional<CareActualSeparationView> first;
 const auto& order=common.state().facts.order();for(std::size_t n=start+1;n<stop;++n){const auto* head=common.state().facts.find(order[n]);if(!head||head->id!=order[n])fail("CareSeparationActualSourceRequired",order[n]);
  auto memory=CheckpointMemoryCharge(0);const SourceRef ref{head->domain,head->kind,head->id};if(!ordinary_change(ref))continue;
  const auto schema=take(actor_spatial_history_source_schema(ref));auto actual=take(read_actual_separation(binding,accepted,{ref,schema}));if(actual){first=std::move(actual);break;}
 }
 const auto& until=first?first->occurred_at:span.to;
 if(take(compare_world_moments(span.from,until,spatial_.budget))>0||take(compare_world_moments(until,span.to,spatial_.budget))>0)fail("CareSeparationOriginalCoverageMomentMismatch",binding.binding_id);
 // No immutable head change is not proof of absence of motion. Before the
 // first actual separation (or the genuine end), both retained full spans
 // must prove the accepted ordinary place. Positive travel is not silently
 // recovered from a later arrival or from today's restored same location.
 const auto child=point(spatial_,binding.child_actor_id,span.from,start+1,military_);
 const auto carer=point(spatial_,binding.carer_actor_id,span.from,start+1,military_);
 if(!at_accepted_place(spatial_,accepted,child,carer))fail("CareSeparationOriginalStartPlaceMismatch",binding.binding_id);
 if(take(compare_world_moments(span.from,until,spatial_.budget))<0){
  for(const auto* actual:{&child,&carer}){
   if(actual->actual_location_sources.size()!=1)fail("CareSeparationOriginalStartHeadRequired",actual->actor_id);
   const auto full=take(read_actor_stationary_location_span(spatial_.common,spatial_.common_catalog,spatial_.movement,spatial_.carriers,spatial_.registrations,spatial_.foundation,spatial_.actors,spatial_.spatial,actual->actor_id,actual->actual_location_sources.front().source,{span.from,until},military_,spatial_.budget,spatial_.family_catalog,spatial_.births,spatial_.military_actions));
   const auto* anchor=ordinary_actor_movement_anchor(*actual);if(!anchor||!take(same_actual_movement_position(*anchor,full.actual_anchor,spatial_.budget)))fail("CareSeparationOriginalSpanPlaceMismatch",actual->actor_id);
  }
 }
 return {true,std::move(first),{}};
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
}
