#include "native_formation_original.hpp"
#include "formation_exact_detail.hpp"
#include "military_action_original_rules.hpp"
#include "military_action_resources.hpp"
#include "military_formation_reservations.hpp"
#include "military_active_army_read.hpp"
#include "military_battle_lifecycle.hpp"
#include "military_return_detachment_codec.hpp"
#include "military_equal.hpp"
#include "occupation_time.hpp"
#include "naval_battle_history.hpp"
#include "military_roster_history.hpp"
#include "resident/domains/travel/military_action_placement.hpp"
#include "resident/domains/travel/movement_exact.hpp"
#include "resident/domains/personal/personal_battle_lifecycle.hpp"
#include "resident/domains/estate/estate_detachment_release.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
using namespace movement_detail;
template<class T>const T* original(const std::vector<T>& rows,const StableId& id){const T* out=nullptr;for(const auto& row:rows)if(row.id==id){if(out)fail("FormationOriginalIdentityRepeated",id);out=&row;}return out;}
bool contains(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
struct Endpoint {
 const MilitaryActionEndpointObservation2& observation;const WorldMoment& at;
 SourceRefBuildView source;std::size_t end;
};
// One synchronous endpoint window. Adapters borrow the original providers,
// never themselves; every nested genuine-M source keeps this outer fence even
// when proving a retained source that was published after the endpoint.
struct FormationEndpointSources {
 MilitaryActualSourceWindow window;
 NavalWindowedFormationSourceRead formations;
 NavalWindowedReturnSourceRead returns;
 std::optional<FamilyLifeCauseSources4> life;
 CharacterConditionSources body;
 MilitaryActionConditionContext conditions;
 MilitaryAuthoritySourceContext authority;
 std::optional<NavalOriginalSourceContext> sources;
 static CharacterConditionSources scoped_body(const CharacterConditionSources& source,
  std::optional<FamilyLifeCauseSources4>& life,const MilitaryFormationSourceRead& formations,
  const MilitaryReturnSourceRead& returns){
  auto result=source;result.military_history_order=MilitaryCasualtyHistoryOrder::actual_common_prefix;result.military_formations=&formations;result.military_returns=&returns;
  if(life){life->military_formations=&formations;life->military_returns=&returns;result.life_sources=&*life;}
  return result;
 }
 static ExactOfficeSourceReadContext scoped_office(const ExactOfficeSourceReadContext& source,
  const CharacterConditionSources& body){
  return {source.common,source.military,source.actions,source.elapsed,source.catalog,source.access,
   source.scale,source.clock,source.prepared,source.organization,source.family,body,source.budget,
   source.selected_action_time};
 }
 FormationEndpointSources(const NativeFormationOriginalContext& x,const Endpoint& e,const WorldMoment& current)
  :window{e.observation.common_prefix,e.at},
   formations(x.common,x.military,x.formations,x.returns,x.actions,x.catalog,x.access,x.budget,window,current),
   returns(x.common,x.military,x.formations,x.returns,x.actions,x.catalog,x.access,x.budget,window,current),
   life{x.conditions.sources.life_sources?std::optional<FamilyLifeCauseSources4>{*x.conditions.sources.life_sources}:std::nullopt},
   body(scoped_body(x.conditions.sources,life,formations,returns)),
   conditions{x.conditions.common,x.conditions.actions,x.conditions.foundation,body,x.conditions.clock},
   authority{x.authority.common,x.authority.military,x.authority.actions,x.authority.elapsed,
    scoped_office(x.authority.office,body),x.authority.selected_authority_time,&window},
   sources{}{
    if(x.naval){if(!x.exact)fail("FormationNavalDynamicRootRequired");sources.emplace(NavalOriginalSourceContext{x.common,x.military,*x.exact,x.actions,x.elapsed,authority,formations,returns,&x.periods,&window});}
   }
 FormationEndpointSources(const FormationEndpointSources&)=delete;
 FormationEndpointSources(FormationEndpointSources&&)=delete;
 FormationEndpointSources& operator=(const FormationEndpointSources&)=delete;
 FormationEndpointSources& operator=(FormationEndpointSources&&)=delete;
};
WorldMoment now(const NativeFormationOriginalContext& x){auto value=take(x.clock.read_current_moment());take(encode_world_moment(value,x.budget));if(value.half_day_tick<x.common.tick()||value.half_day_tick-x.common.tick()>1)fail("FormationOriginalClockMismatch");return value;}
std::size_t position(const NativeFormationOriginalContext& x,const SourceRefBuildView& ref){return take(read_actual_common_fact_position(x.common,ref));}
bool before(const NativeFormationOriginalContext& x,const Endpoint& endpoint,const SourceRefBuildView& ref,const WorldMoment* actual=nullptr){
 const auto order=position(x,ref);if(actual){const auto current=now(x);if(take(compare_world_moments(*actual,current,x.budget))>0)fail("FormationOriginalFutureSource",StableId(ref.id));const auto relation=take(compare_world_moments(*actual,endpoint.at,x.budget));if((order<endpoint.end&&relation>0)||(order>=endpoint.end&&relation<0))fail("FormationOriginalSourceMomentMismatch",StableId(ref.id));}return order<endpoint.end;
}
void raw_tick(const NativeFormationOriginalContext& x,Integer tick,const StableId& id){if(tick<0||tick>now(x).half_day_tick)fail("FormationOriginalLegacyTickMismatch",id);}
bool same_observation(const MilitaryActionEndpointObservation2& a,const MilitaryActionEndpointObservation2& b){
 if(a.common_prefix.common_fact_count!=b.common_prefix.common_fact_count||bool(a.common_prefix.last_common_fact)!=bool(b.common_prefix.last_common_fact)||(a.common_prefix.last_common_fact&&!same_source(*a.common_prefix.last_common_fact,*b.common_prefix.last_common_fact))||a.condition_prefix.applied_receipt_count!=b.condition_prefix.applied_receipt_count||bool(a.condition_prefix.last_applied_receipt)!=bool(b.condition_prefix.last_applied_receipt)||(a.condition_prefix.last_applied_receipt&&a.condition_prefix.last_applied_receipt->result_id!=b.condition_prefix.last_applied_receipt->result_id)||a.participant_heads.size()!=b.participant_heads.size())return false;
 for(std::size_t i=0;i<a.participant_heads.size();++i){const auto& l=a.participant_heads[i];const auto& r=b.participant_heads[i];if(l.actor_id!=r.actor_id||l.condition_revision!=r.condition_revision||l.initial_condition_seed_ref.seed_key!=r.initial_condition_seed_ref.seed_key||bool(l.latest_receipt_ref)!=bool(r.latest_receipt_ref)||(l.latest_receipt_ref&&l.latest_receipt_ref->result_id!=r.latest_receipt_ref->result_id))return false;}
 return true;
}
const ArmyFormationTerms& terms(const NativeFormationOriginalContext& x,const DynamicMilitaryActionFact2& f){
 const auto* retained=x.actions.dynamic_action_facts.find(f.id);const auto* t=std::get_if<ArmyFormationTerms>(&f.original_terms);
 if(!retained||retained->id!=f.id||!t||f.id.empty()||f.command!="army_form"||retained->command!=f.command||f.rule_version!=2||retained->rule_version!=2||f.actor_id!=t->actor_id||retained->actor_id!=f.actor_id||!same_terms(retained->original_terms,f.original_terms)||!same_moment(retained->started_at,f.started_at,x.budget)||f.original_context||retained->original_context)fail("FormationOriginalAcceptedIdentityMismatch",f.id);
 // The first hot caller passes its local Prepared row; identity is value/source,
 // never retained-address equality, and elapsed need not yet have been adopted.
 formation_exact_detail::header(x.common,f.id);
 if(f.phase!=retained->phase||f.success!=retained->success||f.elapsed_source_ref.action_ref!=f.id||retained->elapsed_source_ref.action_ref!=f.id||bool(f.result)!=bool(retained->result)||(f.result&&std::tie(f.result->ok,f.result->office_id,f.result->army_id,f.result->order_id)!=std::tie(retained->result->ok,retained->result->office_id,retained->result->army_id,retained->result->order_id)))fail("FormationOriginalRowValueMismatch",f.id);
 return *t;
}
Endpoint endpoint(const NativeFormationOriginalContext& x,const DynamicMilitaryActionFact2& f,MilitaryActionConditionEndpoint2 which){
 const auto* retained=x.actions.dynamic_action_facts.find(f.id);
 if(which==MilitaryActionConditionEndpoint2::acceptance){if(!f.acceptance_observation||!retained->acceptance_observation||!same_observation(*f.acceptance_observation,*retained->acceptance_observation))fail("FormationAcceptanceObservationRequired",f.id);const auto& saved=*retained->acceptance_observation;return {saved,retained->started_at,{"military","military_action",retained->id},take(read_actual_common_fact_prefix(x.common,saved.common_prefix))};}
 if(!f.completed_at||!retained->completed_at||!same_moment(*f.completed_at,*retained->completed_at,x.budget)||!f.completion_observation||!retained->completion_observation||!same_observation(*f.completion_observation,*retained->completion_observation)||!f.terminal_source_ref||!retained->terminal_source_ref||!same_source(*f.terminal_source_ref,*retained->terminal_source_ref)||f.success!=retained->success||f.phase!="completed"||retained->phase!="completed")fail("FormationCompletionObservationRequired",f.id);
 const auto& saved=*retained->completion_observation;const auto& source=*retained->terminal_source_ref;return {saved,*retained->completed_at,{source.domain,source.kind,source.id},take(read_actual_common_fact_prefix(x.common,saved.common_prefix))};
}
ActorSpatialLocationView location(const NativeFormationOriginalContext& x,const StableId& actor,const Endpoint& e){const auto& s=x.spatial;return take(read_actor_spatial_location_at_actual_fact_prefix(x.common,s.catalog,s.movement,s.carriers,s.registrations,s.foundation,s.actors,s.sources,actor,e.at,e.observation.common_prefix,s.military_handoffs,x.budget,s.family_catalog,s.births,&s.placements));}
bool at_place(const ActorSpatialLocationView& value,const StableId& place){const auto* local=std::get_if<SettlementLocation>(&value.location_snapshot);return local&&local->place_id==place;}
bool battle_free(const NativeFormationOriginalContext& x,const StableId& actor,const Endpoint& e,const WorldMoment& current,const FormationEndpointSources& scoped){
 // Literal BundleServices OR order. A genuine positive first branch does not
 // demand later domains; each entered reader still validates its full cut.
 if(x.selected_naval){const auto& n=*x.naval;if(take(naval_actor_held_at_actual_prefix(x.common,x.military,x.catalog,x.access,n.travel,n.state,n.catalog,n.history,n.exact,actor,e.observation.common_prefix,e.at)))return false;}
 if(x.selected_personal&&take(read_personal_battle_occupied_before(x.common,*x.personal,x.selected_personal_lifecycle,actor,e.observation.common_prefix,e.at,current,x.budget)).occupied)return false;
 return !take(read_selected_military_battle_occupied_before(x.common,x.military,x.identity,x.exact,x.selected_land_lifecycle,actor,e.observation.common_prefix,e.at,current,&scoped.formations,&scoped.returns,x.budget,x.field_history)).occupied;
}
const GarrisonDefinition& camp(const NativeFormationOriginalContext& x,const ArmyFormationTerms& t){
 const auto* c=original(x.catalog.garrisons,t.garrison_id);const auto* o=original(x.catalog.offices,t.office_id);
 if(!c||!o||o->garrison_id!=c->id||o->organization_id!=t.organization_id||c->place_id!=t.place_id||c->inventory_id!=t.source_inventory_id||c->wallet_id!=t.source_wallet_id||t.unit_ids.empty()||t.commanders.size()!=t.unit_ids.size()||(o->command_scope=="unit"&&t.unit_ids.size()!=1))fail("ExactFormationOriginalTermsMismatch",t.order_id);
 return *c;
}
bool office_ready(const NativeFormationOriginalContext& x,const ArmyFormationTerms& t,const Endpoint& e,bool acceptance,const FormationEndpointSources& scoped){
 const SourceRef source{StableId(e.source.domain),StableId(e.source.kind),StableId(e.source.id)};
 // The complete participant body kernel has already checked this actor/head.
 // Office authority separately short-circuits only alive/available, not hp or
 // injury, before reading holder and finally current-at-cut garrison ownership.
 const auto available=take(read_character_availability_before_fact_provenance_v2(scoped.body,t.actor_id,source,e.at,x.clock));
 if(!available.present)fail("FormationOriginalParticipantSourceRequired",t.actor_id);
 if(!available.alive||!available.available)return false;
 const auto authority=take(read_military_office_authority_before(scoped.authority,t.office_id,source,e.at));
 if(!authority.appointment)return false;
 const auto* office=std::get_if<OfficeTerms>(&authority.appointment->original_terms);if(!office)fail("FormationOriginalOfficeTermsRequired",t.office_id);
 if(office->actor_id!=t.actor_id||office->office_id!=t.office_id||office->organization_id!=t.organization_id)return false;
 const auto ownership=take(read_military_garrison_owner_before_actual(x.common,x.military,x.catalog,x.access,t.garrison_id,e.source,e.at,now(x),x.access.occupation_source_schema,x.budget,&scoped.window));
 if(acceptance){const StableId empty;const auto& actual=ownership.actual_occupation?ownership.actual_occupation->id:empty;if(actual!=t.ownership_id)fail("ExactFormationAcceptedOwnershipMismatch",t.order_id);}
 return ownership.organization_id()==t.organization_id;
}
// Only the original accepted-and-not-yet-consumed predicate is reconstructed.
// Later marching/reporting states are not a new Campaign state machine here.
bool campaign_musterable(const NativeFormationOriginalContext& x,const ArmyFormationTerms& t,const Endpoint& e,const FormationEndpointSources& scoped){
 const auto* p=original(x.military.campaigns,t.order_id);if(!p||p->actor_id!=t.actor_id||p->office_id!=t.office_id||p->garrison_id!=t.garrison_id||p->issuer_organization_id!=t.organization_id||!p->exact_acceptance)fail("FormationOriginalCampaignAcceptanceRequired",t.order_id);
 const auto& accepted=*p->exact_acceptance;const auto* verified=take(read_military_campaign_acceptance_source(scoped.authority,accepted.actual_acceptance_source,1));if(verified!=p)fail("FormationOriginalCampaignIdentityMismatch",t.order_id);
 const auto& acceptance=accepted.actual_acceptance_source;const auto accepted_pos=position(x,{acceptance.domain,acceptance.kind,acceptance.id});if(!before(x,e,{acceptance.domain,acceptance.kind,acceptance.id},&accepted.accepted_at))fail("FormationCampaignNotYetAccepted",p->id);
 bool consumed=false;std::set<StableId> settlements;
 for(const auto& army:x.military.armies){const auto formed=take(read_original_military_formation(x.common.state(),x.military,army,now(x),&scoped.formations,x.budget));if(!formed.original_terms)fail("FormationOriginalArmyTermsRequired",army.id);if(formed.original_terms->order_id!=p->id)continue;if(formed.completed_source.source_schema!=2)fail("MilitaryActualFormationCompletionRequired",army.id);const auto& ref=formed.completed_source.source_ref;if(position(x,{ref.domain,ref.kind,ref.id})<=accepted_pos)fail("FormationCampaignPredecessorMismatch",army.id);if(before(x,e,{ref.domain,ref.kind,ref.id},&formed.completed_at))consumed=true;}
 for(const auto& f:x.military.settlement_facts){if(f.campaign_id!=p->id)continue;raw_tick(x,f.tick,f.id);if(f.actor_id!=p->actor_id||!settlements.insert(f.id).second||position(x,{"military","campaign_settlement",f.id})<=accepted_pos)fail("FormationCampaignSettlementMismatch",f.id);if(before(x,e,{"military","campaign_settlement",f.id}))consumed=true;}
 for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="military"&&row.second.kind=="campaign_settlement"){const auto* f=original(x.military.settlement_facts,row.first);if(!f||f->id!=row.first)fail("FormationCampaignSettlementHistoryRequired",row.first);}
 return !consumed;
}

// No present detached slot is used as an initial value. Native authored Estate
// starts with no detachments; actual set/return commands and lost-return effects
// are the only original writers of this slot. Their full business admission is
// still owned by Estate/Return, rather than re-entered from this source reader.
void muster_source(const NativeFormationOriginalContext& x,const StableId& source,
 const StableId& estate,const StableId& person,const StableId* order=nullptr){
 const auto* h=x.common.state().facts.find(source);if(!h||h->id!=source)fail("FormationRetainerMusterSourceRequired",source);
 if(h->domain=="estate"&&h->kind=="estate_muster"){
  if(order)fail("FormationMilitaryRetainerMusterRequired",source);
  const auto* payload=x.estates.facts.find(source);const auto* origin=payload?std::get_if<EstateMusterSource>(payload):nullptr;
  if(!origin||origin->kind!="estate_muster"||origin->estate_id!=estate||!x.estate_catalog.estates.find(estate))fail("FormationRetainerMusterSourceMismatch",source);
 }else if(h->domain=="military"&&h->kind=="military_muster"){
  const auto* origin=original(x.military.muster_facts,source);if(!origin||origin->estate_id!=estate||!x.common.actor(origin->actor_id)||!contains(origin->actor_ids,person)||(order&&origin->order_id!=*order))fail("FormationRetainerMusterSourceMismatch",source);
  raw_tick(x,origin->tick,origin->id);std::set<StableId> people;for(const auto& id:origin->actor_ids)if(!x.common.actor(id)||!people.insert(id).second)fail("FormationRetainerMusterPeopleMismatch",source);
 }else fail("FormationRetainerMusterSourceRequired",source);
}
const MilitaryLogisticsFact& legacy_return(const NativeFormationOriginalContext& x,const ArmyRecord& army,const MilitaryReturnFact& returned){
 const MilitaryLogisticsFact* action=nullptr;
 for(const auto& candidate:x.military.logistics_facts)if(candidate.result&&candidate.result->receipt_id==returned.id){if(action)fail("FormationRetainerReturnAmbiguous",returned.id);action=&candidate;}
 if(!action||action->command!="army_return"||action->phase!="completed"||!action->success||!action->result->ok.value_or(false)||action->terms.army_id!=army.id||action->result->army_id!=army.id||action->terms.garrison_id!=returned.garrison_id||action->terms.source_garrison_id!=returned.source_garrison_id||action->actor_id!=returned.actor_id||action->result->order_id!=returned.order_id||army.order_id!=returned.order_id||action->completed_tick!=returned.tick||action->start_tick==INT64_MAX||returned.tick!=action->start_tick+1)fail("FormationRetainerReturnRequired",returned.id);
 raw_tick(x,returned.tick,returned.id);if(position(x,{"military","military_action",action->id})>=position(x,{"military","military_return",returned.id}))fail("FormationRetainerReturnOrderMismatch",returned.id);return *action;
}
struct DetachmentEvent {
 std::size_t position=0;const EstateDetachment* command=nullptr;
 const RetainerRecord* lost=nullptr;const MilitaryReturnDetachmentRelease1* saved=nullptr;
};
const StableId* detached_source(const NativeFormationOriginalContext& x,const StableId& estate_id,
 const StableId& actor_id,const Endpoint& endpoint,const FormationEndpointSources& scoped){
 const auto* estate=x.estates.estates.find(estate_id);const auto* initial=x.estate_catalog.estates.find(estate_id);
 if(!estate||estate->id!=estate_id||!initial||initial->id!=estate_id||!contains(initial->resident_actor_ids,actor_id)||!contains(estate->resident_actor_ids,actor_id))fail("PrivateRetainerConsentRequired",actor_id);
 std::vector<DetachmentEvent> events;
 for(const auto& row:x.estates.facts.rows()){
  const auto* fact=std::get_if<EstateManagementFact>(&row.second);if(!fact||fact->kind!="estate_detachment")continue;
  const auto* command=std::get_if<EstateDetachment>(&fact->command);
  if(!command||fact->cash||fact->transfer||command->estate_id!=fact->estate_id||!x.estate_catalog.estates.find(fact->estate_id)||!x.common.actor(fact->actor_id)||command->actor_ids.empty())fail("FormationEstateDetachmentSourceMismatch",row.first);
  const auto order=position(x,{"estate","estate_detachment",row.first});raw_tick(x,fact->tick,row.first);
  std::set<StableId> people;for(const auto& id:command->actor_ids)if(!x.common.actor(id)||!people.insert(id).second)fail("FormationEstateDetachmentSourceMismatch",row.first);
  if(command->estate_id!=estate_id||!contains(command->actor_ids,actor_id))continue;
  muster_source(x,command->source_id,command->estate_id,actor_id);
  const auto* source=x.common.state().facts.find(command->source_id);
  if(!source||position(x,{source->domain,source->kind,source->id})>=order)fail("FormationRetainerMusterOrderMismatch",row.first);
  events.push_back({order,command,nullptr,nullptr});
 }
 for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="estate"&&row.second.kind=="estate_detachment"){
  const auto* payload=x.estates.facts.find(row.first);const auto* fact=payload?std::get_if<EstateManagementFact>(payload):nullptr;
  if(!fact||fact->kind!="estate_detachment")fail("FormationEstateDetachmentHistoryRequired",row.first);
 }
 take(validate_military_return_detachment_sources(x.common,x.actions,x.selected_return_detachments));
 for(const auto& army:x.military.armies){
  const auto formed=take(read_original_military_formation(x.common.state(),x.military,army,now(x),&scoped.formations,x.budget));
  if(!formed.original_terms||formed.completed_source.source_schema!=2||!same_retainers(army.retainers,formed.original_terms->retainers))fail("FormationRetainerArmySourceRequired",army.id);
  const auto& source=formed.completed_source.source_ref;const auto formation_position=position(x,{source.domain,source.kind,source.id});
  before(x,endpoint,{source.domain,source.kind,source.id},&formed.completed_at);
  const RetainerRecord* pledge=nullptr;
  for(const auto& old:formed.original_terms->retainers)if(old.actor_id==actor_id){if(pledge)fail("FormationRetainerPledgeRepeated",army.id);pledge=&old;}
  if(!pledge||pledge->estate_id!=estate_id)continue;
  const auto exact=take(scoped.returns.read_exact_army_return(army.id));
  const MilitaryReturnFact* old_return=nullptr;for(const auto& row:x.military.return_facts)if(row.army_id==army.id){if(old_return)fail("FormationRetainerReturnAmbiguous",army.id);old_return=&row;}
  if(exact&&old_return)fail("FormationRetainerReturnAmbiguous",army.id);
  if(exact){
   const auto* row=exact->original;if(!row||row->army_id!=army.id||exact->actual_return.source_schema!=2||exact->actual_application.source_schema!=2||exact->actual_return.source_ref.domain!="military"||exact->actual_return.source_ref.kind!="military_return"||exact->actual_return.source_ref.id!=row->id||!same_source(exact->actual_application.source_ref,row->completion_source_ref))fail("FormationRetainerReturnRequired",army.id);
   const auto receipt=position(x,{"military","military_return",row->id});const auto& applied=exact->actual_application.source_ref;const auto application=position(x,{applied.domain,applied.kind,applied.id});
   if(formation_position>=receipt||receipt>=application||(receipt<endpoint.end)!=(application<endpoint.end))fail("FormationRetainerReturnOrderMismatch",row->id);
   before(x,endpoint,{applied.domain,applied.kind,applied.id},&row->completed_at);
   if(!contains(row->lost_retainers,actor_id))continue;
   if(x.selected_return_detachments==0){
    // Original /0 has no saved before/after adjunct. The same conditional
    // erase is reconstructed below from the immutable pledge and real return.
    events.push_back({application,nullptr,pledge,nullptr});continue;
   }
   if(!row->actual_detachment_releases)fail("FormationRetainerReleaseWitnessRequired",row->id);
   const auto& releases=row->actual_detachment_releases->releases;
   if(releases.size()!=row->lost_retainers.size())fail("FormationRetainerReleaseCoverageMismatch",row->id);
   CheckpointVector<const RetainerRecord*> ordered;ordered.reserve(formed.original_terms->retainers.size());
   for(const auto& old:formed.original_terms->retainers)ordered.push_back(&old);
   std::sort(ordered.begin(),ordered.end(),[](const RetainerRecord* a,const RetainerRecord* b){return a->actor_id<b->actor_id;});
   std::set<StableId> losses;std::size_t index=0;
   for(const auto* original:ordered){const auto& old=*original;if(!contains(row->lost_retainers,old.actor_id))continue;
    if(index>=releases.size()||!losses.insert(old.actor_id).second)fail("FormationRetainerReleaseCoverageMismatch",row->id);
    const auto& release=releases[index++];if(release.actor_id!=old.actor_id||release.estate_id!=old.estate_id||release.source_id!=old.source_id)fail("FormationRetainerReleasePledgeMismatch",row->id);
    take(validate_original_lost_estate_retainer_release({release.estate_id,release.actor_id,release.source_id,release.detached_before?&*release.detached_before:nullptr,release.detached_after?&*release.detached_after:nullptr}));
    if(old.actor_id==actor_id&&old.estate_id==estate_id)events.push_back({application,nullptr,&old,&release});
   }
   if(index!=releases.size())fail("FormationRetainerReleaseCoverageMismatch",row->id);
  }else if(old_return){
   legacy_return(x,army,*old_return);const auto returned=position(x,{"military","military_return",old_return->id});
   if(formation_position>=returned)fail("FormationRetainerReturnOrderMismatch",old_return->id);
   if(pledge&&pledge->estate_id==estate_id&&contains(old_return->lost_retainers,actor_id))events.push_back({returned,nullptr,pledge,nullptr});
  }
 }
 std::sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.position<b.position;});
 const StableId* current=nullptr;const StableId* captured=nullptr;std::optional<std::size_t> previous;
 for(const auto& event:events){
  if(previous&&*previous==event.position)fail("FormationRetainerDetachmentEventRepeated",actor_id);previous=event.position;
  if(event.command){if(event.command->returning){if(!current||*current!=event.command->source_id)fail("EstateDetachmentMismatch",actor_id);current=nullptr;}else{if(current)fail("EstateAlreadyDetached",actor_id);current=&event.command->source_id;}}
  else{
   if(!event.lost)fail("FormationRetainerReleaseRequired",actor_id);
   if(event.saved&&(bool(current)!=bool(event.saved->detached_before)||(current&&*current!=*event.saved->detached_before)))fail("FormationRetainerReleaseBeforeMismatch",actor_id);
   const auto* after=current&&*current!=event.lost->source_id?current:nullptr;
   take(validate_original_lost_estate_retainer_release({event.lost->estate_id,event.lost->actor_id,event.lost->source_id,current,after}));
   if(event.saved&&(bool(after)!=bool(event.saved->detached_after)||(after&&*after!=*event.saved->detached_after)))fail("FormationRetainerReleaseAfterMismatch",actor_id);
   current=after;
  }
  if(event.position<endpoint.end)captured=current;
 }
 return captured;
}
void retainer(const NativeFormationOriginalContext& x,const ArmyFormationTerms& terms,
 const RetainerRecord& wanted,const Endpoint& endpoint,const FormationEndpointSources& scoped){
 bool matched=false;
 // The original Native access scans the same Estate row order and selects
 // the first detached military_muster for this actor and this Campaign.
 for(const auto& row:x.estates.estates.rows()){
  const auto* definition=x.estate_catalog.estates.find(row.first);
  if(!definition||row.second.id!=row.first)fail("FormationEstateOriginalRequired",row.first);
  if(!contains(definition->resident_actor_ids,wanted.actor_id))continue;
  const auto* source=detached_source(x,row.first,wanted.actor_id,endpoint,scoped);if(!source)continue;
  const auto* header=x.common.state().facts.find(*source);if(!header||header->kind!="military_muster")continue;
  const auto* muster=original(x.military.muster_facts,*source);
  if(!muster||muster->estate_id!=row.first||muster->order_id!=terms.order_id||!contains(muster->actor_ids,wanted.actor_id))continue;
  muster_source(x,*source,row.first,wanted.actor_id,&terms.order_id);
  if(row.first!=wanted.estate_id||*source!=wanted.source_id)fail("FormationRetainerOriginalSelectionMismatch",wanted.actor_id);
  matched=true;break;
 }
 if(!matched)fail("PrivateRetainerConsentRequired",wanted.actor_id);
 // Pledge reuse is forbidden even after the old Army closes. A later Army,
 // including this action's own completion, is not yet present at acceptance.
 for(const auto& army:x.military.armies){
  const auto formed=take(read_original_military_formation(x.common.state(),x.military,army,now(x),&scoped.formations,x.budget));
  if(!formed.original_terms||formed.completed_source.source_schema!=2||!same_retainers(army.retainers,formed.original_terms->retainers))fail("FormationRetainerArmySourceRequired",army.id);
  const auto& source=formed.completed_source.source_ref;const bool prior=before(x,endpoint,{source.domain,source.kind,source.id},&formed.completed_at);
  for(const auto& old:formed.original_terms->retainers)if(prior&&old.actor_id==wanted.actor_id&&old.source_id==wanted.source_id)fail("RetainerPledgeAlreadyUsed",wanted.actor_id);
 }
}
void frozen_people(const NativeFormationOriginalContext& x,const ArmyFormationTerms& t){
 if(t.member_actor_ids.empty()||t.member_actor_ids.front()!=t.actor_id||t.unit_ids.empty()||t.commanders.size()!=t.unit_ids.size())fail("FormationOriginalParticipantCoverage",t.order_id);
 std::set<StableId> units,people;std::vector<StableId> expected;
 for(std::size_t i=0;i<t.unit_ids.size();++i){const auto& commander=t.commanders[i];if(t.unit_ids[i].empty()||!units.insert(t.unit_ids[i]).second||commander.first!=t.unit_ids[i]||!x.common.actor(commander.second)||!people.insert(commander.second).second||(i==0&&commander.second!=t.actor_id))fail("FormationOriginalCommanderMismatch",t.order_id);expected.push_back(commander.second);}
 for(const auto& r:t.retainers){if(!x.common.actor(r.actor_id)||!people.insert(r.actor_id).second||r.estate_id.empty()||r.source_id.empty()||r.unit_id!=t.unit_ids.front())fail("FormationOriginalRetainerMismatch",t.order_id);expected.push_back(r.actor_id);}
 if(expected!=t.member_actor_ids)fail("FormationOriginalParticipantCoverage",t.order_id);
}
void equipment(const NativeFormationOriginalContext& x,const ArmyFormationTerms& t,
 const DynamicMilitaryActionFact2& f,const Endpoint& e,const GarrisonDefinition& camp,const FormationEndpointSources& scoped){
 const auto roster=take(read_military_rosters_at_selected_actual_prefix(x.common,x.military,x.catalog,x.identity,x.exact,t.unit_ids,e.observation.common_prefix,now(x),x.population,x.population_catalog,x.budget,&scoped.formations,&scoped.returns,&x.periods,&scoped.window,x.field_history));
 if(roster.units.size()!=t.unit_ids.size())fail("FormationOriginalRosterCoverage",f.id);
 std::vector<std::pair<StableId,Integer>> expected;Integer mouths=Integer(t.member_actor_ids.size());
 for(std::size_t i=0;i<t.unit_ids.size();++i){const auto* unit=original(roster.units,t.unit_ids[i]);if(!unit||unit->garrison_id!=camp.id||unit->organization_id!=t.organization_id||!unit->army_id.empty()||(!unit->mobilizing_action_id.empty()&&unit->mobilizing_action_id!=f.id)||(unit->training_action_id&&!unit->training_action_id->empty())||unit->fit.empty()||(i&&unit->commander_actor_id!=t.commanders[i].second))fail("GarrisonUnitUnavailable",t.unit_ids[i]);mouths+=take(append_original_formation_unit_equipment(x.catalog,*unit,i?0:Integer(t.retainers.size()),expected));}
 std::set<StableId> goods;Integer recorded_food=0,equipment_food=0;
 for(const auto& good:t.goods){if(good.first.empty()||good.second<0||!goods.insert(good.first).second||!x.spatial.catalog.items.find(good.first))fail("FormationOriginalGoodsMismatch",f.id);if(good.first==camp.food_item_id)recorded_food=good.second;}
 for(const auto& good:expected)if(good.first==camp.food_item_id)equipment_food=good.second;
 if(recorded_food<equipment_food)fail("FormationOriginalEquipmentMismatch",f.id);
 // The request may choose any original legal amount. The frozen aggregate
 // proves that amount after removing equipment; it must not be replaced by
 // today's default mouths*ration*2 or forced to the historical default.
 const Integer food=take(read_original_formation_food_quantity(camp,mouths,std::optional<Integer>{recorded_food-equipment_food},true,t.money,true));
 if(food>0)append_original_formation_good(expected,camp.food_item_id,food);
 if(!same_dictionary(expected,t.goods))fail("FormationOriginalEquipmentMismatch",f.id);
 const auto* retained=x.actions.dynamic_action_facts.find(f.id);if(!retained)fail("FormationOriginalAcceptedIdentityMismatch",f.id);
 take(validate_original_formation_acceptance_reservations(x.catalog,*retained,x.access.military_formation_reservations_schema));
 const auto& reservations=*retained->acceptance_resource_reservations;
 const auto& opening=reservations.empty_inventory_opening?*reservations.empty_inventory_opening:reservations.inventory_reservations.front().before;
 const Owner owner{Owner::Kind::organization,t.organization_id,{}};const StableId empty;
 if(take(count_inventory_frame_items(opening,camp.food_item_id,{owner},ReservationRef{empty}))<food)fail("InsufficientMilitarySupply",f.id);
 Integer weight=0;
 for(const auto& good:t.goods){if(take(count_inventory_frame_items(opening,good.first,{owner},ReservationRef{empty}))<good.second)fail("InsufficientMilitaryEquipmentOrFood",f.id);const auto* item=x.spatial.catalog.items.find(good.first);if(!item)fail("FormationOriginalItemRequired",good.first);weight=original_formation_goods_weight_after(weight,good.second,item->weight);}
 if(weight>camp.army_capacity)fail("ArmyCapacityExceeded",f.id);
}
bool original_endpoint_ready(const NativeFormationOriginalContext& x,const DynamicMilitaryActionFact2& f,const ArmyFormationTerms& t,const Endpoint& e,MilitaryActionConditionEndpoint2 which,const FormationEndpointSources& scoped){
 bool ready=true;const auto current=now(x);
 for(const auto& actor:t.member_actor_ids){
  const bool body=take(read_military_action_endpoint_participant_body_ready(scoped.conditions,f.id,which,actor,2));
  if(!body||!battle_free(x,actor,e,current,scoped))ready=false;
 }
 // Original completion checks the campaign first and invokes Office only if
 // it is still musterable. A missing true source is never a business false.
 const bool campaign=campaign_musterable(x,t,e,scoped);
 if(!campaign||!office_ready(x,t,e,which==MilitaryActionConditionEndpoint2::acceptance,scoped))ready=false;
 // Placement is a separate original loop, even after a readiness failure.
 for(const auto& actor:t.member_actor_ids)if(!at_place(location(x,actor,e),t.place_id))ready=false;
 return ready;
}
}
bool NativeFormationOriginalRead::bound_to(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& a)const noexcept{return &c==&context_.common&&&m==&context_.military&&&a==&context_.actions;}
Status NativeFormationOriginalRead::validate_bindings()const{return status([&]{
 const auto& x=context_;const auto current=now(x);
 take(validate_selected_military_roster_root(x.common,x.identity,x.exact));take(validate_selected_military_field_history(x.common,x.military,x.identity,x.field_history));
 if(!x.formations.bound_to(x.common.state(),x.military)||!x.returns.bound_to(x.common,x.military)||!x.social.bound_to(x.common,x.interaction)||!x.spatial.placements.bound_to(x.common,x.spatial.movement))fail("CrossCandidateFormationOriginalSources");
 const auto& body=x.conditions.sources;
 if(&x.conditions.common!=&x.common||&x.conditions.actions!=&x.actions||&x.conditions.clock!=&x.clock||&x.conditions.foundation!=&x.spatial.foundation||&body.common!=&x.common||&body.state!=&x.spatial.foundation||&body.catalog!=&x.spatial.actors||!same_moment(body.actual_now,current,x.budget)||body.shared!=&x.spatial.catalog||body.elapsed!=&x.elapsed||body.scale!=&x.scale||body.movement!=&x.spatial.movement||body.military!=&x.military||body.military_exact!=x.exact||body.military_supply_periods!=&x.periods||body.military_formations!=&x.formations||body.military_returns!=&x.returns||body.interaction!=&x.interaction||body.interaction_catalog!=x.access.interaction_catalog)fail("CrossCandidateFormationConditionSources");
 // 生命来源须借用同一精确军事实根，不能比较指针成员自身的地址。
 if (body.life_sources) {
   const auto &life = *body.life_sources;
   if (&life.military != &x.military || life.military_exact != x.exact ||
       life.military_formations != &x.formations ||
       life.military_returns != &x.returns)
     fail("CrossCandidateFormationLifeSources");
 }
 const auto& office=x.authority.office;
 if(&x.authority.common!=&x.common||&x.authority.military!=&x.military||&x.authority.actions!=&x.actions||&x.authority.elapsed!=&x.elapsed||&office.catalog!=&x.catalog||&office.access!=&x.access||&office.scale!=&x.scale||&office.clock!=&x.clock||&office.body!=&body)fail("CrossCandidateFormationAuthoritySources");
 take(validate_military_authority_source_context(x.authority));
 if(x.access.common_catalog!=&x.spatial.catalog||x.access.interaction!=&x.interaction||!x.access.interaction_catalog||x.access.military_action_conditions_schema!=2||x.access.military_formation_reservations_schema!=2||x.access.military_action_resources_schema!=1||x.authority.selected_authority_time!=1||x.selected_land_lifecycle!=1||x.access.military_battle_lifecycle_schema!=x.selected_land_lifecycle||x.access.military_return_detachments_schema!=x.selected_return_detachments||(x.selected_return_detachments!=0&&x.selected_return_detachments!=1))fail("FormationOriginalSelectedSourcesRequired");
 if(!x.access.action_conditions||&x.access.action_conditions->common!=&x.common||&x.access.action_conditions->actions!=&x.actions||&x.access.action_conditions->foundation!=&x.spatial.foundation||&x.access.action_conditions->sources!=&body||&x.access.action_conditions->clock!=&x.clock)fail("CrossCandidateFormationConditionAccess");
 if((x.selected_personal!=0&&x.selected_personal!=1)||bool(x.selected_personal)!=bool(x.personal)||(x.selected_personal?x.selected_personal_lifecycle!=1:x.selected_personal_lifecycle!=0)||x.selected_naval<0||bool(x.selected_naval)!=bool(x.naval))fail("FormationOriginalBattleSourcesRequired");
 if(x.naval){
  const auto& n=*x.naval;const auto& repair=n.exact.repair_sources;
  if(&n.exact.military_actions!=&x.actions||&n.exact.elapsed!=&x.elapsed||x.access.naval_state!=&n.state||x.access.travel_state!=&n.travel||&repair.military_catalog!=&x.catalog||&repair.access!=&x.access||&repair.naval_catalog!=&n.catalog||&repair.scale!=&x.scale||&repair.clock!=&x.clock||!n.exact.original_sources||n.history.original_sources!=n.exact.original_sources)fail("CrossCandidateFormationNavalSources");
  const auto& original=*n.exact.original_sources;
  take(validate_naval_original_source_context(original,x.common,x.military,x.catalog,x.access));
  if(&original.exact!=x.exact||&original.actions!=&x.actions||&original.elapsed!=&x.elapsed||&original.formations!=&x.formations||&original.returns!=&x.returns||original.periods!=&x.periods||&original.authority.office.body!=&body||&original.authority.office.clock!=&x.clock||&original.authority.office.scale!=&x.scale)fail("CrossCandidateFormationNavalSources");
 }
});}
Status NativeFormationOriginalRead::validate_formation_acceptance(const DynamicMilitaryActionFact2& f)const{return status([&]{
 take(validate_bindings());const auto& x=context_;const auto& t=terms(x,f);frozen_people(x,t);const auto& original_camp=camp(x,t);const auto e=endpoint(x,f,MilitaryActionConditionEndpoint2::acceptance);
 const auto current=now(x);const FormationEndpointSources scoped{x,e,current};
 take(validate_military_action_endpoint_observation_source(scoped.conditions,f.id,MilitaryActionConditionEndpoint2::acceptance,2));
 if(!original_endpoint_ready(x,f,t,e,MilitaryActionConditionEndpoint2::acceptance,scoped))fail("FormationOriginalAcceptanceRejected",f.id);
 const ExactFormationReadContext finite{x.catalog,x.access,x.scale,*this,x.periods,x.clock,x.budget};
 take(validate_exact_formation_unit_activity_acceptance(x.common,x.military,x.actions,x.elapsed,finite,f));
 const SourceRef boundary{StableId(e.source.domain),StableId(e.source.kind),StableId(e.source.id)};
 for(const auto& actor:t.member_actor_ids){
  const auto active=take(read_actor_active_armies_at_selected_actual_prefix(x.common,x.military,x.identity,x.exact,x.naval?&x.naval->state:nullptr,actor,e.observation.common_prefix,e.at,current,x.budget,&scoped.formations,&scoped.returns,scoped.sources?&*scoped.sources:nullptr,x.field_history));
  if(!active.active_army_ids.empty())fail("MilitaryAssemblyRequired",actor);
  const auto social=take(x.social.read_actor_social_occupancy_before_source(actor,boundary,e.at));if(social.value.party_id)fail("MilitaryAssemblyRequired",actor);
 }
 for(const auto& pledge:t.retainers)retainer(x,t,pledge,e,scoped);
 equipment(x,t,f,e,original_camp,scoped);
 // Ordinary Common Action mutual exclusion and exact S0 owner/reverse coverage
 // remain mandatory in the enclosing original start/whole caller. This source
 // reader never invokes S0's full validator or substitutes today's action_id.
});}
Status NativeFormationOriginalRead::validate_formation_actual_service(const DynamicMilitaryActionFact2& f,const MomentSpan& span)const{return status([&]{
 take(validate_bindings());const auto& x=context_;const auto& t=terms(x,f);frozen_people(x,t);camp(x,t);const auto e=endpoint(x,f,MilitaryActionConditionEndpoint2::acceptance);
 take(make_moment_span(span.from,span.to,x.budget));if(take(compare_world_moments(span.from,span.to,x.budget))>=0||take(compare_world_moments(span.from,f.started_at,x.budget))<0||take(compare_world_moments(span.to,formation_exact_detail::due(f,x.budget),x.budget))>0||take(compare_world_moments(span.to,now(x),x.budget))>0||(f.completed_at&&take(compare_world_moments(span.to,*f.completed_at,x.budget))>0))fail("FormationOriginalServiceSpanMismatch",f.id);
 const auto current=now(x);const FormationEndpointSources scoped{x,e,current};
 take(validate_military_action_endpoint_observation_source(scoped.conditions,f.id,MilitaryActionConditionEndpoint2::acceptance,2));
 const auto& s=x.spatial;
 for(const auto& actor:t.member_actor_ids){
  const auto accepted=location(x,actor,e);if(!at_place(accepted,t.place_id)||accepted.actual_location_sources.size()!=1)fail("FormationOriginalAcceptedPlacementRequired",actor);
  const auto& source=accepted.actual_location_sources.front().source;
  const auto actual=take(read_actor_stationary_location_span(x.common,s.catalog,s.movement,s.carriers,s.registrations,s.foundation,s.actors,s.sources,actor,source,span,s.military_handoffs,x.budget,s.family_catalog,s.births,&s.placements));
  const auto projected=take(project_movement_location(actual.actual_anchor,x.budget));const auto* place=std::get_if<SettlementLocation>(&projected);
  if(actual.actor_id!=actor||!same_moment(actual.span.from,span.from,x.budget)||!same_moment(actual.span.to,span.to,x.budget)||!place||place->place_id!=t.place_id)fail("FormationOriginalActualPlacementMismatch",actor);
 }
 // Named participants and their real stationary service only. Body readiness,
 // party, equipment, command and garrison friendliness are original endpoint
 // predicates, not newly invented continuous-health/authorization requirements.
});}
Status NativeFormationOriginalRead::validate_formation_completed_result(const DynamicMilitaryActionFact2& f)const{return status([&]{
 take(validate_bindings());const auto& x=context_;const auto& t=terms(x,f);frozen_people(x,t);camp(x,t);formation_exact_detail::validate_completion_header(x.common,x.actions,f);const auto e=endpoint(x,f,MilitaryActionConditionEndpoint2::completion);
 if(!same_moment(e.at,formation_exact_detail::due(f,x.budget),x.budget)||!f.result||!f.result->office_id.empty())fail("ExactFormationCompletedResultMismatch",f.id);
 const auto service=take(f.success?read_fixed_service_completion(x.elapsed,f.elapsed_source_ref,now(x),x.scale,x.budget):read_fixed_service_completed_recheck_interruption(x.elapsed,f.elapsed_source_ref,now(x),x.scale,x.budget));
 if(!service.actual_service.complete||!same_moment(service.started_at,f.started_at,x.budget)||!same_moment(service.completed_at,e.at,x.budget))fail("ExactFormationFullActualServiceRequired",f.id);
 const auto current=now(x);const FormationEndpointSources scoped{x,e,current};
 take(validate_military_action_endpoint_observation_source(scoped.conditions,f.id,MilitaryActionConditionEndpoint2::completion,2));
 const bool succeeded=original_endpoint_ready(x,f,t,e,MilitaryActionConditionEndpoint2::completion,scoped);
 if(succeeded!=f.success)fail("FormationOriginalCompletionPredicateMismatch",f.id);
 const auto* retained=x.actions.dynamic_action_facts.find(f.id);if(!retained)fail("FormationOriginalAcceptedIdentityMismatch",f.id);
 take(validate_original_formation_resource_result(x.military,x.catalog,*retained,x.access.military_action_resources_schema));
 if(f.success){
  if(f.result->ok!=std::optional<bool>{true}||f.result->army_id.empty()||f.result->order_id!=t.order_id)fail("ExactFormationCompletedResultMismatch",f.id);
  const auto* army=original(x.military.armies,f.result->army_id);if(!army||army->formation_source!=f.id)fail("FormationOriginalArmyResultRequired",f.id);
  const auto formed=take(scoped.formations.read_exact_army_formation(army->id));if(!formed||!formed->original_terms||!same_terms(*formed->original_terms,f.original_terms)||formed->completed_source.source_schema!=2||!same_source(formed->completed_source.source_ref,*f.terminal_source_ref)||!same_moment(formed->completed_at,e.at,x.budget))fail("FormationOriginalArmyResultRequired",f.id);
  const ExactFormationReadContext finite{x.catalog,x.access,x.scale,*this,x.periods,x.clock,x.budget};const auto period=take(read_exact_formation_supply_period(finite,e.at));const auto& source=period.original_opening.source_ref;
  if(position(x,{source.domain,source.kind,source.id})>=e.end)fail("ExactFormationCompletionPeriodMismatch",period.period_ref);
 }else{
  if(f.result->ok||!f.result->army_id.empty()||!f.result->order_id.empty()||f.actual_resource_effect)fail("ExactFormationFailedResultMismatch",f.id);
  for(const auto& army:x.military.armies)if(army.formation_source==f.id)fail("FormationFailedCompletionHasArmy",f.id);
 }
 // Hot completion calls here after the real result/resource/application, but
 // before spatial adoption and the caller's S0 terminal/release. Running full
 // credited H=1 is valid here, including an original failed recheck. No later
 // mutable Army roster/commander or current participant projection is required.
});}
}
