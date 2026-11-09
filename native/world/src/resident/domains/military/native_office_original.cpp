#include "native_office_original.hpp"
#include "office_exact_detail.hpp"
#include "military_equal.hpp"
#include "military_battle_lifecycle.hpp"
#include "military_roster_history.hpp"
#include "naval_battle_history.hpp"
#include "occupation_time.hpp"
#include "resident/domains/personal/personal_battle_lifecycle.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
using namespace office_exact_detail;
using Endpoint=MilitaryActionConditionEndpoint2;
const ExactOfficeSourceReadContext& office(const NativeOfficeOriginalContext& x){return x.authority.office;}
const CommonStore& common(const NativeOfficeOriginalContext& x){return x.authority.common;}
WorldMoment roots(const NativeOfficeOriginalContext& x){
 const auto& a=x.authority;const auto& o=office(x);const auto& q=x.conditions;const auto& p=x.spatial;
 take(validate_selected_military_roster_root(a.common,x.identity,x.exact));take(validate_selected_military_field_history(a.common,a.military,x.identity,x.field_history));
 if(x.handoffs&&(!x.exact||x.handoffs!=&x.exact->spatial_handoffs))fail("NativeOfficeSpatialRootSelectionMismatch");
 take(validate_military_authority_source_context(a));if(!o.catalog.military)fail("MilitaryUnavailable");
 if(&q.common!=&a.common||&q.actions!=&a.actions||&q.sources!=&o.body||&q.foundation!=&o.body.state||&q.clock!=&o.clock||o.access.military_action_conditions_schema!=2||!o.access.action_conditions)
  fail("NativeOfficeConditionRootsRequired");
 const auto& selected=*o.access.action_conditions;
 if(&selected.common!=&q.common||&selected.actions!=&q.actions||&selected.foundation!=&q.foundation||&selected.sources!=&q.sources||&selected.clock!=&q.clock)
  fail("NativeOfficeConditionRootsRequired");
 if(&p.common!=&a.common||&p.foundation!=&q.foundation||&p.actors!=&q.sources.catalog||o.body.shared!=&p.common_catalog||!p.military_actions||o.body.military!=&a.military||o.body.military_exact!=x.exact)
  fail("NativeOfficeSpatialRootsRequired");
 if(!x.formations.bound_to(a.common.state(),a.military)||!x.returns.bound_to(a.common,a.military)||o.body.military_formations!=&x.formations||o.body.military_returns!=&x.returns)
  fail("NativeOfficeMilitarySourceRootsRequired");
 // 生命来源保存的是精确军事实根指针；异根或缺根仍拒绝任职读取。
 if (const auto *life = o.body.life_sources)
   if (&life->military != &a.military || life->military_exact != x.exact ||
       life->military_formations != &x.formations ||
       life->military_returns != &x.returns)
     fail("NativeOfficeLifeSourceRootsRequired");
 if(x.selected_land_lifecycle!=1||o.access.military_battle_lifecycle_schema!=x.selected_land_lifecycle)
  fail("NativeOfficeLandLifecycleRequired");
 if(x.selected_personal==0){if(x.personal||x.selected_personal_lifecycle)fail("UnexpectedNativeOfficePersonalSources");}
 else if(x.selected_personal!=1||!x.personal||x.selected_personal_lifecycle!=1)fail("NativeOfficePersonalLifecycleRequired");
 if(x.selected_naval==0){if(x.naval)fail("UnexpectedNativeOfficeNavalSources");}
 else {
  if(x.selected_naval<0||!x.naval||!x.naval->assets.original_sources||!x.naval->history.original_sources||x.naval->assets.original_sources!=x.naval->history.original_sources)
   fail("NativeOfficeNavalSourcesRequired");
  const auto& n=*x.naval->assets.original_sources;
  take(validate_naval_original_source_context(n,a.common,a.military,o.catalog,o.access));
  if(&n.exact!=x.exact||&n.actions!=&a.actions||&n.elapsed!=&a.elapsed||&n.authority.office.body!=&o.body||&n.authority.office.clock!=&o.clock||&n.formations!=&x.formations||&n.returns!=&x.returns)
   fail("NativeOfficeNavalRootsRequired");
 }
 const auto now=take(o.clock.read_current_moment());
 if(!same_moment(now,o.body.actual_now,x.budget)||now.half_day_tick<a.common.tick())fail("NativeOfficeClockMismatch");
 return now;
}
bool same_head(const ActorConditionHead& a,const ActorConditionHead& b){return a.actor_id==b.actor_id&&a.initial_condition_seed_ref.seed_key==b.initial_condition_seed_ref.seed_key&&a.condition_revision==b.condition_revision&&bool(a.latest_receipt_ref)==bool(b.latest_receipt_ref)&&(!a.latest_receipt_ref||a.latest_receipt_ref->result_id==b.latest_receipt_ref->result_id);}
bool same_observation(const std::optional<MilitaryActionEndpointObservation2>& a,const std::optional<MilitaryActionEndpointObservation2>& b){
 if(bool(a)!=bool(b))return false;if(!a)return true;
 if(a->common_prefix.common_fact_count!=b->common_prefix.common_fact_count||bool(a->common_prefix.last_common_fact)!=bool(b->common_prefix.last_common_fact)||(a->common_prefix.last_common_fact&&!same_source(*a->common_prefix.last_common_fact,*b->common_prefix.last_common_fact))||a->condition_prefix.applied_receipt_count!=b->condition_prefix.applied_receipt_count||bool(a->condition_prefix.last_applied_receipt)!=bool(b->condition_prefix.last_applied_receipt)||(a->condition_prefix.last_applied_receipt&&a->condition_prefix.last_applied_receipt->result_id!=b->condition_prefix.last_applied_receipt->result_id)||a->participant_heads.size()!=b->participant_heads.size())return false;
 for(std::size_t n=0;n<a->participant_heads.size();++n)if(!same_head(a->participant_heads[n],b->participant_heads[n]))return false;return true;
}
const DynamicMilitaryActionFact2& retained(const NativeOfficeOriginalContext& x,const DynamicMilitaryActionFact2& input){
 const auto& s=x.authority.actions;const auto& f=fact(s,input.id);
 // Hot acceptance passes its Prepared local row, while advance and completion
 // pass retained rows. Compare the actual payload, never the address of input.
 if(input.command!=f.command||input.actor_id!=f.actor_id||input.phase!=f.phase||input.success!=f.success||input.rule_version!=f.rule_version||input.elapsed_source_ref.action_ref!=f.elapsed_source_ref.action_ref||!same_moment(input.started_at,f.started_at,x.budget)||!same_terms(input.original_terms,f.original_terms)||!same_context(input.original_context,f.original_context)||bool(input.completed_at)!=bool(f.completed_at)||(input.completed_at&&!same_moment(*input.completed_at,*f.completed_at,x.budget))||bool(input.terminal_source_ref)!=bool(f.terminal_source_ref)||(input.terminal_source_ref&&!same_source(*input.terminal_source_ref,*f.terminal_source_ref))||bool(input.result)!=bool(f.result)||!same_observation(input.acceptance_observation,f.acceptance_observation)||!same_observation(input.completion_observation,f.completion_observation))fail("NativeOfficeRetainedPayloadMismatch",input.id);
 if(input.result&&(input.result->ok!=f.result->ok||input.result->office_id!=f.result->office_id||input.result->army_id!=f.result->army_id||input.result->order_id!=f.result->order_id||input.result->field_order!=f.result->field_order))fail("NativeOfficeRetainedPayloadMismatch",input.id);
 if(input.acceptance_conditions||f.acceptance_conditions||input.actual_resource_effect||f.actual_resource_effect||input.acceptance_resource_reservations||f.acceptance_resource_reservations)fail("NativeOfficeUnexpectedPayload",input.id);
 validate_original_office_identity(common(x),x.authority.military,s,office(x),f);
 if(f.phase=="started"){if(f.completed_at||f.result||f.success||f.terminal_source_ref||f.completion_observation)fail("ExactOfficePrematureResult",f.id);}
 else if(f.phase=="completed")validate_completion_header(common(x),s,f);
 else fail("InvalidExactOfficePhase",f.id);
 return f;
}
const OfficeDefinition& definition(const NativeOfficeOriginalContext& x,const OfficeTerms& t){
 const OfficeDefinition* found=nullptr;for(const auto& d:office(x).catalog.offices)if(d.id==t.office_id){if(found)fail("DuplicateOfficeDefinition",t.office_id);found=&d;}
 if(!found)fail("UnknownOffice",t.office_id);bool exists=false;for(const auto& current:x.authority.military.offices)if(current.id==t.office_id){if(exists)fail("DuplicateOffice",t.office_id);exists=true;}if(!exists)fail("UnknownOffice",t.office_id);return *found;
}
SourceRefBuildView boundary(const DynamicMilitaryActionFact2& f,Endpoint e){return e==Endpoint::acceptance?SourceRefBuildView{"military","military_action",f.id}:SourceRefBuildView{f.terminal_source_ref->domain,f.terminal_source_ref->kind,f.terminal_source_ref->id};}
const WorldMoment& endpoint_at(const DynamicMilitaryActionFact2& f,Endpoint e){if(e==Endpoint::acceptance)return f.started_at;if(!f.completed_at)fail("MilitaryActionCompletionObservationRequired",f.id);return *f.completed_at;}
const CommonActualFactPrefix& prefix(const DynamicMilitaryActionFact2& f,Endpoint e){const auto& observed=e==Endpoint::acceptance?f.acceptance_observation:f.completion_observation;if(!observed)fail("MilitaryActionEndpointOwnerRequired",f.id);return observed->common_prefix;}
// A fresh synchronous source scope for exactly one original endpoint. The
// permanent context and Access pointers are never modified or claimed rebound.
class OfficeEndpointSources final {
 MilitaryActualSourceWindow window_;
 NavalWindowedFormationSourceRead formations_;
 NavalWindowedReturnSourceRead returns_;
 std::optional<FamilyLifeCauseSources4> life_;
 CharacterConditionSources body_;
 NativeOfficeOriginalContext view_;
 static CharacterConditionSources body(const NativeOfficeOriginalContext& x,
  const MilitaryFormationSourceRead& formations,const MilitaryReturnSourceRead& returns,
  std::optional<FamilyLifeCauseSources4>& life){
  auto result=x.conditions.sources;
  result.military_history_order=MilitaryCasualtyHistoryOrder::actual_common_prefix;
  result.military_formations=&formations;result.military_returns=&returns;
  if(life){life->military_formations=&formations;life->military_returns=&returns;result.life_sources=&*life;}
  return result;
 }
 static MilitaryAuthoritySourceContext authority(const NativeOfficeOriginalContext& x,
  const CharacterConditionSources& body,const MilitaryActualSourceWindow& window){
  const auto& a=x.authority;const auto& o=a.office;
  return {a.common,a.military,a.actions,a.elapsed,
   {o.common,o.military,o.actions,o.elapsed,o.catalog,o.access,o.scale,o.clock,o.prepared,o.organization,o.family,body,o.budget,o.selected_action_time},
   a.selected_authority_time,&window};
 }
public:
 OfficeEndpointSources(const NativeOfficeOriginalContext& x,const DynamicMilitaryActionFact2& f,Endpoint e,const WorldMoment& now)
  :window_{prefix(f,e),endpoint_at(f,e)},
   formations_{common(x),x.authority.military,x.formations,x.returns,x.authority.actions,office(x).catalog,office(x).access,x.budget,window_,now},
   returns_{common(x),x.authority.military,x.formations,x.returns,x.authority.actions,office(x).catalog,office(x).access,x.budget,window_,now},
   life_(x.conditions.sources.life_sources?std::optional<FamilyLifeCauseSources4>{*x.conditions.sources.life_sources}:std::nullopt),
   body_(body(x,formations_,returns_,life_)),
   view_{authority(x,body_,window_),{x.conditions.common,x.conditions.actions,x.conditions.foundation,body_,x.conditions.clock},
    x.exact,x.identity,x.spatial,formations_,returns_,x.handoffs,x.personal,x.naval,
    x.selected_personal,x.selected_personal_lifecycle,x.selected_naval,x.selected_land_lifecycle,x.budget,x.field_history}{}
 OfficeEndpointSources(const OfficeEndpointSources&)=delete;
 OfficeEndpointSources& operator=(const OfficeEndpointSources&)=delete;
 OfficeEndpointSources(OfficeEndpointSources&&)=delete;
 OfficeEndpointSources& operator=(OfficeEndpointSources&&)=delete;
 const NativeOfficeOriginalContext& view()const noexcept{return view_;}
};
bool battle(const NativeOfficeOriginalContext& x,const StableId& actor,const CommonActualFactPrefix& cut,const WorldMoment& at,const WorldMoment& now){
 const auto& o=office(x);
 // Literal original disjunction, including its ordering and short circuit.
 if(x.naval&&take(naval_actor_held_at_actual_prefix(common(x),x.authority.military,o.catalog,o.access,x.naval->travel,x.naval->state,x.naval->catalog,x.naval->history,x.naval->assets,actor,cut,at)))return true;
 if(x.personal&&take(read_personal_battle_occupied_before(common(x),*x.personal,x.selected_personal_lifecycle,actor,cut,at,now,x.budget)).occupied)return true;
 return take(read_selected_military_battle_occupied_before(common(x),x.authority.military,x.identity,x.exact,x.selected_land_lifecycle,actor,cut,at,now,&x.formations,&x.returns,x.budget,x.field_history)).occupied;
}
bool at_place(const MovementAnchor& anchor,const StableId& place){const auto* original=std::get_if<OriginalMovementAnchor>(&anchor);const auto* at=original?std::get_if<SettlementLocation>(&original->location):nullptr;return at&&at->place_id==place;}
bool at_place(const NativeOfficeOriginalContext& x,const StableId& actor,const StableId& place,const CommonActualFactPrefix& cut,const WorldMoment& at){
 const auto& p=x.spatial;const auto point=take(read_actor_spatial_location_at_actual_fact_prefix(p.common,p.common_catalog,p.movement,p.carriers,p.registrations,p.foundation,p.actors,p.spatial,actor,at,cut,x.handoffs,p.budget,p.family_catalog,p.births,p.military_actions));
 const auto* anchor=ordinary_actor_movement_anchor(point);return anchor&&at_place(*anchor,place);
}
bool raw_at_place(const NativeOfficeOriginalContext& x,const StableId& actor,const StableId& place,const CommonActualFactPrefix& cut,const WorldMoment& at){
 const auto& p=x.spatial;const auto point=take(read_actor_spatial_location_at_actual_fact_prefix(p.common,p.common_catalog,p.movement,p.carriers,p.registrations,p.foundation,p.actors,p.spatial,actor,at,cut,x.handoffs,p.budget,p.family_catalog,p.births,p.military_actions));
 // The original military at() tests the actor's un-resolved Location. Contact
 // separately uses its resolved place, so an InArmy projection stays distinct.
 const auto* location=std::get_if<SettlementLocation>(&point.location_snapshot);return location&&location->place_id==place;
}
bool body_ready(const NativeOfficeOriginalContext& x,const DynamicMilitaryActionFact2& f,Endpoint e,const StableId& actor){return take(read_military_action_endpoint_participant_body_ready(x.conditions,f.id,e,actor,2));}
bool participant_available(const NativeOfficeOriginalContext& x,const DynamicMilitaryActionFact2& f,Endpoint e,const StableId& actor){
 // Acceptance provider uses the original contact-only body predicate and the
 // same head/availability equality gate. Requester readiness was already true.
 if(e==Endpoint::acceptance)return body_ready(x,f,e,actor);
 // Completion already checked both participant bodies above, even when false.
 const auto view=boundary(f,e);auto memory=CheckpointMemoryCharge(0);const SourceRef ref{StableId(view.domain),StableId(view.kind),StableId(view.id)};
 const auto value=take(read_character_availability_before_fact_provenance_v2(office(x).body,actor,ref,endpoint_at(f,e),office(x).clock));
 if(!value.present)fail("NativeOfficeParticipantSourceRequired",actor);return value.alive&&value.available;
}
bool contact(const NativeOfficeOriginalContext& x,const DynamicMilitaryActionFact2& f,Endpoint e,const WorldMoment& now,bool accepted_requester_ready=false){
 const auto& t=std::get<OfficeTerms>(f.original_terms);const auto& ctx=*f.original_context;const auto& cut=prefix(f,e);const auto& at=endpoint_at(f,e);
 // The service-bearing Office context is the ordinary authored role branch;
 // its original identity kernel rejects narrative/encounter/service mismatch.
 if(battle(x,t.actor_id,cut,at,now)||battle(x,t.provider_actor_id,cut,at,now))return false;
 if(ctx.kind&&(*ctx.kind=="encounter"||*ctx.kind=="narrative"))return false;
 if(!at_place(x,t.actor_id,ctx.place_id,cut,at))return false;
 if(!participant_available(x,f,e,t.provider_actor_id)||!at_place(x,t.provider_actor_id,ctx.place_id,cut,at))return false;
 const auto role=take(read_original_military_interaction_role_state_before_prefix(common(x),office(x).prepared,office(x).organization,office(x).family,cut,ctx.role_id,ctx.provider_actor_id,StableId{"office_accept"}));
 if(!role.enabled||role.original.authored_role->place_id!=ctx.place_id)return false;
 // Native service_admission's own-action exception applies to this exact
 // frozen provider participant. The actual ordinary Action/occupation is the
 // caller's S0 responsibility, not a historical lookup of today's action_id.
 // 仅同一次 acceptance 的同端点、同参与者已证明 true；中间全部只读。
 // completion 从不传此值，仍按原身体/来源读取顺序核验，不跨候选保留结果。
 return accepted_requester_ready||participant_available(x,f,e,t.actor_id);
}
bool friendly_garrison(const NativeOfficeOriginalContext& x,const DynamicMilitaryActionFact2& f,Endpoint e,const OfficeDefinition& d,const WorldMoment& now){
 const auto& o=office(x);const auto owner=take(read_military_garrison_owner_before_actual(common(x),x.authority.military,o.catalog,o.access,d.garrison_id,boundary(f,e),endpoint_at(f,e),now,o.access.occupation_source_schema,x.budget,x.authority.window));
 return owner.organization_id()==std::get<OfficeTerms>(f.original_terms).organization_id;
}
const DynamicMilitaryActionFact2* holder(const NativeOfficeOriginalContext& x,const StableId& id,const DynamicMilitaryActionFact2& f,Endpoint e){
 const auto view=boundary(f,e);auto memory=CheckpointMemoryCharge(0);const SourceRef ref{StableId(view.domain),StableId(view.kind),StableId(view.id)};
 return take(read_military_office_authority_before(x.authority,id,ref,endpoint_at(f,e))).appointment;
}
void prior_reservations(const NativeOfficeOriginalContext& x,const DynamicMilitaryActionFact2& f,const WorldMoment& now){
 const auto& t=std::get<OfficeTerms>(f.original_terms);const auto& cut=prefix(f,Endpoint::acceptance);const auto count=take(read_actual_common_fact_prefix(common(x),cut));const MilitaryActualSourceWindow window{cut,f.started_at};
 for(const auto& row:x.authority.actions.dynamic_action_facts.rows()){
  const auto& other=row.second;if(other.id==f.id||other.command!="office_accept")continue;const auto* terms=std::get_if<OfficeTerms>(&other.original_terms);if(!terms)fail("ExactOfficeOriginalTermsMismatch",row.first);if(terms->office_id!=t.office_id)continue;
  if(row.first!=other.id)fail("ExactOfficeSourceKeyMismatch",row.first);validate_original_office_identity(common(x),x.authority.military,x.authority.actions,office(x),other);
  const SourceRefBuildView start{"military","military_action",other.id};const auto begin=take(read_actual_common_fact_position(common(x),start));take(validate_military_source_in_actual_window(common(x),start,other.started_at,now,&window,x.budget));
  std::optional<std::size_t> end;
  if(other.phase=="completed"){validate_completion_header(common(x),x.authority.actions,other);end=take(read_actual_common_fact_position(common(x),*other.terminal_source_ref));take(validate_military_source_in_actual_window(common(x),*other.terminal_source_ref,*other.completed_at,now,&window,x.budget));if(*end<count){const auto* applied=take(read_exact_office_completion_application(common(x),x.authority.military,x.authority.actions,x.authority.elapsed,office(x),*other.terminal_source_ref,2));if(applied!=&other)fail("ExactOfficeCompletionApplicationRequired",other.id);}}
  else if(other.phase!="started"||other.completed_at||other.terminal_source_ref||other.result||other.success)fail("InvalidExactOfficePhase",other.id);
  if(begin<count&&(!end||*end>=count))fail("OfficeAppointmentReserved",t.office_id);
 }
 for(const auto& old:x.authority.military.action_facts)if(old.id!=f.id)if(const auto* terms=std::get_if<OfficeTerms>(&old.terms);terms&&terms->office_id==t.office_id){
  const auto pos=take(read_actual_common_fact_position(common(x),SourceRefBuildView{"military","military_action",old.id}));if(pos<count)fail("NativeOfficeLegacyReservationOccurrenceRequired",old.id);
 }
}
void acceptance(const NativeOfficeOriginalContext& x,const DynamicMilitaryActionFact2& f,const WorldMoment& now){
 take(validate_military_action_endpoint_observation_source(x.conditions,f.id,Endpoint::acceptance,2));const auto& t=std::get<OfficeTerms>(f.original_terms);
 const bool requester_ready=body_ready(x,f,Endpoint::acceptance,t.actor_id);
 if(!requester_ready||battle(x,t.actor_id,prefix(f,Endpoint::acceptance),f.started_at,now))fail("MilitaryActorUnavailable",t.actor_id);
 const auto& d=definition(x,t);if(!contact(x,f,Endpoint::acceptance,now,requester_ready))fail("ExactOfficeOriginalContactRejected",f.id);
 if(!friendly_garrison(x,f,Endpoint::acceptance,d,now))fail("OfficeAuthorityLost",t.office_id);
 if(holder(x,t.office_id,f,Endpoint::acceptance))fail("OfficeOccupied",t.office_id);
 for(const auto& other:office(x).catalog.offices){const auto* appointment=holder(x,other.id,f,Endpoint::acceptance);if(appointment&&appointment->actor_id==t.actor_id)fail("PublicAppointmentPending",t.actor_id);}
 prior_reservations(x,f,now);
}
}
bool NativeOfficeOriginalRead::bound_to(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s)const noexcept{return &c==&context_.authority.common&&&m==&context_.authority.military&&&s==&context_.authority.actions;}
Status NativeOfficeOriginalRead::validate_office_acceptance(const DynamicMilitaryActionFact2& input)const{return status([&]{const auto now=roots(context_);const auto& f=retained(context_,input);OfficeEndpointSources source(context_,f,Endpoint::acceptance,now);acceptance(source.view(),f,now);});}
Status NativeOfficeOriginalRead::validate_office_actual_service(const DynamicMilitaryActionFact2& input,const MomentSpan& span)const{return status([&]{
 const auto now=roots(context_);const auto& f=retained(context_,input);OfficeEndpointSources source(context_,f,Endpoint::acceptance,now);const auto& x=source.view();acceptance(x,f,now);
 if(take(compare_world_moments(span.from,f.started_at,x.budget))<0||take(compare_world_moments(span.to,span.from,x.budget))<=0||take(compare_world_moments(span.to,due(f,x.budget),x.budget))>0||take(compare_world_moments(span.to,now,x.budget))>0)fail("ExactOfficeInvalidActualSpan",f.id);
 const auto& p=x.spatial;const auto& t=std::get<OfficeTerms>(f.original_terms);
 for(const auto* actor:participant_refs(f)){
  // This public span reader requires a real placement source, not the Office
  // acceptance header. Resolve it at the already verified original start cut.
  const auto accepted=take(read_actor_spatial_location_at_actual_fact_prefix(p.common,p.common_catalog,p.movement,p.carriers,p.registrations,p.foundation,p.actors,p.spatial,*actor,f.started_at,prefix(f,Endpoint::acceptance),x.handoffs,p.budget,p.family_catalog,p.births,p.military_actions));
  if(accepted.actor_id!=*actor||accepted.actual_location_sources.empty())fail("ExactOfficeAcceptedPlacementSourceRequired",*actor);
  const auto place=take(read_actor_stationary_location_span(p.common,p.common_catalog,p.movement,p.carriers,p.registrations,p.foundation,p.actors,p.spatial,*actor,accepted.actual_location_sources.front().source,span,x.handoffs,p.budget,p.family_catalog,p.births,p.military_actions));
  if(!at_place(place.actual_anchor,t.place_id))fail("ExactOfficeOriginalServicePlaceMismatch",*actor);
 }
 // No all-span health/contact rule. Elapsed units, ordinary Action coverage,
 // actual owner identity and release are checked by the existing S0 callers.
});}
Status NativeOfficeOriginalRead::validate_office_completed_result(const DynamicMilitaryActionFact2& input)const{return status([&]{
 const auto now=roots(context_);const auto& f=retained(context_,input);validate_completion_header(common(context_),context_.authority.actions,f);OfficeEndpointSources source(context_,f,Endpoint::completion,now);const auto& x=source.view();
 const auto& t=std::get<OfficeTerms>(f.original_terms);if(!same_moment(*f.completed_at,due(f,x.budget),x.budget)||!f.result||f.result->ok||!f.result->army_id.empty()||!f.result->order_id.empty()||f.result->office_id!=(f.success?t.office_id:StableId{}))fail("ExactOfficeCompletedResultMismatch",f.id);
 take(validate_military_action_endpoint_observation_source(x.conditions,f.id,Endpoint::completion,2));
 bool success=true;for(const auto* actor:participant_refs(f))if(!body_ready(x,f,Endpoint::completion,*actor)||battle(x,*actor,prefix(f,Endpoint::completion),*f.completed_at,now))success=false;
 const auto& d=definition(x,t);
 if(!raw_at_place(x,t.actor_id,t.place_id,prefix(f,Endpoint::completion),*f.completed_at)||holder(x,t.office_id,f,Endpoint::completion)||!friendly_garrison(x,f,Endpoint::completion,d,now)||!f.original_context)success=false;
 // Original completion calls contact even after ready or office failed. A
 // successfully proved false is business interruption; source errors propagate.
 if(f.original_context&&!contact(x,f,Endpoint::completion,now))success=false;
 if(success!=f.success)fail("ExactOfficeCompletedPredicateMismatch",f.id);
});}
}
