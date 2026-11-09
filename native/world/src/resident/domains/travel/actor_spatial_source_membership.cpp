#include "actor_spatial_source_membership.hpp"
#include "local_move_source.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <type_traits>
#include "birth_placement.hpp"
#include "military_action_placement.hpp"
#include "movement_exact_detail.hpp"
#include "movement_result_read.hpp"
#include "movement_control.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {using namespace movement_detail;namespace {
class PreparedActorSpatialTransitionPrefix final {
 CheckpointMemoryCharge copied_fields_;
 ActorSpatialActualFactPrefix prefix_;
 static CheckpointMemoryCharge memory(const FactRecord* head){
  return CheckpointMemoryCharge(0);
 }
public:
 PreparedActorSpatialTransitionPrefix(Integer count,const FactRecord* head)
  :copied_fields_(memory(head)){
  static_assert(std::is_nothrow_move_constructible_v<SourceRef>,"Actor spatial transition prefix requires nonthrowing payload transfer");
  prefix_.common_fact_count=count;
  if(head)prefix_.last_common_fact.emplace(SourceRef{head->domain,head->kind,head->id});
 }
 ~PreparedActorSpatialTransitionPrefix()=default;
 PreparedActorSpatialTransitionPrefix(const PreparedActorSpatialTransitionPrefix&)=delete;
 PreparedActorSpatialTransitionPrefix& operator=(const PreparedActorSpatialTransitionPrefix&)=delete;
 PreparedActorSpatialTransitionPrefix(PreparedActorSpatialTransitionPrefix&&)=delete;
 PreparedActorSpatialTransitionPrefix& operator=(PreparedActorSpatialTransitionPrefix&&)=delete;
 const ActorSpatialActualFactPrefix& prefix()const noexcept{return prefix_;}
};
bool contains(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
void header(const CommonStore& c,const SourceRef& source){const auto* row=c.state().facts.find(source.id);if(!row||row->id!=source.id||row->kind!=source.kind||row->domain!=source.domain)fail("ActorSpatialHeadSourceRequired",source.id);}
void not_future(const WorldMoment& at,const WorldMoment& now,ExactArithmeticBudget b){take(encode_world_moment(at,b));if(take(compare_world_moments(at,now,b))>0)fail("ActorSpatialHeadSourceInFuture");}
bool unit_member(const MovementExactState& movement,const StableId& unit_id,const StableId& actor){
 const auto* unit=movement.units.find(unit_id);if(!unit||unit->id!=unit_id||unit->effort_basis_refs.empty())fail("ActorSpatialHeadUnitRequired",unit_id);
 bool found=false;for(const auto& ref:unit->effort_basis_refs){
  const auto* basis=ref.domain=="travel"&&ref.kind=="travel_exertion_basis"?movement.exertion_bases.find(ref.id):nullptr;
  if(!basis||basis->id!=ref.id||basis->unit_ref!=unit_id||execution_key(basis->execution)!=execution_key(unit->execution))fail("ActorSpatialHeadBasisRequired",unit_id);
  if(contains(basis->actual_participants,actor))found=true;
 }return found;
}
}
bool is_actor_spatial_head_source_kind(const SourceRef& ref)noexcept{
 if(ref.domain=="actor_foundation")return ref.kind=="scenario_placement";
 if(ref.domain=="family")return ref.kind=="actor_creation";
 if(ref.domain=="military")return is_military_action_placement_source_kind(ref);
 if(ref.domain!="travel")return false;
 return ref.kind=="local_move"||ref.kind=="travel_dependent_binding_change"||ref.kind=="travel_exertion_basis"||ref.kind=="movement_unit_close"||ref.kind=="movement_arrival"||ref.kind=="personal_movement_control"||ref.kind=="transport_registration"||ref.kind=="transport_release";
}
namespace {
struct ActualHeadMembership { bool actor_member=false; WorldMoment occurred_at; };
Outcome<ActualHeadMembership> read_membership(const ActorSpatialHeadSourceReadContext& x,const StableId& actor,const MomentOwnerSource& source){return guard<ActualHeadMembership>([&]{
 const auto& ref=source.source_ref;if(actor.empty()||!x.common.actor(actor)||!source_valid(ref)||!is_actor_spatial_head_source_kind(ref))fail("ActorSpatialCanonicalHeadRequired",ref.id);
 if(source.source_schema!=take(actor_spatial_history_source_schema(ref)))fail("ActorSpatialHeadSchemaMismatch",ref.id);
 const auto now=take(x.spatial.current_moment());
 if(ref.domain=="actor_foundation"){
  const auto origin=take(read_authored_character_origin_v2(x.common.state(),x.foundation,x.actors,ref.id));
  const auto* definition=x.actors.actors.find(ref.id);const auto* binding=x.foundation.actor_start_context.actor_domain_bindings.find(ref.id);
  if(!definition||!binding||definition->placement.kind!=AuthoredStartPlacement::Kind::at_place||binding->placement_ref.kind!=AuthoredStartPlacement::Kind::at_place||definition->placement.place_ref!=binding->placement_ref.place_ref||!x.common_catalog.places.find(definition->placement.place_ref))fail("ActorOriginalPlacementRequired",ref.id);
  not_future(origin.created_at.exact_moment,now,x.budget);return ActualHeadMembership{ref.id==actor,origin.created_at.exact_moment};
 }
 header(x.common,ref);
 if(ref.domain=="family"){
  if(!x.family_catalog)throw Failure{common_failure("ActorCreatedPlacementHistoryRequired",ref.id,ErrorKind::unsupported_capability).error};
  const auto placement=take(read_created_character_initial_placement_v2(x.common,x.foundation,x.actors,*x.family_catalog,x.common_catalog,ref.id,now));
  if(!same_source(placement.actual_creation_source,ref)||placement.source_schema!=source.source_schema||placement.actor_id!=ref.id)fail("ActorCreatedPlacementSourceRequired",ref.id);
  not_future(placement.created_at,now,x.budget);return ActualHeadMembership{placement.actor_id==actor,placement.created_at};
 }
 if(ref.domain=="military"){
  if(!x.military_actions||!x.military_actions->bound_to(x.common,x.movement))fail("MilitaryActionPlacementOriginalReadRequired",actor);
  const auto at=take(x.military_actions->read_military_action_placement_source_moment(source));not_future(at,now,x.budget);
  auto actual=take(x.military_actions->read_military_action_placement(source,actor));
  if(actual&&!same_moment(actual->occurred_at,at,x.budget))fail("MilitaryActionPlacementMomentMismatch",ref.id);
  return ActualHeadMembership{actual.has_value(),at};
 }
 if(ref.kind=="travel_dependent_binding_change"){
  if(!x.births||!x.births->bound_to(x.common))throw Failure{common_failure("BornActualPlacementHistoryRequired",ref.id,ErrorKind::unsupported_capability).error};
  const auto original=take(x.births->read_birth_placement_result(source));
  if(!original.actual_placement||!same_source(original.location_result.source_ref,ref)||original.location_result.source_schema!=source.source_schema||original.child_actor_id.empty()||!x.common.actor(original.child_actor_id))fail("BornActualPlacementSourceRequired",ref.id);
  not_future(original.occurred_at,now,x.budget);return ActualHeadMembership{original.child_actor_id==actor,original.occurred_at};
 }
 if(ref.kind=="local_move") {
  const auto* row=take(read_local_move_source(x.common,x.movement,ref,source.source_schema,x.budget));
  not_future(row->occurred_at,now,x.budget); return ActualHeadMembership{row->actor_id==actor,row->occurred_at};
 }
 if(ref.kind=="transport_registration"||ref.kind=="transport_release"){
  const TransportRegistrationSource* registered=nullptr;const TransportUnitReleaseSource* released=nullptr;
  if(ref.kind=="transport_registration")registered=x.registrations.registrations.find(ref.id);
  else released=x.registrations.releases.find(ref.id);
  if((!registered&&!released)||(registered&&registered->id!=ref.id)||(released&&released->id!=ref.id))fail("ActorTransportHeadSourceRequired",ref.id);
  const auto& unit_id=registered?registered->transport_unit_id:released->transport_unit_id;const auto* unit=x.carriers.transport_units.find(unit_id);
  if(!unit||unit->transport_unit_id!=unit_id)fail("ActorTransportHeadUnitRequired",ref.id);
  take(x.spatial.verify_registration_source(*unit));
  if(registered&&!same_source(unit->registration_source_ref,ref))fail("ActorTransportHeadBacklinkMismatch",ref.id);
  if(released&&(unit->state!=TransportUnitLifecycle::dissolved||released->released_composition.transport_unit_id!=unit_id||released->composition_revision==std::numeric_limits<Integer>::max()||unit->composition_revision!=released->composition_revision+1))fail("ActorTransportReleaseHeadMismatch",ref.id);
  not_future(registered?registered->occurred_at:released->occurred_at,now,x.budget);
  return ActualHeadMembership{contains(registered?registered->accepted_scope.named_participants:released->released_composition.named_participants,actor),registered?registered->occurred_at:released->occurred_at};
 }
 take(validate_movement_exact_values(x.movement,x.budget));
 if(ref.kind=="personal_movement_control"){
  take(validate_personal_movement_controls(x.common,x.movement,x.budget));const auto* control=x.movement.personal_controls.find(ref.id);
  if(!control||control->id!=ref.id)fail("ActorSpatialControlHeadRequired",ref.id);
  not_future(control->occurred_at,now,x.budget);return ActualHeadMembership{unit_member(x.movement,control->unit_ref,actor),control->occurred_at};
 }
 const auto occurred_at=take(read_movement_source_moment(x.movement,ref,source.source_schema,x.budget));not_future(occurred_at,now,x.budget);
 if(ref.kind=="travel_exertion_basis"){
  const auto* basis=x.movement.exertion_bases.find(ref.id);if(!basis||basis->id!=ref.id)fail("ActorSpatialHeadBasisRequired",ref.id);
  return ActualHeadMembership{contains(basis->actual_participants,actor),occurred_at};
 }
 if(ref.kind=="movement_arrival")return ActualHeadMembership{unit_member(x.movement,take(read_verified_movement_arrival(x.movement,ref,source.source_schema,x.budget)).unit_ref,actor),occurred_at};
 const auto* close=x.movement.unit_closes.find(ref.id);if(!close||close->id!=ref.id)fail("ActorSpatialCloseHeadRequired",ref.id);
 return ActualHeadMembership{unit_member(x.movement,close->unit_ref,actor),occurred_at};
});}
std::size_t causal_ordinal(const CommonStore& common,const SourceRef& ref){
 if(ref.domain=="actor_foundation"&&ref.kind=="scenario_placement")return 0;
 header(common,ref);const auto& ids=common.state().facts.order();const auto found=std::find(ids.begin(),ids.end(),ref.id);
 if(found==ids.end())fail("ActorSpatialHistorySourceOrderRequired",ref.id);return std::size_t(found-ids.begin())+1;
}
}
Outcome<bool> read_actor_spatial_head_source_membership(const ActorSpatialHeadSourceReadContext& x,const StableId& actor,const MomentOwnerSource& source){return guard<bool>([&]{return take(read_membership(x,actor,source)).actor_member;});}
Outcome<std::optional<WorldMoment>> read_actor_spatial_head_source_occurrence(const ActorSpatialHeadSourceReadContext& x,const StableId& actor,const MomentOwnerSource& source){return guard<std::optional<WorldMoment>>([&]()->std::optional<WorldMoment>{
 const auto original=take(read_membership(x,actor,source));if(!original.actor_member)return {};return original.occurred_at;
});}
Outcome<std::optional<ActorSpatialLocationTransitionRead>> read_actor_spatial_location_transition_at_source(const ActorSpatialHeadSourceReadContext& x,const StableId& actor,const MomentOwnerSource& source,const MilitarySpatialHandoffState* military){return guard<std::optional<ActorSpatialLocationTransitionRead>>([&]()->std::optional<ActorSpatialLocationTransitionRead>{
 const auto original=take(read_membership(x,actor,source));if(!original.actor_member)return {};
 const auto& ref=source.source_ref;
 if(ref.domain=="actor_foundation"||ref.domain=="family"||ref.kind=="travel_dependent_binding_change")throw Failure{common_failure("ActorInitialPlacementIsNotLocationTransition",ref.id,ErrorKind::unsupported_capability).error};
 const auto& ids=x.common.state().facts.order();const auto found=std::find(ids.begin(),ids.end(),ref.id);
 if(found==ids.end()||std::count(ids.begin(),ids.end(),ref.id)!=1||ids.size()>static_cast<std::uint64_t>(std::numeric_limits<Integer>::max()))fail("ActorSpatialTransitionActualCutRequired",ref.id);
 const auto index=static_cast<std::size_t>(found-ids.begin());
 auto prefix=[&](std::size_t count){const FactRecord* last=nullptr;if(count){last=x.common.state().facts.find(ids[count-1]);if(!last||last->id!=ids[count-1])fail("ActorSpatialTransitionActualCutRequired",ref.id);}return PreparedActorSpatialTransitionPrefix(static_cast<Integer>(count),last);};
 auto at_cut=[&](std::size_t count){const auto actual_prefix=prefix(count);return take(read_actor_spatial_location_at_actual_fact_prefix(x.common,x.common_catalog,x.movement,x.carriers,x.registrations,x.foundation,x.actors,x.spatial,actor,original.occurred_at,actual_prefix.prefix(),military,x.budget,x.family_catalog,x.births,x.military_actions));};
 ActorSpatialLocationTransitionRead out;out.actor_id=actor;out.actual_source=source;out.occurred_at=original.occurred_at;
 out.before=at_cut(index);out.after=at_cut(index+1);
 if(out.after.actual_location_sources.size()!=1||!same_source(out.after.actual_location_sources.front().source,ref)||out.after.actual_location_sources.front().schema!=source.source_schema)fail("ActorSpatialTransitionActualHeadMismatch",ref.id);
 out.actual_position_changed=!take(same_actual_movement_position(*take(require_actor_ordinary_movement_anchor(out.before)),*take(require_actor_ordinary_movement_anchor(out.after)),x.budget));
 return out;
});}
Outcome<ActorSpatialLocationView> read_actor_spatial_location_from_head_pool(const ActorSpatialHeadSourceReadContext& x,const StableId& actor,const std::vector<MomentOwnerSource>& pool,const WorldMoment& actual_from,const MilitarySpatialHandoffState* military){return guard<ActorSpatialLocationView>([&]{
 take(encode_world_moment(actual_from,x.budget));const MomentOwnerSource* selected=nullptr;WorldMoment selected_at;std::size_t selected_order=0;
 for(const auto& source:pool){
  const auto actual=take(read_membership(x,actor,source));
  if(take(compare_world_moments(actual.occurred_at,actual_from,x.budget))>0)fail("ActorSpatialHeadPoolSourceAfterAdoption",source.source_ref.id);
  if(!actual.actor_member)continue;
  const auto order=causal_ordinal(x.common,source.source_ref);
  const auto by_moment=selected?take(compare_world_moments(actual.occurred_at,selected_at,x.budget)):1;
  if(by_moment>0||(by_moment==0&&order>selected_order)){selected=&source;selected_at=actual.occurred_at;selected_order=order;}
  else if(by_moment==0&&order==selected_order&&selected&&!same_source(selected->source_ref,source.source_ref))fail("ActorSpatialHeadPoolCausalCollision",actor);
 }
 if(!selected)fail("ActorSpatialFrozenSourceHeadRequired",actor);
 const std::vector<ActualSpatialSourceRef> head{{selected->source_ref,selected->source_schema}};
 return take(read_actor_spatial_location_at_source_head(x.common,x.common_catalog,x.movement,x.carriers,x.registrations,x.foundation,x.actors,x.spatial,actor,head,actual_from,military,x.budget,x.family_catalog,x.births,x.military_actions));
});}
}
