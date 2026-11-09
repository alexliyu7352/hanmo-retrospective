#include "growth_fixed_spatial_inputs.hpp"
#include "growth_model.hpp"
#include "growth_action_time_read.hpp"
#include "growth_quiet_source.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include "resident/domains/travel/movement_exact.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
template<class F>Status guarded(F fn){try{fn();return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
bool same(const SourceRef& a,const SourceRef& b){return a.domain==b.domain&&a.kind==b.kind&&a.id==b.id;}
const StableId& original_place(const GrowthState& growth,const LifeFixedServiceBoundary& boundary){
 if(const auto* raw=growth.facts.find(boundary.action_ref)){
  if(const auto* f=std::get_if<TrainingFact>(raw))return f->training.place_id;
  if(const auto* f=std::get_if<StudyFact>(raw))return f->terms.context.place_id;
  if(std::holds_alternative<RestFact>(*raw)){const auto& quiet=boundary.input_snapshot.specific.growth_fact.original_quiet_or_rest_basis;if(!quiet)fail("GrowthFixedRestQuietSourceRequired",boundary.boundary_id);return quiet->place_ref;}
 }
 if(const auto* f=growth.life_activities.find(boundary.action_ref))return f->context.place_id;
 fail("GrowthFixedOriginalPlaceRequired",boundary.action_ref);
}
void at_original_place(const GrowthState& growth,const LifeFixedServiceBoundary& boundary,const ActorSpatialLocationView& value){
 if(std::holds_alternative<FieldBattleSpatialView>(value.actual_anchor))fail("GrowthFixedActualSiteMismatch",value.actor_id);
 const auto* place=std::get_if<SettlementLocation>(&value.location_snapshot);
 if(!place||place->place_id!=original_place(growth,boundary))fail("GrowthFixedActualSiteMismatch",value.actor_id);
}
struct RetainedSpatialSources {CheckpointMemoryCharge memory;std::vector<ActualSpatialSourceRef> values;};
RetainedSpatialSources retained_sources(const CharacterConditionSources& body,const LifeFixedActorInputs& input){
 if(input.actual_location_source_refs.size()>SIZE_MAX/sizeof(ActualSpatialSourceRef))throw std::bad_alloc();

 RetainedSpatialSources owned{CheckpointMemoryCharge(0),{}};auto& result=owned.values;result.reserve(input.actual_location_source_refs.size());
 for(const auto& stored:input.actual_location_source_refs){SourceRef source;
  if(const auto* value=std::get_if<SourceRef>(&stored))source=*value;
  else if(const auto* value=std::get_if<ScenarioOriginRef>(&stored)){
   auto original=take(read_authored_character_origin_v2(body.common.state(),body.state,body.catalog,input.actor_id));const auto* authored=std::get_if<AuthoredCharacterSource>(&original.source);
   if(!authored||!actor_foundation_origin_equal(*value,authored->actor_start_context_ref))fail("GrowthFixedScenarioPlacementMismatch",input.actor_id);
   source={"actor_foundation","scenario_placement",input.actor_id};
  }else fail("GrowthFixedActualLocationSourceRequired",input.actor_id);
  const auto schema=take(actor_spatial_history_source_schema(source));
  for(const auto& previous:result)if(same(previous.source,source))fail("GrowthFixedDuplicateLocationSource",input.actor_id);
  result.push_back({std::move(source),schema});
 }
 if(result.empty())fail("GrowthFixedActualLocationSourceRequired",input.actor_id);return owned;
}
ActorSpatialLocationView read_historical(const CharacterConditionSources& body,const GrowthFixedSpatialSources& spatial,const LifeFixedServiceBoundary& boundary,const LifeFixedActorInputs& input){
 take(validate_growth_fixed_spatial_source_moment(spatial,body.actual_now));
 if(!body.shared)fail("GrowthFixedSharedCatalogRequired");
 take(read_character_initial_values_v2(body.common,body.state,body.catalog,body.family_catalog,body.shared,input.actor_id,body.actual_now,body.born_sources));
 auto sources=retained_sources(body,input);
 auto value=take(read_actor_spatial_location_at_source_head(body.common,*body.shared,spatial.movement,spatial.carriers,spatial.registrations,body.state,body.catalog,spatial.original,input.actor_id,sources.values,boundary.occurred_at,spatial.military_handoffs,{},body.family_catalog,spatial.births,spatial.military_actions,spatial.custody_releases,spatial.field_exits));
 return value;
}
bool requires_site(const GrowthState& growth,const LifeFixedServiceBoundary& boundary){
 if(boundary.kind==LifeFixedBoundaryKind::suspended||boundary.kind==LifeFixedBoundaryKind::stopped)return false;
 return boundary.kind!=LifeFixedBoundaryKind::input_rebound||!growth_fixed_was_suspended(growth,boundary);
}
ActorSpatialLocationView historical(const CharacterConditionSources& body,const GrowthFixedSpatialSources& spatial,const LifeFixedServiceBoundary& boundary,const LifeFixedActorInputs& input){
 auto value=read_historical(body,spatial,boundary,input);
 if(!growth_fixed_location_equal(input.location_snapshot,value.location_snapshot))fail("GrowthFixedFrozenLocationMismatch",boundary.boundary_id);
 if(requires_site(*body.growth,boundary))at_original_place(*body.growth,boundary,value);return value;
}
}
// 含晚绑定历史 reader 的快照必须留在其原同 M 完整租约内；不改 Body 刷新规则。
Status validate_growth_fixed_spatial_source_moment(
 const GrowthFixedSpatialSources& spatial,const WorldMoment& actual_now){return guarded([&]{
 if(!spatial.history_owner_at){
  if(spatial.custody_releases||spatial.field_exits)fail("GrowthSpatialHistoryOwnerMomentRequired");
  return;
 }
 if(take(compare_world_moments(*spatial.history_owner_at,actual_now))||
    take(compare_world_moments(*spatial.history_owner_at,take(spatial.original.current_moment()))))
  fail("GrowthSpatialHistoryOwnerMomentMismatch");
});}
ActorFoundationRuleRef growth_fixed_original_site_rule(const CatalogIdentity& identity){return {"original_growth_fixed_site",1,identity.content_digest};}
bool growth_fixed_was_suspended(const GrowthState& growth,const LifeFixedServiceBoundary& boundary){
 bool paused=false;Integer ordinal=-1;
 for(const auto& row:growth.fixed_service_boundaries.rows()){const auto& prior=row.second;if(prior.action_ref!=boundary.action_ref||prior.boundary_ordinal>=boundary.boundary_ordinal||prior.boundary_ordinal<=ordinal)continue;if(prior.kind==LifeFixedBoundaryKind::suspended||prior.kind==LifeFixedBoundaryKind::resumed){ordinal=prior.boundary_ordinal;paused=prior.kind==LifeFixedBoundaryKind::suspended;}}
 return paused;
}
bool growth_fixed_location_equal(const Location& a,const Location& b){
 if(a.index()!=b.index())return false;
 return std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(b);
  if constexpr(std::is_same_v<T,SettlementLocation>)return x.settlement_id==y.settlement_id&&x.place_id==y.place_id;
  else if constexpr(std::is_same_v<T,WildernessLocation>)return x.map_id==y.map_id&&x.origin==y.origin&&x.point.x==y.point.x&&x.point.y==y.point.y;
  else if constexpr(std::is_same_v<T,OnRouteLocation>)return x.edge_id==y.edge_id&&x.origin==y.origin&&x.progress==y.progress;
  else if constexpr(std::is_same_v<T,FollowingLocation>)return x.party_id==y.party_id;
  else if constexpr(std::is_same_v<T,InArmyLocation>)return x.army_id==y.army_id;
  else return x.carrier_kind==y.carrier_kind&&x.carrier_id==y.carrier_id;
 },a);
}
bool growth_fixed_location_heads_equal(const LifeFixedActorInputs& a,const LifeFixedActorInputs& b){
 if(a.actual_location_source_refs.size()!=b.actual_location_source_refs.size())return false;
 for(std::size_t i=0;i<a.actual_location_source_refs.size();++i){const auto& x=a.actual_location_source_refs[i];const auto& y=b.actual_location_source_refs[i];if(x.index()!=y.index())return false;
  if(const auto* ref=std::get_if<SourceRef>(&x)){if(!same(*ref,std::get<SourceRef>(y)))return false;}
  else if(const auto* ref=std::get_if<ScenarioOriginRef>(&x)){if(!actor_foundation_origin_equal(*ref,std::get<ScenarioOriginRef>(y)))return false;}
  else return false;
 }return true;
}
Status refresh_growth_fixed_spatial_projection(const CharacterConditionSources& body,const GrowthFixedSpatialSources& spatial,LifeFixedServiceBoundary& boundary){return guarded([&]{
 if(!body.growth||body.movement!=&spatial.movement)fail("GrowthFixedSpatialSourcesRequired",boundary.action_ref);
 for(auto& input:boundary.input_snapshot.actors)input.location_snapshot=std::move(read_historical(body,spatial,boundary,input).location_snapshot);
});}
Outcome<bool> read_growth_fixed_actor_at_original_site(const CharacterConditionSources& body,const GrowthFixedSpatialSources& spatial,const GrowthState& growth,const LifeFixedServiceBoundary& boundary,const LifeFixedActorInputs& input){try{
 if(body.growth!=&growth)fail("GrowthFixedSpatialCandidateRequired",boundary.action_ref);
 auto actual=read_historical(body,spatial,boundary,input);const auto* place=std::get_if<SettlementLocation>(&actual.location_snapshot);
 return {true,place&&place->place_id==original_place(growth,boundary),{}};
 }catch(Failure& e){return {false,false,std::move(e.error)};}}
Status validate_growth_fixed_position_input_change(const CharacterConditionSources& body,const GrowthFixedSpatialSources& spatial,const GrowthState& growth,const LifeFixedServiceBoundary& boundary,const LifeActualPositionInputChange& change){return guarded([&]{
 if(body.growth!=&growth||!body.growth_catalog||!body.growth_catalog->complete_exact_command_cuts||!boundary.previous_boundary_ref||take(actor_spatial_history_source_schema(change.original_source_ref))!=change.source_schema)fail("GrowthFixedActualPositionSourceRequired",boundary.boundary_id);
 const auto* prior=growth.fixed_service_boundaries.find(*boundary.previous_boundary_ref);if(!prior||prior->action_ref!=boundary.action_ref||prior->boundary_ordinal+1!=boundary.boundary_ordinal)fail("GrowthFixedInputPreviousMismatch",boundary.boundary_id);
 const ActorFoundationRuleRef* used=nullptr;if(const auto* cause=std::get_if<LifeActualInputChange>(&boundary.cause))used=&cause->transition_rule_ref;else if(const auto* terminal=std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))if(const auto* stop=std::get_if<LifeActualStopTrigger>(&terminal->trigger))used=&stop->original_stop_rule_ref;
 if(!used||!actor_foundation_rule_equal(*used,growth_fixed_original_site_rule(body.catalog.identity)))fail("GrowthFixedOriginalSiteRuleMismatch",boundary.boundary_id);
 bool affected=false,at_site=true;
 for(const auto& input:boundary.input_snapshot.actors){const LifeFixedActorInputs* old=nullptr;for(const auto& candidate:prior->input_snapshot.actors)if(candidate.actor_id==input.actor_id&&candidate.role==input.role){if(old)fail("DuplicateGrowthFixedActorRole",boundary.boundary_id);old=&candidate;}if(!old)fail("GrowthFixedInputRoleChanged",boundary.boundary_id);
  if(!growth_fixed_location_heads_equal(*old,input)){bool actual=false;for(const auto& ref:input.actual_location_source_refs)if(const auto* source=std::get_if<SourceRef>(&ref);source&&same(*source,change.original_source_ref))actual=true;if(!actual||affected)fail("GrowthFixedUnexplainedPositionChange",boundary.boundary_id);affected=true;}
  auto actual=historical(body,spatial,boundary,input);const auto* place=std::get_if<SettlementLocation>(&actual.location_snapshot);at_site=at_site&&place&&place->place_id==original_place(growth,boundary);
 }
 if(!affected)fail("GrowthFixedPositionSourceNotChanged",boundary.boundary_id);
 const bool paused=growth_fixed_was_suspended(growth,boundary);
 if(boundary.kind==LifeFixedBoundaryKind::input_rebound){if(!paused&&!at_site)fail("GrowthFixedPositionCannotContinue",boundary.boundary_id);}
 else if(boundary.kind==LifeFixedBoundaryKind::suspended){if(paused||at_site)fail("GrowthFixedPositionSuspensionMismatch",boundary.boundary_id);}
 else if(boundary.kind==LifeFixedBoundaryKind::stopped){if(at_site)fail("GrowthFixedPositionStopMismatch",boundary.boundary_id);}
 else fail("GrowthFixedPositionDispositionMismatch",boundary.boundary_id);
});}
Outcome<std::vector<ActorSpatialHeadChange>> read_growth_fixed_spatial_head_changes(const CharacterConditionSources& body,const GrowthFixedSpatialSources& spatial,const LifeFixedServiceBoundary& from,const LifeFixedActorInputs& input,const LifeFixedServiceBoundary* terminal){try{take(validate_growth_fixed_spatial_source_moment(spatial,body.actual_now));

 if(!body.shared||!body.growth||body.movement!=&spatial.movement)fail("GrowthFixedSpatialSourcesRequired",from.action_ref);
 auto start=retained_sources(body,input);std::vector<ActualSpatialSourceRef> current;RetainedSpatialSources retained;WorldMoment until=body.actual_now;
 if(terminal){const LifeFixedActorInputs* final=nullptr;for(const auto& person:terminal->input_snapshot.actors)if(person.actor_id==input.actor_id&&person.role==input.role){if(final)fail("DuplicateGrowthFixedActorRole",terminal->boundary_id);final=&person;}if(!final||terminal->action_ref!=from.action_ref)fail("GrowthFixedFinalRoleMissing",from.action_ref);retained=retained_sources(body,*final);until=terminal->occurred_at;}
 else current=take(read_actual_actor_spatial_location(body.common,spatial.movement,spatial.original,spatial.battle,input.actor_id)).actual_location_sources;
 return read_actor_spatial_head_changes(body.common,*body.shared,spatial.movement,spatial.carriers,spatial.registrations,body.state,body.catalog,spatial.original,input.actor_id,start.values,from.occurred_at,terminal?retained.values:current,until,spatial.military_handoffs,{},body.family_catalog,spatial.births,spatial.military_actions,spatial.custody_releases,spatial.field_exits);
 }catch(Failure& e){return {false,{},std::move(e.error)};}}
Status capture_growth_fixed_spatial_inputs_in_candidate(const CharacterConditionSources& body,const GrowthFixedSpatialSources& spatial,GrowthState& growth,const StableId& id,const ReadStamp& stamp,const ReadStampAuthority& authority,const DomainReadVersionSource& versions){return guarded([&]{
 if(body.growth!=&growth||!body.shared||body.movement!=&spatial.movement||stamp.mode()==ReadMode::committed_history)fail("GrowthFixedSpatialCandidateRequired",id);
 take(authority.validate(stamp,body.common.state().identity,stamp.mode(),body.common,versions));
 auto* boundary=growth.fixed_service_boundaries.find(id);if(!boundary||take(compare_world_moments(boundary->occurred_at,body.actual_now))||take(compare_world_moments(stamp.span().to,body.actual_now))||std::find(stamp.source_ids().begin(),stamp.source_ids().end(),id)==stamp.source_ids().end())fail("GrowthFixedSpatialBoundaryStampRequired",id);
 const auto* action=body.common.action(boundary->action_ref);if(!action)fail("GrowthFixedSpatialActualActionRequired",id);
 for(auto& input:boundary->input_snapshot.actors){
  bool version=false;for(const auto& v:stamp.resources())if(v.resource.kind==ResourceKey::Kind::actor&&v.resource.id==input.actor_id)version=true;
  const auto* actor=body.common.actor(input.actor_id);if(!version||!actor||actor->action_id!=action->id||!input.actual_location_source_refs.empty())fail("GrowthFixedSpatialCaptureMismatch",input.actor_id);
  take(read_character_initial_values_v2(body.common,body.state,body.catalog,body.family_catalog,body.shared,input.actor_id,body.actual_now,body.born_sources));
  if(take(read_actual_transport_participation_presence(spatial.carriers,spatial.original,input.actor_id)))fail("GrowthFixedActorInTransport",input.actor_id);
  auto actual=take(read_actual_actor_spatial_location(body.common,spatial.movement,spatial.original,spatial.battle,input.actor_id));at_original_place(growth,*boundary,actual);
  for(const auto& source:actual.actual_location_sources){if(take(actor_spatial_history_source_schema(source.source))!=source.schema)fail("GrowthFixedSpatialSchemaMismatch",source.source.id);input.actual_location_source_refs.emplace_back(source.source);}
  input.location_snapshot=std::move(actual.location_snapshot);
  historical(body,spatial,*boundary,input);
 }
});}
Status validate_growth_fixed_spatial_inputs(const CharacterConditionSources& body,const GrowthFixedSpatialSources& spatial,const GrowthState& growth){return guarded([&]{take(validate_growth_fixed_spatial_source_moment(spatial,body.actual_now));

 if(body.growth!=&growth||!body.shared||!body.elapsed||!body.scale||body.movement!=&spatial.movement)fail("GrowthFixedSpatialSourcesRequired");
 for(const auto& row:growth.fixed_service_boundaries.rows()){
  const auto& boundary=row.second;const LifeActualPositionInputChange* position=nullptr;
  if(boundary.action_kind=="long_rest"){const auto* raw=growth.facts.find(boundary.action_ref);const auto* rest=raw?std::get_if<RestFact>(raw):nullptr;const auto& quiet=boundary.input_snapshot.specific.growth_fact.original_quiet_or_rest_basis;if(!rest||!quiet||take(compare_world_moments(quiet->observed_at,boundary.occurred_at)))fail("GrowthLongRestQuietSourceRequired",boundary.boundary_id);take(validate_growth_long_rest_quiet(body,rest->actor_id,*quiet));if(boundary.kind!=LifeFixedBoundaryKind::stopped&&!quiet->is_quiet)fail("GrowthLongRestNotQuiet",boundary.boundary_id);}

  if(const auto* changed=std::get_if<LifeActualInputChange>(&boundary.cause))position=std::get_if<LifeActualPositionInputChange>(&changed->change);
  else if(const auto* terminal=std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))if(const auto* stop=std::get_if<LifeActualStopTrigger>(&terminal->trigger))if(const auto* changed=std::get_if<LifeFixedInputChange>(&stop->source))position=std::get_if<LifeActualPositionInputChange>(changed);
  const auto* prior=boundary.previous_boundary_ref?growth.fixed_service_boundaries.find(*boundary.previous_boundary_ref):nullptr;
  for(const auto& input:boundary.input_snapshot.actors){historical(body,spatial,boundary,input);if(prior&&!position){const LifeFixedActorInputs* old=nullptr;for(const auto& candidate:prior->input_snapshot.actors)if(candidate.actor_id==input.actor_id&&candidate.role==input.role)old=&candidate;if(!old||!growth_fixed_location_heads_equal(*old,input))fail("GrowthFixedUnexplainedPositionChange",boundary.boundary_id);}}
  if(position)take(validate_growth_fixed_position_input_change(body,spatial,growth,boundary,*position));
 }
 // Every immutable change after Started must have one real owner adoption.
 // The S1 reader removes continuous interpolation and resolves contact aliases.
 for(const auto& row:growth.fixed_service_boundaries.rows()){
  const auto& start=row.second;if(start.kind!=LifeFixedBoundaryKind::started)continue;
  const LifeFixedServiceBoundary* last=&start;for(const auto& candidate:growth.fixed_service_boundaries.rows())if(candidate.second.action_ref==start.action_ref&&candidate.second.boundary_ordinal>last->boundary_ordinal)last=&candidate.second;
  const bool terminal=last->kind==LifeFixedBoundaryKind::completed||last->kind==LifeFixedBoundaryKind::stopped;
  for(const auto& person:start.input_snapshot.actors){auto changes=take(read_growth_fixed_spatial_head_changes(body,spatial,start,person,terminal?last:nullptr));
   for(const auto& actual:changes){std::size_t matches=0;
    for(const auto& candidate:growth.fixed_service_boundaries.rows()){const auto& boundary=candidate.second;if(boundary.action_ref!=start.action_ref||take(compare_world_moments(boundary.occurred_at,actual.occurred_at)))continue;
     const LifeActualPositionInputChange* changed=nullptr;if(const auto* cause=std::get_if<LifeActualInputChange>(&boundary.cause))changed=std::get_if<LifeActualPositionInputChange>(&cause->change);else if(const auto* result=std::get_if<LifeOriginalTerminalApplication>(&boundary.cause))if(const auto* stop=std::get_if<LifeActualStopTrigger>(&result->trigger))if(const auto* change=std::get_if<LifeFixedInputChange>(&stop->source))changed=std::get_if<LifeActualPositionInputChange>(change);
     if(!changed)continue;for(const auto& input:boundary.input_snapshot.actors)if(input.actor_id==person.actor_id&&input.role==person.role){bool is_cause=false;for(const auto& ref:input.actual_location_source_refs)if(const auto* source=std::get_if<SourceRef>(&ref);source&&same(*source,changed->original_source_ref))is_cause=true;if(!is_cause)continue;auto frozen=retained_sources(body,input);const auto order=take(compare_actor_spatial_source_heads_at_moment(body.common,*body.shared,spatial.movement,spatial.carriers,spatial.registrations,body.state,body.catalog,spatial.original,person.actor_id,actual.immutable_source_head,frozen.values,boundary.occurred_at,spatial.military_handoffs,{},body.family_catalog,spatial.births,spatial.military_actions,spatial.custody_releases,spatial.field_exits));if(!order)++matches;}
    }
    if(matches!=1)fail("GrowthFixedPositionInputCoverageMismatch",start.action_ref);
   }
  }
 }
 // One pass per actual S0 running interval. Suspended windows supply nothing.
 for(const auto& row:growth.fixed_service_boundaries.rows()){
  const auto& boundary=row.second;if(boundary.kind==LifeFixedBoundaryKind::suspended||boundary.kind==LifeFixedBoundaryKind::completed||boundary.kind==LifeFixedBoundaryKind::stopped)continue;
  const LifeFixedServiceBoundary* next=nullptr;for(const auto& later:growth.fixed_service_boundaries.rows())if(later.second.action_ref==boundary.action_ref&&later.second.boundary_ordinal==boundary.boundary_ordinal+1)next=&later.second;
  const auto& to=next?next->occurred_at:body.actual_now;if(take(compare_world_moments(boundary.occurred_at,to))>=0)continue;
  auto intervals=take(read_fixed_service_intervals(*body.elapsed,boundary.action_ref,boundary.occurred_at,to,body.actual_now,*body.scale));
  for(const auto& interval:intervals.intervals){
   if(boundary.action_kind=="long_rest"){const auto* raw=growth.facts.find(boundary.action_ref);const auto* rest=raw?std::get_if<RestFact>(raw):nullptr;const auto& quiet=boundary.input_snapshot.specific.growth_fact.original_quiet_or_rest_basis;if(!rest||!quiet)fail("GrowthLongRestQuietSourceRequired",boundary.boundary_id);take(validate_growth_long_rest_quiet_span(body,rest->actor_id,*quiet,{interval.from,interval.until}));}

   for(const auto& input:boundary.input_snapshot.actors){auto sources=retained_sources(body,input);auto span=take(read_actor_stationary_location_span(body.common,*body.shared,spatial.movement,spatial.carriers,spatial.registrations,body.state,body.catalog,spatial.original,input.actor_id,sources.values.front().source,MomentSpan{interval.from,interval.until},spatial.military_handoffs,{},body.family_catalog,spatial.births,spatial.military_actions,spatial.custody_releases,spatial.field_exits));
    const auto location=take(project_movement_location(span.actual_anchor));const auto* place=std::get_if<SettlementLocation>(&location);if(!place||place->place_id!=original_place(growth,boundary))fail("GrowthFixedServiceLeftActualSite",input.actor_id);
   }
  }
 }
});}
}
