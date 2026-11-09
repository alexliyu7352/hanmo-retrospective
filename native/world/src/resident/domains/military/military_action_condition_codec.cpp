#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_action_condition_codec.hpp"
#include "military_action_condition_fields.hpp"
#include "military_state_codec_archive.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
struct Staging {OrderedTable<MilitaryActionAcceptanceConditions1> formation_acceptances,return_acceptances;};
struct Keys {
 template<class T>static constexpr bool owns=std::is_same_v<T,MilitaryActionAcceptanceConditions1>||std::is_same_v<T,MilitaryActionEndpointObservation2>;
 bool operator()(const StableId& key,const MilitaryActionAcceptanceConditions1&)const{return !key.empty();}
 bool operator()(const StableId& key,const MilitaryActionEndpointObservation2&)const{return !key.empty();}
};
template<class Saved>void shape(const StableId& id,const StableId& actor,const Saved& saved){
 if(id.empty()||actor.empty()||saved.participant_heads.empty())fail("MilitaryActionConditionOwnerRequired",id);bool actor_present=false;
 for(std::size_t i=0;i<saved.participant_heads.size();++i){const auto& head=saved.participant_heads[i];
  if(head.actor_id.empty()||head.condition_revision<0||head.initial_condition_seed_ref.seed_key.empty()||(head.condition_revision==0)!=!head.latest_receipt_ref||(head.latest_receipt_ref&&head.latest_receipt_ref->result_id.empty()))fail("MilitaryActionConditionHeadInvalid",id);
  for(std::size_t j=0;j<i;++j)if(saved.participant_heads[j].actor_id==head.actor_id)fail("MilitaryActionConditionParticipantDuplicate",head.actor_id);actor_present=actor_present||head.actor_id==actor;
 }if(!actor_present)fail("MilitaryActionConditionActorMissing",id);
}
void formation(const DynamicMilitaryActionFact2& row,const MilitaryActionAcceptanceConditions1& saved){
 const auto* terms=std::get_if<ArmyFormationTerms>(&row.original_terms);
 if(row.command!="army_form"||row.rule_version!=2||!terms||row.actor_id!=terms->actor_id||(row.phase!="started"&&row.phase!="completed"))fail("MilitaryFormationConditionOwnerRequired",row.id);shape(row.id,row.actor_id,saved);
 if(terms->member_actor_ids.size()!=saved.participant_heads.size())fail("MilitaryActionConditionParticipantsMismatch",row.id);
 for(std::size_t i=0;i<terms->member_actor_ids.size();++i)if(terms->member_actor_ids[i]!=saved.participant_heads[i].actor_id)fail("MilitaryActionConditionParticipantsMismatch",row.id);
}
void returning(const DynamicMilitaryLogisticsFact2& row,const MilitaryActionAcceptanceConditions1& saved){
 if(row.command!="army_return"||row.rule_version!=2||row.actor_id!=row.original_terms.actor_id||(row.phase!="started"&&row.phase!="completed"))fail("MilitaryReturnConditionOwnerRequired",row.id);shape(row.id,row.actor_id,saved);
}
template<class A>Status fields(A& archive,Staging& staging,Integer selected){
 if(selected!=1)return archive.reject("MilitaryActionConditionSelectedSchemaRequired");Integer schema=selected;archive.field("schema",schema);if(!archive.status().ok)return archive.status();if(schema!=selected)return archive.reject("MilitaryActionConditionSelectedSchemaMismatch");
 military_state_codec::RootArchive<A,Keys> root{archive};
 root.table("formation_acceptances",staging.formation_acceptances,[](auto& child,auto& row){return military_action_condition_fields::value(child,row);});
 root.table("return_acceptances",staging.return_acceptances,[](auto& child,auto& row){return military_action_condition_fields::value(child,row);});return root.status();
}
void empty_targets(const MilitaryActionTimeState& state){
 for(const auto& item:state.dynamic_action_facts.rows())if(item.second.acceptance_conditions||item.second.acceptance_observation||item.second.completion_observation)fail("MilitaryActionConditionsAlreadyAttached",item.first);
 for(const auto& item:state.dynamic_logistics_facts.rows())if(item.second.acceptance_conditions)fail("MilitaryActionConditionsAlreadyAttached",item.first);
}
void collect(const MilitaryActionTimeState& state,Staging& staging){
 for(const auto& item:state.dynamic_action_facts.rows()){const auto& row=item.second;
  if(row.command!="army_form"){if(row.acceptance_conditions)fail("UnexpectedMilitaryActionConditions",item.first);continue;}
  if(item.first!=row.id||!row.acceptance_conditions)fail("MilitaryActionAcceptanceConditionsRequired",item.first);formation(row,*row.acceptance_conditions);if(!staging.formation_acceptances.insert(item.first,*row.acceptance_conditions))fail("MilitaryActionConditionDuplicate",item.first);
 }
 for(const auto& item:state.dynamic_logistics_facts.rows()){const auto& row=item.second;
  if(row.command!="army_return"){if(row.acceptance_conditions)fail("UnexpectedMilitaryActionConditions",item.first);continue;}
  if(item.first!=row.id||!row.acceptance_conditions)fail("MilitaryActionAcceptanceConditionsRequired",item.first);returning(row,*row.acceptance_conditions);if(!staging.return_acceptances.insert(item.first,*row.acceptance_conditions))fail("MilitaryActionConditionDuplicate",item.first);
 }
}
template<class T>CheckpointMemoryCharge reserve_row(const T& value){return CheckpointMemoryCharge(0);}
template<class T>struct PreparedRow {CheckpointMemoryCharge copied_fields;T row;explicit PreparedRow(const T& value):copied_fields(reserve_row(value)),row(value){}};
void attach(MilitaryActionTimeState& state,const Staging& staging){
 std::size_t formations=0,returns=0;
 for(const auto& item:state.dynamic_action_facts.rows())if(item.second.command=="army_form")++formations;
 for(const auto& item:state.dynamic_logistics_facts.rows())if(item.second.command=="army_return")++returns;
 if(formations!=staging.formation_acceptances.size()||returns!=staging.return_acceptances.size())fail("MilitaryActionConditionCoverageMismatch");
 for(const auto& item:staging.formation_acceptances.rows()){const auto* row=state.dynamic_action_facts.find(item.first);if(!row||row->id!=item.first)fail("MilitaryFormationConditionOrphan",item.first);formation(*row,item.second);}
 for(const auto& item:staging.return_acceptances.rows()){const auto* row=state.dynamic_logistics_facts.find(item.first);if(!row||row->id!=item.first)fail("MilitaryReturnConditionOrphan",item.first);returning(*row,item.second);}
 for(const auto& item:staging.formation_acceptances.rows()){PreparedRow<DynamicMilitaryActionFact2> candidate(*state.dynamic_action_facts.find(item.first));candidate.row.acceptance_conditions.emplace(item.second);state.dynamic_action_facts.set(item.first,candidate.row);}
 for(const auto& item:staging.return_acceptances.rows()){PreparedRow<DynamicMilitaryLogisticsFact2> candidate(*state.dynamic_logistics_facts.find(item.first));candidate.row.acceptance_conditions.emplace(item.second);state.dynamic_logistics_facts.set(item.first,candidate.row);}
}
template<class Visit>void each(const MilitaryActionTimeState& state,Visit visit){
 for(const auto& item:state.dynamic_action_facts.rows()){const auto& row=item.second;if(row.command!="army_form"){if(row.acceptance_conditions)fail("UnexpectedMilitaryActionConditions",item.first);continue;}if(item.first!=row.id||!row.acceptance_conditions)fail("MilitaryActionAcceptanceConditionsRequired",item.first);formation(row,*row.acceptance_conditions);visit(row.id,row.started_at,*row.acceptance_conditions,"formation_acceptance_conditions");}
 for(const auto& item:state.dynamic_logistics_facts.rows()){const auto& row=item.second;if(row.command!="army_return"){if(row.acceptance_conditions)fail("UnexpectedMilitaryActionConditions",item.first);continue;}if(item.first!=row.id||!row.acceptance_conditions)fail("MilitaryActionAcceptanceConditionsRequired",item.first);returning(row,*row.acceptance_conditions);visit(row.id,row.started_at,*row.acceptance_conditions,"return_acceptance_conditions");}
}
struct Staging2 {
 OrderedTable<MilitaryActionEndpointObservation2> formation_acceptance_observations,formation_completion_observations,office_acceptance_observations,office_completion_observations;
 OrderedTable<MilitaryActionAcceptanceConditions1> return_acceptances;
};
void no_endpoint_observations(const MilitaryActionTimeState& state){for(const auto& item:state.dynamic_action_facts.rows())if(item.second.acceptance_observation||item.second.completion_observation)fail("MilitaryActionEndpointObservationRequires2",item.first);}
void endpoint_shape(const DynamicMilitaryActionFact2& row,const MilitaryActionEndpointObservation2& saved,bool completed){
 if(row.rule_version!=2||(row.phase!="started"&&row.phase!="completed")||row.acceptance_conditions||bool(row.completed_at)!=(row.phase=="completed")||(completed&&row.phase!="completed"))fail("MilitaryActionEndpointOwnerRequired",row.id);shape(row.id,row.actor_id,saved);
 if(saved.common_prefix.common_fact_count<0||bool(saved.common_prefix.common_fact_count)!=bool(saved.common_prefix.last_common_fact)||saved.condition_prefix.applied_receipt_count<0||bool(saved.condition_prefix.applied_receipt_count)!=bool(saved.condition_prefix.last_applied_receipt)||(saved.condition_prefix.last_applied_receipt&&saved.condition_prefix.last_applied_receipt->result_id.empty()))fail("MilitaryActionConditionPrefixInvalid",row.id);
 if(row.command=="army_form"){const auto* terms=std::get_if<ArmyFormationTerms>(&row.original_terms);if(!terms||terms->actor_id!=row.actor_id||terms->member_actor_ids.size()!=saved.participant_heads.size())fail("MilitaryActionConditionParticipantsMismatch",row.id);for(std::size_t i=0;i<terms->member_actor_ids.size();++i)if(terms->member_actor_ids[i]!=saved.participant_heads[i].actor_id)fail("MilitaryActionConditionParticipantsMismatch",row.id);}
 else if(row.command=="office_accept"){const auto* terms=std::get_if<OfficeTerms>(&row.original_terms);if(!terms||terms->actor_id!=row.actor_id||saved.participant_heads.size()!=2||saved.participant_heads[0].actor_id!=terms->actor_id||saved.participant_heads[1].actor_id!=terms->provider_actor_id)fail("MilitaryActionConditionParticipantsMismatch",row.id);}
 else fail("MilitaryActionEndpointCommandRequired",row.id);
}
template<class Visit>void each_endpoint(const MilitaryActionTimeState& state,Visit visit){
 for(const auto& item:state.dynamic_action_facts.rows()){const auto& row=item.second;if(row.command!="army_form"&&row.command!="office_accept"){if(row.acceptance_conditions||row.acceptance_observation||row.completion_observation)fail("UnexpectedMilitaryActionConditions",item.first);continue;}
  if(item.first!=row.id||!row.acceptance_observation||bool(row.completion_observation)!=(row.phase=="completed"))fail("MilitaryActionEndpointObservationRequired",item.first);endpoint_shape(row,*row.acceptance_observation,false);
  visit(row,*row.acceptance_observation,MilitaryActionConditionEndpoint2::acceptance,row.command=="army_form"?"formation_acceptance_observation":"office_acceptance_observation");
  if(row.completion_observation){endpoint_shape(row,*row.completion_observation,true);visit(row,*row.completion_observation,MilitaryActionConditionEndpoint2::completion,row.command=="army_form"?"formation_completion_observation":"office_completion_observation");}
 }
}
template<class Visit>void each_return(const MilitaryActionTimeState& state,Visit visit){for(const auto& item:state.dynamic_logistics_facts.rows()){const auto& row=item.second;if(row.command!="army_return"){if(row.acceptance_conditions)fail("UnexpectedMilitaryActionConditions",item.first);continue;}if(item.first!=row.id||!row.acceptance_conditions)fail("MilitaryActionAcceptanceConditionsRequired",item.first);returning(row,*row.acceptance_conditions);visit(row.id,row.started_at,*row.acceptance_conditions,"return_acceptance_conditions");}}
template<class A>Status fields2(A& archive,Staging2& staging){
 Integer schema=2;archive.field("schema",schema);if(!archive.status().ok)return archive.status();if(schema!=2)return archive.reject("MilitaryActionConditionSelectedSchemaMismatch");military_state_codec::RootArchive<A,Keys> root{archive};
 root.table("formation_acceptance_observations",staging.formation_acceptance_observations,[](auto& c,auto& v){return military_action_condition_fields::value(c,v);});
 root.table("formation_completion_observations",staging.formation_completion_observations,[](auto& c,auto& v){return military_action_condition_fields::value(c,v);});
 root.table("office_acceptance_observations",staging.office_acceptance_observations,[](auto& c,auto& v){return military_action_condition_fields::value(c,v);});
 root.table("office_completion_observations",staging.office_completion_observations,[](auto& c,auto& v){return military_action_condition_fields::value(c,v);});
 root.table("return_acceptances",staging.return_acceptances,[](auto& c,auto& v){return military_action_condition_fields::value(c,v);});return root.status();
}
void collect2(const MilitaryActionTimeState& state,Staging2& staging){
 each_endpoint(state,[&](const DynamicMilitaryActionFact2& row,const MilitaryActionEndpointObservation2& saved,MilitaryActionConditionEndpoint2 endpoint,std::string_view){auto& table=row.command=="army_form"?(endpoint==MilitaryActionConditionEndpoint2::acceptance?staging.formation_acceptance_observations:staging.formation_completion_observations):(endpoint==MilitaryActionConditionEndpoint2::acceptance?staging.office_acceptance_observations:staging.office_completion_observations);if(!table.insert(row.id,saved))fail("MilitaryActionConditionDuplicate",row.id);});
 each_return(state,[&](const StableId& id,const WorldMoment&,const MilitaryActionAcceptanceConditions1& saved,std::string_view){if(!staging.return_acceptances.insert(id,saved))fail("MilitaryActionConditionDuplicate",id);});
}
void attach2(MilitaryActionTimeState& state,const Staging2& staging){
 std::size_t formations=0,formation_completed=0,offices=0,office_completed=0,returns=0;
 for(const auto& item:state.dynamic_action_facts.rows()){const auto& row=item.second;if(row.command=="army_form"){++formations;if(row.phase=="completed")++formation_completed;}else if(row.command=="office_accept"){++offices;if(row.phase=="completed")++office_completed;}}
 for(const auto& item:state.dynamic_logistics_facts.rows())if(item.second.command=="army_return")++returns;
 if(formations!=staging.formation_acceptance_observations.size()||formation_completed!=staging.formation_completion_observations.size()||offices!=staging.office_acceptance_observations.size()||office_completed!=staging.office_completion_observations.size()||returns!=staging.return_acceptances.size())fail("MilitaryActionConditionCoverageMismatch");
 const auto validate=[&](const auto& table,std::string_view command,bool completed){for(const auto& item:table.rows()){const auto* row=state.dynamic_action_facts.find(item.first);if(!row||row->id!=item.first||row->command!=command)fail("MilitaryActionEndpointOrphan",item.first);endpoint_shape(*row,item.second,completed);}};
 validate(staging.formation_acceptance_observations,"army_form",false);validate(staging.formation_completion_observations,"army_form",true);validate(staging.office_acceptance_observations,"office_accept",false);validate(staging.office_completion_observations,"office_accept",true);
 for(const auto& item:staging.return_acceptances.rows()){const auto* row=state.dynamic_logistics_facts.find(item.first);if(!row||row->id!=item.first)fail("MilitaryReturnConditionOrphan",item.first);returning(*row,item.second);}
 const auto publish=[&](const auto& accepted,const auto& completed){for(const auto& item:accepted.rows()){PreparedRow<DynamicMilitaryActionFact2> candidate(*state.dynamic_action_facts.find(item.first));candidate.row.acceptance_observation.emplace(item.second);if(candidate.row.phase=="completed"){const auto* ending=completed.find(item.first);if(!ending)fail("MilitaryActionCompletionObservationRequired",item.first);candidate.row.completion_observation.emplace(*ending);}state.dynamic_action_facts.set(item.first,candidate.row);}};
 publish(staging.formation_acceptance_observations,staging.formation_completion_observations);publish(staging.office_acceptance_observations,staging.office_completion_observations);
 for(const auto& item:staging.return_acceptances.rows()){PreparedRow<DynamicMilitaryLogisticsFact2> candidate(*state.dynamic_logistics_facts.find(item.first));candidate.row.acceptance_conditions.emplace(item.second);state.dynamic_logistics_facts.set(item.first,candidate.row);}
}
}
Status visit_military_action_conditions(CodecVisitor& c,MilitaryActionTimeState& state,Integer selected){return status([&]{try{
 if(selected!=1&&selected!=2)fail("MilitaryActionConditionSelectedSchemaRequired");
 if(selected==2){if(c.reading())empty_targets(state);Staging2 staging;if(!c.reading())collect2(state,staging);take(fields2(c,staging));if(c.reading())attach2(state,staging);return;}no_endpoint_observations(state);
 if(c.reading())empty_targets(state);Staging staging;if(!c.reading())collect(state,staging);take(fields(c,staging,selected));if(c.reading())attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status decode_military_action_conditions(const BoundaryValue& raw,MilitaryActionTimeState& state,Integer selected){return status([&]{try{
 if(selected!=1&&selected!=2)fail("MilitaryActionConditionSelectedSchemaRequired");if(!ScopedCheckpointJsonDecode::current())fail("CheckpointJsonContextRequired");empty_targets(state);ActorFoundationJsonVisitor c(raw);take(c.ordered_fields());if(selected==2){Staging2 staging;take(fields2(c,staging));take(c.finish());attach2(state,staging);return;}no_endpoint_observations(state);Staging staging;take(fields(c,staging,selected));take(c.finish());attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_action_conditions_fields(ProjectionWriter& sink,const MilitaryActionTimeState& state,Integer selected){return status([&]{try{
 if(selected!=1&&selected!=2)fail("MilitaryActionConditionSelectedSchemaRequired");ActorFoundationProjectionArchive c(sink);if(selected==2){Staging2 staging;collect2(state,staging);take(fields2(c,staging));return;}no_endpoint_observations(state);Staging staging;collect(state,staging);take(fields(c,staging,selected));
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_action_conditions(ProjectionWriter& sink,std::string_view name,const MilitaryActionTimeState& state,Integer selected){return sink.object(name,[&](auto& fields){return emit_military_action_conditions_fields(fields,state,selected);});}
Outcome<BoundaryValue> encode_military_action_conditions(const MilitaryActionTimeState& state,Integer selected){return guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);take(emit_military_action_conditions(sink,"",state,selected));take(sink.finish());return out;});}
Status validate_military_action_condition_layout(const MilitaryActionTimeState& state,Integer selected){return status([&]{
 if(selected==0){empty_targets(state);return;}if(selected==2){each_endpoint(state,[](const auto&,const auto&,auto,std::string_view){});each_return(state,[](const auto&,const auto&,const auto&,std::string_view){});return;}if(selected!=1)fail("MilitaryActionConditionSelectedSchemaRequired");no_endpoint_observations(state);each(state,[](const auto&,const auto&,const auto&,std::string_view){});
});}
Status validate_military_action_condition_sources(const MilitaryActionConditionContext& x,Integer selected){return status([&]{
 take(validate_military_action_condition_layout(x.actions,selected));if(selected==0)return;
 if(selected==2){each_endpoint(x.actions,[&](const auto& row,const auto&,auto endpoint,std::string_view){take(validate_military_action_endpoint_observation_source(x,row.id,endpoint,selected));});}
 const auto legacy=[&](const StableId& id,const WorldMoment& at,const MilitaryActionAcceptanceConditions1& saved,std::string_view){
  take(read_actual_common_fact_position(x.common,SourceRefBuildView{"military","military_action",id}));take(validate_military_action_condition_prefix(x,saved,at));
  if(const auto* action=x.common.action(id)){if(action->participants.size()!=saved.participant_heads.size())fail("MilitaryActionConditionParticipantsMismatch",id);for(std::size_t i=0;i<action->participants.size();++i)if(action->participants[i]!=saved.participant_heads[i].actor_id)fail("MilitaryActionConditionParticipantsMismatch",id);}
 };if(selected==2)each_return(x.actions,legacy);else each(x.actions,legacy);
});}
namespace {
template<class Emit>class EndpointCommonPins final:public CommonActualFactPrefixSink {
 Emit& emit_;const StableId& id_;std::string_view kind_;
public:
 EndpointCommonPins(Emit& emit,const StableId& id,std::string_view kind):emit_(emit),id_(id),kind_(kind){}
 Status actual_prefix_fact(std::size_t,const FactRecord& fact)override{return emit_(SourcePinBuildView{"military",kind_,id_,"common_prefix_fact",fact.domain,fact.kind,fact.id,SourcePinReason::dependent_state});}
};
template<class Emit>Status visit_pins(const MilitaryActionConditionContext& x,Integer selected,Emit emit){return status([&]{
 take(validate_military_action_condition_sources(x,selected));
 const auto heads=[&](const StableId& id,const auto& saved,std::string_view kind){for(const auto& head:saved.participant_heads){take(emit(SourcePinBuildView{"military",kind,id,"participant_initial_seed","actor_foundation","initial_condition_seed",head.initial_condition_seed_ref.seed_key,SourcePinReason::dependent_state}));if(head.latest_receipt_ref)take(emit(SourcePinBuildView{"military",kind,id,"participant_latest_receipt","actor_foundation","condition_receipt",head.latest_receipt_ref->result_id,SourcePinReason::dependent_state}));}};
 const auto legacy=[&](const StableId& id,const WorldMoment&,const MilitaryActionAcceptanceConditions1& saved,std::string_view kind){heads(id,saved,kind);};
 if(selected==0)return;if(selected==1){each(x.actions,legacy);return;}each_return(x.actions,legacy);
 each_endpoint(x.actions,[&](const DynamicMilitaryActionFact2& row,const MilitaryActionEndpointObservation2& saved,MilitaryActionConditionEndpoint2,std::string_view kind){
  heads(row.id,saved,kind);EndpointCommonPins<Emit> common(emit,row.id,kind);take(visit_actual_common_fact_prefix(x.common,saved.common_prefix,common));
  // Count is anchored to this whole original application order. Retaining
  // only its last receipt would not preserve the captured prefix.
  take(validate_character_condition_applied_prefix(x.sources,saved.condition_prefix));const auto& receipts=x.foundation.character_condition_extensions.applied_receipts.rows();
  for(std::size_t i=0;i<static_cast<std::size_t>(saved.condition_prefix.applied_receipt_count);++i)take(emit(SourcePinBuildView{"military",kind,row.id,"condition_prefix_receipt","actor_foundation","condition_receipt",receipts[i].second.result_id,SourcePinReason::dependent_state}));
 });
});}
}
Status collect_military_action_condition_pins(const MilitaryActionConditionContext& x,SourcePinSink& sink,Integer selected){return visit_pins(x,selected,[&](const SourcePinBuildView& pin){return sink.contains(pin)?common_success():sink.append(pin);});}
Status validate_military_action_condition_pin(const MilitaryActionConditionContext& x,const SourcePin& input,Integer selected){return status([&]{
 bool found=false;take(visit_pins(x,selected,[&](const SourcePinBuildView& pin){if(pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id&&pin.reason==input.reason)found=true;return common_success();}));if(!found)fail("MilitaryActionConditionPinRequired",input.consumer.id);
});}
std::vector<RequiredSourceConsumerSlot> military_action_condition_consumer_slots(){
 std::vector<RequiredSourceConsumerSlot> slots{{"military","formation_acceptance_conditions","participant_initial_seed"},{"military","formation_acceptance_conditions","participant_latest_receipt"},{"military","return_acceptance_conditions","participant_initial_seed"},{"military","return_acceptance_conditions","participant_latest_receipt"}};
 for(const auto* kind:{"formation_acceptance_observation","formation_completion_observation","office_acceptance_observation","office_completion_observation"})for(const auto* field:{"participant_initial_seed","participant_latest_receipt","common_prefix_fact","condition_prefix_receipt"})slots.push_back({"military",kind,field});return slots;
}
Outcome<bool> military_action_condition_source_may_release(const MilitaryActionConditionContext& x,const SourceRef& source,Integer schema,Integer selected){return guard<bool>([&]{
 if(source.domain!="actor_foundation"||(source.kind!="initial_condition_seed"&&source.kind!="condition_receipt"))fail("MilitaryActionConditionSourceRequired",source.id);
 const auto source_selected=source.kind=="initial_condition_seed"?take(read_initial_condition_seed_schema(x.sources.catalog)):Integer(2);if(schema!=source_selected)fail("MilitaryActionConditionSourceSchemaMismatch",source.id);bool found=false;
 take(visit_pins(x,selected,[&](const SourcePinBuildView& pin){if(pin.source_domain==source.domain&&pin.source_kind==source.kind&&pin.source_id==source.id)found=true;return common_success();}));if(!found)fail("MilitaryActionConditionSourceRequired",source.id);return false;
});}
}
