#include "care_dependent_placement.hpp"
#include "movement_exact_detail.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {using namespace movement_detail;namespace {
constexpr const char* result_kind="travel_dependent_binding_change";
bool same(const MomentOwnerSource& a,const MomentOwnerSource& b){return a.source_schema==b.source_schema&&same_source(a.source_ref,b.source_ref);}
void bound(const CareDependentPlacementContext& x){
 if(x.selected_version!=2||&x.common!=&x.handovers.acceptance.goals.common)fail("CareDependentSameCandidateRequired");
}
std::size_t position(const CommonStore& c,const SourceRef& ref){
 const auto* head=c.state().facts.find(ref.id);const auto& ids=c.state().facts.order();const auto it=std::find(ids.begin(),ids.end(),ref.id);
 if(!head||head->id!=ref.id||head->domain!=ref.domain||head->kind!=ref.kind||it==ids.end()||std::count(ids.begin(),ids.end(),ref.id)!=1)fail("CareDependentActualSourceRequired",ref.id);return std::size_t(it-ids.begin());
}
ActorSpatialActualFactPrefix prefix(const CommonStore& c,std::size_t count){
 const auto& ids=c.state().facts.order();if(count>ids.size()||count>std::size_t(std::numeric_limits<Integer>::max()))fail("CareDependentActualCutOverflow");ActorSpatialActualFactPrefix out;out.common_fact_count=Integer(count);
 if(count){const auto* last=c.state().facts.find(ids[count-1]);if(!last)fail("CareDependentActualCutRequired");out.last_common_fact=SourceRef{last->domain,last->kind,last->id};}return out;
}
ActorSpatialLocationView point(const CareDependentPlacementContext& x,const StableId& actor,const WorldMoment& at,const ActorSpatialActualFactPrefix& cut){
 return take(read_actor_spatial_location_at_actual_fact_prefix(x.common,x.catalog,x.movement,x.carriers,x.registrations,x.foundation,x.actors,x.spatial,actor,at,cut,x.military_handoffs,x.budget,x.family_catalog,x.births,x.military_actions));
}
const SettlementLocation& settlement(const Location& location,const StableId& actor){const auto* result=std::get_if<SettlementLocation>(&location);if(!result)throw Failure{common_failure("CareDependentOriginalTransportPlacementRequired",actor,ErrorKind::unsupported_capability).error};return *result;}
struct Placement {Location location;MovementAnchor anchor;};
Placement actual_place(const CareDependentPlacementContext& x,const CareHandoverPlacementInput& original,const ActorSpatialActualFactPrefix& cut){
 if(!original.execution||!original.accepted||!original.last_actual_period)fail("CareDependentOriginalHandoverRequired");
 const auto& accepted=*original.accepted;const auto& child=accepted.subject_actor_id;const auto& carer=accepted.provider.carer_actor_id;
 if(child.empty()||carer.empty()||child==carer)fail("CareDependentOriginalPeopleMismatch");
 const auto actual_child=point(x,child,original.last_actual_period->provided_at,cut);const auto actual_carer=point(x,carer,original.last_actual_period->provided_at,cut);
 const auto& place=settlement(actual_child.location_snapshot,child);const auto& carer_place=settlement(actual_carer.location_snapshot,carer);
 const auto* locked=x.catalog.places.find(place.place_id);
 if(!locked||locked->settlement_id!=place.settlement_id||place.place_id!=carer_place.place_id||place.settlement_id!=carer_place.settlement_id||std::find(accepted.terms.allowed_place_ids.begin(),accepted.terms.allowed_place_ids.end(),place.place_id)==accepted.terms.allowed_place_ids.end()||!take(same_actual_movement_position(*take(require_actor_ordinary_movement_anchor(actual_child)),*take(require_actor_ordinary_movement_anchor(actual_carer)),x.budget)))fail("CareDependentActualAcceptedPlaceMismatch",accepted.accepted_id);
 return {actual_child.location_snapshot,*take(require_actor_ordinary_movement_anchor(actual_child))};
}
std::vector<StableId> before_bindings(const CareDependentPlacementContext& x,const StableId& child,const std::optional<SourceRef>& cut){
 const auto* active=take(read_active_travel_dependent_binding(x.common,x.dependents,child,cut,x.budget));if(!active)return {};
 if(!x.births||!x.births->bound_to(x.common))fail("CareDependentOriginalBirthPlacementRequired",child);
 const auto birth=take(x.births->read_birth_placement_for_child(child));
 if(!birth.transport_binding_ref||*birth.transport_binding_ref!=active->binding_id||!same(birth.actual_birth_source,active->source_birth_ref))fail("CareDependentOriginalBindingMismatch",active->binding_id);
 // A vessel/vehicle/custody attachment needs its original actual release too;
 // reaching the same displayed city does not release a passenger or custody.
 if(active->transport_ref)throw Failure{common_failure("CareDependentTransportReleaseOriginalSourceRequired",active->binding_id,ErrorKind::unsupported_capability).error};
 return {active->binding_id};
}
CareHandoverPlacementInput original_input(const CareDependentPlacementContext& x,const MomentOwnerSource& ref){bound(x);return take(read_care_handover_placement_input(x.handovers,ref.source_ref,ref.source_schema));}
const CareDependentBindingChange& read_result(const CareDependentPlacementContext& x,const MomentOwnerSource& ref){
 bound(x);if(ref.source_schema!=2||ref.source_ref.domain!="travel"||ref.source_ref.kind!=result_kind)fail("CareDependentActualResultRequired",ref.source_ref.id);
 const auto* value=x.dependents.binding_change_results.find(ref.source_ref.id);const auto* row=value?std::get_if<CareDependentBindingChange>(value):nullptr;
 if(!row||row->id!=ref.source_ref.id)fail("CareDependentActualResultRequired",ref.source_ref.id);
 const auto original=original_input(x,row->actual_handover_source);const auto cut=position(x.common,ref.source_ref);
 if(!original.accepted||!original.last_actual_period||row->child_actor_id!=original.accepted->subject_actor_id||row->carer_actor_id!=original.accepted->provider.carer_actor_id||!same_moment(row->occurred_at,original.last_actual_period->provided_at,x.budget)||position(x.common,row->actual_handover_source.source_ref)>=cut||!row->after_binding_refs.empty())fail("CareDependentOriginalHandoverMismatch",row->id);
 const auto place=actual_place(x,original,prefix(x.common,cut));
 const auto& expected=settlement(place.location,row->child_actor_id);const auto& actual=settlement(row->actual_location,row->child_actor_id);
 if(actual.settlement_id!=expected.settlement_id||actual.place_id!=expected.place_id||!take(same_actual_movement_position(row->resolved_anchor,place.anchor,x.budget))||row->before_binding_refs!=before_bindings(x,row->child_actor_id,ref.source_ref))fail("CareDependentActualPlacementMismatch",row->id);
 // Exact geometry has its original source as well as a value. A matching
 // coordinate is insufficient to replace that immutable source reference.
 const auto* saved=std::get_if<OriginalMovementAnchor>(&row->resolved_anchor);const auto* derived=std::get_if<OriginalMovementAnchor>(&place.anchor);
 if(!saved||!derived||!same_source(saved->actual_source,derived->actual_source))fail("CareDependentOriginalAnchorSourceMismatch",row->id);
 take(validate_care_handover_placement_adoption_cut(x.handovers,row->actual_handover_source.source_ref,row->actual_handover_source.source_schema,ref,row->occurred_at));
 return *row;
}
}
bool NativeCareDependentPlacementRead::bound_to(const CommonStore& common)const noexcept{return &common==&context_.common&&&common==&context_.handovers.acceptance.goals.common;}
bool NativeCareDependentPlacementRead::bound_to(const CommonStore& common,const TravelDependentState& state)const noexcept{return bound_to(common)&&&state==&context_.dependents;}
Outcome<CareDependentPlacementRead> NativeCareDependentPlacementRead::read_placement_result(const MomentOwnerSource& ref)const{return guard<CareDependentPlacementRead>([&]{return CareDependentPlacementRead{&read_result(context_,ref),ref};});}
Status NativeCareDependentPlacementRead::validate_stable_sources()const{return status([&]{
 const auto& x=context_;bound(x);take(validate_care_handover_time_source_closure(x.handovers));take(validate_travel_dependent_binding_chain(x.common,x.dependents,x.budget));std::set<StableId> used;
 for(const auto& item:x.dependents.binding_change_results.rows())if(std::holds_alternative<CareDependentBindingChange>(item.second)){
  const auto& row=read_result(x,{{"travel",result_kind,item.first},2});const auto input=original_input(x,row.actual_handover_source);
  if(!input.actual_completion||!used.insert(row.actual_handover_source.source_ref.id).second)fail("CareDependentCompletedSourceCoverage",item.first);
 }
 for(const auto& item:x.handovers.executions.rows())if(item.second.completion_event_ref){
  const auto& execution=item.second;if(!used.count(execution.handover_id)||execution.actual_placement_result_refs.size()!=1)fail("CareDependentCompletedResultCoverage",item.first);
  const auto& row=read_result(x,execution.actual_placement_result_refs.front());if(row.actual_handover_source.source_ref.id!=execution.handover_id)fail("CareDependentCompletedResultCoverage",item.first);
 }
 // S4 independently verifies acceptance, permissions and its completed effects;
 // this owner proves both directions of the actual S1 result relationship.
});}
Status validate_travel_dependents(const CommonStore& common,const TravelDependentState& state,Integer selected,const BirthPlacementRead* births,const NativeCareDependentPlacementRead* care,ExactArithmeticBudget budget){return status([&]{
 if(selected!=2)fail("UnsupportedTravelDependentsVersion");take(validate_travel_dependent_binding_chain(common,state,budget));
 std::set<StableId> children,original_births,created_bindings;bool has_care=false;
 for(const auto& item:state.binding_change_results.rows()){
  if(const auto* row=std::get_if<BirthDependentBindingChange>(&item.second)){
   if(!births||!births->bound_to(common))fail("BirthPlacementOriginalSourcesRequired",item.first);
   const MomentOwnerSource result{{"travel",result_kind,item.first},2};const auto original=take(births->read_birth_placement_application(row->actual_birth_source,result));
   if(original.actual_placement!=&row->actual_placement||original.child_actor_id!=row->child_actor_id||original.delivery_actor_id!=row->delivery_actor_id||original.birth_arrangement_id!=row->birth_arrangement_id||!same_moment(original.occurred_at,row->occurred_at,budget)||!children.insert(row->child_actor_id).second||!original_births.insert(row->actual_birth_source.source_ref.id).second)fail("TravelDependentBirthSourceCoverage",item.first);
   for(const auto& ref:row->after_binding_refs)if(!created_bindings.insert(ref).second)fail("DuplicateTravelDependentBinding",ref);
  }else has_care=true;
 }
 if(created_bindings.size()!=state.bindings.size())fail("UnlinkedTravelDependentBinding");
 if(has_care&&!care)fail("CareDependentOriginalResultReaderRequired");
 if(care){if(!care->bound_to(common,state))fail("CareDependentOriginalResultReaderRequired");take(care->validate_stable_sources());}
 for(const auto& item:common.state().facts.rows())if(item.second.domain=="family_birth"&&item.second.kind=="family_birth_recorded"&&!original_births.count(item.first))fail("BirthPlacementBirthCoverageMismatch",item.first);
});}
Outcome<CareDependentPlacementApplication> adopt_care_settlement_handover_in_candidate(CommonStore& common,TravelDependentState& state,const CareDependentPlacementContext& x,const MomentOwnerSource& execution){return guard<CareDependentPlacementApplication>([&]{
 bound(x);if(&common!=&x.common||&state!=&x.dependents)fail("CareDependentSameCandidateRequired");const auto original=original_input(x,execution);
 const CareDependentBindingChange* prior=nullptr;for(const auto& item:state.binding_change_results.rows())if(const auto* row=std::get_if<CareDependentBindingChange>(&item.second);row&&same(row->actual_handover_source,execution)){if(prior)fail("DuplicateCareDependentPlacement",execution.source_ref.id);prior=row;}
 if(prior){const MomentOwnerSource result{{"travel",result_kind,prior->id},2};read_result(x,result);return CareDependentPlacementApplication{result,true};}
 if(original.actual_completion)fail("CareDependentMissingOriginalPlacement",execution.source_ref.id);
 const auto now=take(x.handovers.acceptance.goals.clock.read_current_moment());if(!same_moment(now,original.last_actual_period->provided_at,x.budget))fail("CareDependentActualCompletionMomentRequired",execution.source_ref.id);
 const auto& child=original.accepted->subject_actor_id;const auto& carer=original.accepted->provider.carer_actor_id;
 for(const auto* id:{&child,&carer}){const auto* actor=common.actor(*id);if(!actor||!actor->alive)fail("CareDependentActualActorRequired",*id);settlement(actor->location,*id);const auto* binding=x.movement.spatial_bindings.find("actor:"+*id);if(binding&&!std::holds_alternative<StableMovementWriter>(binding->writer))throw Failure{common_failure("CareDependentStablePlacementRequired",*id,ErrorKind::unsupported_capability).error};}
 const auto place=actual_place(x,original,prefix(common,common.state().facts.order().size()));const auto before=before_bindings(x,child,{});
 const auto& expected=settlement(place.location,child);for(const auto* id:{&child,&carer}){const auto& current=settlement(common.actor(*id)->location,*id);if(current.place_id!=expected.place_id||current.settlement_id!=expected.settlement_id)fail("CareDependentCurrentProjectionMismatch",*id);}
 CareDependentBindingChange row;row.id=take(common.next_id("travel_dependent_change"));row.actual_handover_source=execution;row.occurred_at=now;row.child_actor_id=child;row.carer_actor_id=carer;row.actual_location=place.location;row.resolved_anchor=place.anchor;row.before_binding_refs=before;
 const auto key=row.id;if(!state.binding_change_results.insert(key,TravelDependentBindingChangeResult{std::move(row)}))fail("DuplicateCareDependentPlacement",key);take(common.append_fact({key,result_kind,"travel"}));
 const MomentOwnerSource result{{"travel",result_kind,key},2};read_result(x,result);take(validate_travel_dependent_binding_chain(common,state,x.budget));return CareDependentPlacementApplication{result,false};
});}
}
