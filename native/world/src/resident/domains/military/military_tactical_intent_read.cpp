#include "military_tactical_intent_read.hpp"
#include "dynamic_effect_source.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
void owner(const MomentOwnerSource& source){
 if(source.source_schema<=0||!source_valid(source.source_ref))
  fail("MilitaryIntentActualOwnerSourceRequired",source.source_ref.id);
}
void selected(const MilitaryAcceptedIntentReadContext& x){
 if(x.originals.selected_intent_schema()!=3||!x.originals.bound_to(x.common,x.exact))
  fail("MilitaryIntentOriginalMissionOwnerRequired");
}
bool same_ordinal(const MilitaryAcceptedIntent3& a,const MilitaryAcceptedIntent3& b){
 return a.session_ref==b.session_ref&&a.generation==b.generation&&a.unit_ref==b.unit_ref&&
     a.action_instance_ordinal==b.action_instance_ordinal;
}
class OriginalOrdinal final:public MilitaryAcceptedIntentVisitor {
 const MilitaryAcceptedIntent3& actual_;std::size_t count_=0;
public:
 explicit OriginalOrdinal(const MilitaryAcceptedIntent3& row):actual_(row){}
 Status accepted_intent(const MilitaryAcceptedIntent3& row)override{
  if(row.id==actual_.id||same_ordinal(row,actual_)){
   if(&row!=&actual_||++count_!=1)return common_failure("MilitaryIntentOriginalOrdinalReused",actual_.id);
  }
  return common_success();
 }
 Status finish()const{return count_==1?common_success():common_failure("MilitaryIntentOriginalMissionMembershipRequired",actual_.id);}
};
const DynamicSessionStateSource2& initialized(const MilitaryAcceptedIntentReadContext& x,
 const MilitaryAcceptedIntent3& row,std::size_t accepted_cut,const WorldMoment& now){
 const auto* current=find_dynamic_military_session_current_row(x.exact,row.session_ref,row.generation);
 if(!current||current->session_ref!=row.session_ref||current->generation!=row.generation)
  fail("MilitaryIntentOriginalSessionRequired",row.id);
 const auto& source=current->initialization_source_ref;
 const auto* initial=x.exact.session_state_sources.find(source.id);
 const auto* payload=initial?std::get_if<DynamicSessionInitialized>(&initial->payload):nullptr;
 if(source.domain!="military"||source.kind!="dynamic_session_state"||!initial||!payload||
    initial->id!=source.id||initial->session_ref!=row.session_ref||initial->generation!=row.generation||
    take(read_dynamic_military_fact_position(x.common,source))>=accepted_cut||
    take(compare_world_moments(initial->effective_at,row.accepted_at,x.budget))>0||
    take(compare_world_moments(row.accepted_at,now,x.budget))>0||
    !same_moment(current->opened_at,initial->effective_at,x.budget))
  fail("MilitaryIntentOriginalInitializationRequired",row.id);
 std::size_t count=0;for(const auto& unit:payload->unit_entry_rows)if(unit.unit_ref==row.unit_ref)++count;
 if(count!=1)fail("MilitaryIntentOriginalUnitRequired",row.unit_ref);
 // A later terminal does not retroactively invalidate this actual acceptance.
 // Same-M ordering is read from real Common append order, never an ID suffix.
 for(const auto& entry:x.exact.session_state_sources.rows()){
  const auto& source_row=entry.second;
  if(source_row.session_ref!=row.session_ref||source_row.generation!=row.generation||
     !std::holds_alternative<DynamicSessionTerminated>(source_row.payload))continue;
  if(entry.first!=source_row.id)fail("MilitaryIntentSessionSourceKeyMismatch",entry.first);
  if(take(read_dynamic_military_fact_position(x.common,{"military","dynamic_session_state",source_row.id}))<accepted_cut||
     take(compare_world_moments(source_row.effective_at,row.accepted_at,x.budget))<0)
   fail("MilitaryIntentAcceptedAfterSessionTerminal",row.id);
 }
 return *initial;
}
void decision(const MilitaryAcceptedIntentReadContext& x,const MilitaryAcceptedIntent3& row,
 const MilitaryCommittedDecisionIntent3& cause,std::size_t accepted_cut){
 const auto& ref=cause.decision_source_ref;owner(ref);
 if(ref.source_schema!=2||ref.source_ref.domain!="military"||ref.source_ref.kind!="dynamic_session_state"||
    cause.controller_ref.empty()||cause.control_binding_generation<=0)
  fail("MilitaryIntentDecisionSourceRequired",row.id);
 const auto* source=x.exact.session_state_sources.find(ref.source_ref.id);
 const auto* value=source?std::get_if<DynamicSessionDecisionCommitted>(&source->payload):nullptr;
 if(!source||!value||source->id!=ref.source_ref.id||source->session_ref!=row.session_ref||source->generation!=row.generation||
    value->controller_ref!=cause.controller_ref||value->control_binding_generation!=cause.control_binding_generation||
    !same_moment(source->effective_at,row.accepted_at,x.budget)||
    take(read_dynamic_military_fact_position(x.common,ref.source_ref))>=accepted_cut)
  fail("MilitaryIntentOriginalDecisionMismatch",row.id);
 const auto own=military_accepted_intent_source3(row.id);std::size_t uses=0;
 for(const auto& selected:value->selected_intent_refs)if(same_source(selected,own.source_ref))++uses;
 if(uses!=1)fail("MilitaryIntentDecisionReverseBindingRequired",row.id);
 take(x.originals.validate_original_decision_inputs(*source));
}
void direct(const MilitaryAcceptedIntentReadContext& x,const MilitaryAcceptedIntent3& row,
 const MilitaryPlayerDirectIntent3& cause){
 owner(cause.actual_command_source);
 const auto& ref=cause.actual_command_source;
 if(cause.controller_actor_id.empty()||cause.control_epoch<0||ref.source_schema!=1||
    ref.source_ref.domain!="common"||ref.source_ref.kind!="command_result")
  fail("MilitaryDirectIntentActualSealRequired",row.id);
 const auto command=take(read_common_command_source(x.common,ref.source_ref));
 if(!command.result||!command.history||command.history->actor_id!=cause.controller_actor_id||
    command.result->tick!=row.accepted_at.half_day_tick)
  fail("MilitaryDirectIntentOriginalControllerMismatch",row.id);
 // The tick equality is only the original coarse projection. This mandatory
 // source-local callback proves the exact accepted M and payload association.
 take(x.originals.validate_original_direct_intent_command(row,command));
}
}
MomentOwnerSource military_accepted_intent_source3(const StableId& id){return {{"military","tactical_intent_accepted",id},3};}
Outcome<const MilitaryAcceptedIntent3*> read_verified_military_accepted_intent3(
 const MilitaryAcceptedIntentReadContext& x,const MomentOwnerSource& source){return guard<const MilitaryAcceptedIntent3*>([&]{
 selected(x);owner(source);
 if(source.source_schema!=3||source.source_ref.domain!="military"||source.source_ref.kind!="tactical_intent_accepted")
  fail("MilitaryAcceptedIntent3SourceRequired",source.source_ref.id);
 const auto cut=take(read_dynamic_military_fact_position(x.common,source.source_ref));
 const auto* row=take(x.originals.read_original_accepted_intent(source.source_ref));
 if(!row||row->id!=source.source_ref.id||row->id.empty()||row->session_ref.empty()||row->generation<=0||
    row->formation_ref.empty()||row->mission_ref.empty()||row->unit_ref.empty()||row->action_instance_ordinal<0||
    row->timing_definition_ref.empty()||row->timing_definition_version<=0)
  fail("MilitaryAcceptedIntentOriginalPayloadRequired",source.source_ref.id);
 owner(row->control_binding_source);owner(row->mission_scope_source);
 if(same_source(row->control_binding_source.source_ref,source.source_ref)||
    same_source(row->mission_scope_source.source_ref,source.source_ref))
  fail("MilitaryAcceptedIntentCannotAuthorizeItself",row->id);
 const auto now=take(x.clock.read_current_moment());take(encode_world_moment(row->accepted_at,x.budget));
 initialized(x,*row,cut,now);OriginalOrdinal ordinal(*row);
 take(x.originals.visit_original_accepted_intents(ordinal));take(ordinal.finish());
 take(x.originals.validate_original_intent_scope(*row));
 std::visit([&](const auto& cause){using T=std::decay_t<decltype(cause)>;
  if constexpr(std::is_same_v<T,MilitaryPlayerDirectIntent3>)direct(x,*row,cause);
  else if constexpr(std::is_same_v<T,MilitaryCommittedDecisionIntent3>)decision(x,*row,cause,cut);
  else{
   owner(cause.order_ref);owner(cause.actual_response_source);
   if(same_source(cause.order_ref.source_ref,source.source_ref)||same_source(cause.actual_response_source.source_ref,source.source_ref))
    fail("MilitaryIntentActualDeliveredOrderRequired",row->id);
   take(x.originals.validate_original_delivered_intent(*row));
  }
 },row->adoption);
 return row;
});}
Outcome<DynamicMilitaryAcceptedIntentRead> read_military_accepted_action_intent3(
 const MilitaryAcceptedIntentReadContext& x,const SourceRef& source){return guard<DynamicMilitaryAcceptedIntentRead>([&]{
 const auto& row=*take(read_verified_military_accepted_intent3(x,{source,3}));
 return DynamicMilitaryAcceptedIntentRead{source,row.session_ref,row.generation,row.unit_ref,
     row.timing_definition_ref,row.timing_definition_version,row.action_instance_ordinal,row.original_order,row.accepted_at};
});}
Status validate_military_accepted_intent_sources3(const MilitaryAcceptedIntentReadContext& x){return status([&]{
 selected(x);
 class Sources final:public MilitaryAcceptedIntentVisitor {
  const MilitaryAcceptedIntentReadContext& x_;
 public:
  explicit Sources(const MilitaryAcceptedIntentReadContext& x):x_(x){}
  Status accepted_intent(const MilitaryAcceptedIntent3& row)override{return status([&]{
   const auto* original=take(read_verified_military_accepted_intent3(x_,military_accepted_intent_source3(row.id)));
   if(original!=&row)fail("MilitaryAcceptedIntentBorrowedRowRequired",row.id);
  });}
 } sources(x);
 take(x.originals.visit_original_accepted_intents(sources));
 for(const auto& fact:x.common.state().facts.rows())if(fact.second.domain=="military"&&fact.second.kind=="tactical_intent_accepted"){
  if(fact.first!=fact.second.id)fail("MilitaryAcceptedIntentHeaderKeyMismatch",fact.first);
  take(read_verified_military_accepted_intent3(x,military_accepted_intent_source3(fact.first)));
 }
 // A retained Decision can neither omit its accepted row nor name a row from
 // another Decision, even if the unit/mission strings happen to be equal.
 for(const auto& entry:x.exact.session_state_sources.rows()){
  const auto* decision=std::get_if<DynamicSessionDecisionCommitted>(&entry.second.payload);if(!decision)continue;
  const MomentOwnerSource original{{"military","dynamic_session_state",entry.first},2};
  for(const auto& accepted:decision->selected_intent_refs){
   const auto* row=take(read_verified_military_accepted_intent3(x,{accepted,3}));
   const auto* cause=std::get_if<MilitaryCommittedDecisionIntent3>(&row->adoption);
   if(entry.first!=entry.second.id||!cause||cause->decision_source_ref.source_schema!=2||
      !same_source(cause->decision_source_ref.source_ref,original.source_ref))
    fail("MilitaryDecisionAcceptedIntentCoverageMismatch",entry.first);
  }
 }
});}
}
