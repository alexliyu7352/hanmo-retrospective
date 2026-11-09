#include "military_tactical_ownership.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {
namespace {
// One borrowed traversal owns the actual source-slot list. It is reused by the
// original lasting PIN builder and exact membership checks; it allocates no
// source/consumer copies, generic dedup set or parallel derived index.
template<class Emit>Status visit_actual_tactical_pins(const CommonStore& common,const MilitaryState& military,
 const MilitaryExactState& exact,const MilitaryTacticalOwnershipContext3& c,Emit emit){return movement_detail::status([&]{
 movement_detail::take(validate_military_tactical_ownership3(common,military,exact,c));
 auto parts=[&](std::string_view kind,const StableId& id,std::string_view slot,
     std::string_view domain,std::string_view source_kind,std::string_view source_id,SourcePinReason reason){
  if(domain.empty()||source_kind.empty()||source_id.empty())movement_detail::fail("MilitaryTacticalPinSourceRequired",id);
  movement_detail::take(emit(SourcePinBuildView{"military",kind,id,slot,domain,source_kind,source_id,reason}));
 };
 auto pin=[&](std::string_view kind,const StableId& id,std::string_view slot,const SourceRef& source,SourcePinReason reason){
  parts(kind,id,slot,source.domain,source.kind,source.id,reason);
 };
 auto prefix=[&](std::string_view kind,const StableId& id,const CommonActualFactPrefix& before){
  const auto count=movement_detail::take(read_actual_common_fact_prefix(common,before));
  for(std::size_t n=0;n<count;++n){
   const auto& key=common.state().facts.order()[n];const auto* actual=common.state().facts.find(key);
   if(!actual||actual->id!=key)movement_detail::fail("MilitaryTacticalPinnedPrefixSourceRequired",id);
   parts(kind,id,"actual_before_common_prefix",actual->domain,actual->kind,actual->id,SourcePinReason::unique_outcome);
  }
 };
 for(const auto& entry:exact.formations.rows()){
  const auto& f=entry.second;
  pin("tactical_formation",entry.first,"original_admission",f.birth.original_admission_source.source_ref,SourcePinReason::active_authority);
  for(const auto& source:f.birth.original_source_refs)pin("tactical_formation",entry.first,"original_sources",source.source_ref,SourcePinReason::active_authority);
  prefix("tactical_formation",entry.first,f.birth.actual_before_common_prefix);
  if(f.superior_formation_ref)parts("tactical_formation",entry.first,"superior","military","tactical_formation_created",*f.superior_formation_ref,SourcePinReason::active_authority);
  if(f.active_intent_ref)parts("tactical_formation",entry.first,"active_intent","military","tactical_mission_adopted",*f.active_intent_ref,SourcePinReason::dependent_state);
  if(const auto* autonomous=std::get_if<MilitaryFormationAutonomous3>(&f.control)){
   std::visit([&](const auto& origin){using T=std::decay_t<decltype(origin)>;
    if constexpr(std::is_same_v<T,MilitaryFormationPrebattle3>)pin("tactical_formation",entry.first,"autonomous_origin",origin.assignment_ref.source_ref,SourcePinReason::active_authority);
    else pin("tactical_formation",entry.first,"autonomous_origin",origin.dispatch_fact_ref.source_ref,SourcePinReason::active_authority);
   },autonomous->origin);
  }
 }
 for(const auto& entry:exact.missions.rows()){
  const auto& m=entry.second;
  prefix("tactical_mission",entry.first,m.actual_before_common_prefix);
  pin("tactical_mission",entry.first,"control_binding",m.control_binding_source.source_ref,SourcePinReason::active_authority);
  if(m.previous_intent_ref)parts("tactical_mission",entry.first,"previous_intent","military","tactical_mission_adopted",*m.previous_intent_ref,SourcePinReason::unique_outcome);
  pin("tactical_mission",entry.first,"actual_command",m.actual_command_source->source_ref,SourcePinReason::unique_outcome);
  for(const auto& source:m.original_scope_sources)pin("tactical_mission",entry.first,"original_scope",source.source_ref,SourcePinReason::active_authority);
  for(const auto& execution:m.original_executions)pin("tactical_mission",entry.first,"original_execution",execution.execution_source.source_ref,SourcePinReason::dependent_state);
  for(const auto& accepted:m.accepted_intents){
   parts("tactical_mission",entry.first,"accepted_intents","military","tactical_intent_accepted",accepted.id,SourcePinReason::unique_outcome);
   pin("tactical_intent_accepted",accepted.id,"control_binding",accepted.control_binding_source.source_ref,SourcePinReason::active_authority);
   pin("tactical_intent_accepted",accepted.id,"mission_scope",accepted.mission_scope_source.source_ref,SourcePinReason::active_authority);
   const DynamicMilitarySessionCurrent2* session=nullptr;
   for(const auto& current:exact.session_current_states.rows())
    if(dynamic_military_session_key_matches(current.first,accepted.session_ref,accepted.generation)){
     if(session)movement_detail::fail("MilitaryTacticalAcceptedSessionRequired",accepted.id);
     session=&current.second;
    }
   if(!session)movement_detail::fail("MilitaryTacticalAcceptedSessionRequired",accepted.id);
   pin("tactical_intent_accepted",accepted.id,"session_initialization",session->initialization_source_ref,SourcePinReason::dependent_state);
   std::visit([&](const auto& adoption){using T=std::decay_t<decltype(adoption)>;
    if constexpr(std::is_same_v<T,MilitaryPlayerDirectIntent3>)pin("tactical_intent_accepted",accepted.id,"adoption",adoption.actual_command_source.source_ref,SourcePinReason::unique_outcome);
    else if constexpr(std::is_same_v<T,MilitaryCommittedDecisionIntent3>)pin("tactical_intent_accepted",accepted.id,"adoption",adoption.decision_source_ref.source_ref,SourcePinReason::unique_outcome);
    else{
     pin("tactical_intent_accepted",accepted.id,"adoption",adoption.order_ref.source_ref,SourcePinReason::unique_outcome);
     pin("tactical_intent_accepted",accepted.id,"adoption",adoption.actual_response_source.source_ref,SourcePinReason::unique_outcome);
    }
   },accepted.adoption);
  }
 }
 });}
}
Status collect_military_tactical_ownership_pins3(const CommonStore& common,const MilitaryState& military,
 const MilitaryExactState& exact,const MilitaryTacticalOwnershipContext3& c,SourcePinSink& output){
 return visit_actual_tactical_pins(common,military,exact,c,[&](const SourcePinBuildView& pin){
  // Preserve the old six-part tuple identity, excluding reason. A repeated
  // actual reference in one slot remains one PIN, with its first reason.
  return output.contains(pin)?common_success():output.append(pin);
 });
}
std::vector<RequiredSourceConsumerSlot> military_tactical_ownership_consumer_slots3(){
 return {
  {"military","tactical_formation","original_admission"},
  {"military","tactical_formation","original_sources"},
  {"military","tactical_formation","actual_before_common_prefix"},
  {"military","tactical_formation","superior"},
  {"military","tactical_formation","active_intent"},
  {"military","tactical_formation","autonomous_origin"},
  {"military","tactical_mission","actual_before_common_prefix"},
  {"military","tactical_mission","control_binding"},
  {"military","tactical_mission","previous_intent"},
  {"military","tactical_mission","actual_command"},
  {"military","tactical_mission","original_scope"},
  {"military","tactical_mission","original_execution"},
  {"military","tactical_mission","accepted_intents"},
  {"military","tactical_intent_accepted","control_binding"},
  {"military","tactical_intent_accepted","mission_scope"},
  {"military","tactical_intent_accepted","session_initialization"},
  {"military","tactical_intent_accepted","adoption"}
 };
}
Status validate_military_tactical_ownership_pin3(const CommonStore& common,const MilitaryState& military,
 const MilitaryExactState& exact,const MilitaryTacticalOwnershipContext3& c,const SourcePin& input){
 return movement_detail::status([&]{
  if(input.consumer.domain!="military")movement_detail::fail("MilitaryTacticalActualPinRequired",input.consumer.id);
  bool seen=false,matched=false;
  movement_detail::take(visit_actual_tactical_pins(common,military,exact,c,[&](const SourcePinBuildView& pin){
   if(!seen&&pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&
      pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&
      pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id){
    seen=true;matched=pin.reason==input.reason;
   }
   return common_success();
  }));
  if(!matched)movement_detail::fail("MilitaryTacticalActualPinRequired",input.consumer.id);
 });
}
Outcome<bool> military_tactical_ownership_source_may_release3(const CommonStore& common,const MilitaryState& military,
 const MilitaryExactState& exact,const MilitaryTacticalOwnershipContext3& c,const MomentOwnerSource& source){
 return movement_detail::guard<bool>([&]{
  movement_detail::take(read_military_tactical_ownership_source_moment3(common,military,exact,c,source));
  // Every present source is the immutable birth, accepted mission or accepted
  // child of a retained original row. No policy exists to erase that row while
  // retaining its original fold, so a valid present payload is not releasable.
  return false;
 });
}
Outcome<std::uint64_t> read_military_tactical_domain_revision3(const CommonStore& common,const MilitaryState& military,
 const MilitaryExactState& exact,const MilitaryTacticalOwnershipContext3& c,const DomainReadKey& key){
 return movement_detail::guard<std::uint64_t>([&]{
  if(key.domain!="military")movement_detail::fail("MilitaryTacticalDomainReadKeyRequired",key.id);
  // Actual typed owned fields only. No zero revision or Common revision fallback.
  if(key.kind=="tactical_formation_intent"||key.kind=="tactical_formation_control"||
     key.kind=="tactical_formation_roster"||key.kind=="tactical_formation_command_binding"||
     key.kind=="tactical_formation_spatial_binding"){
   const auto* f=exact.formations.find(key.id);if(!f)movement_detail::fail("MilitaryTacticalFormationRequired",key.id);
   movement_detail::take(read_military_tactical_ownership_source_moment3(common,military,exact,c,military_tactical_formation_source3(key.id)));
   if(key.kind=="tactical_formation_control")return std::uint64_t(f->control_revision);
   if(key.kind=="tactical_formation_roster")return std::uint64_t(f->roster_revision);
   if(key.kind=="tactical_formation_command_binding")return std::uint64_t(f->command_binding_revision);
   if(key.kind=="tactical_formation_spatial_binding")
    return std::visit([](const auto& spatial){return std::uint64_t(spatial.binding_revision);},f->spatial_binding);
   if(f->intent_revision<=0)movement_detail::fail("MilitaryTacticalNoAcceptedIntent",key.id);
   return std::uint64_t(f->intent_revision);
  }
  if(key.kind=="tactical_mission"){
   const auto* m=exact.missions.find(key.id);if(!m)movement_detail::fail("MilitaryTacticalMissionRequired",key.id);
   movement_detail::take(read_military_tactical_ownership_source_moment3(common,military,exact,c,military_tactical_mission_source3(key.id)));
   return std::uint64_t(m->intent_revision);
  }
  movement_detail::fail("MilitaryTacticalDomainReadKeyRequired",key.id);
 });
}
}
