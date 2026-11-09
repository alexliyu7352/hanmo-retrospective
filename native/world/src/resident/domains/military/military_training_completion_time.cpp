#include "military_training_completion_time.hpp"
#include "military_action_occupation_source.hpp"
#include "training_exact_detail.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
void terminal_shape(const DynamicMilitaryTrainingFact2& f){
 if(f.id.empty()||f.rule_version!=2||f.actor_id!=f.terms.actor_id||f.phase!=DynamicMilitaryTrainingPhase::completed||!f.completed_at||!f.terminal_source_ref||!source_valid(*f.terminal_source_ref)||f.elapsed_source_ref.action_ref!=f.id||f.work_binding_ref.action_id!=f.id||f.work_binding_ref.action_kind!="military_train"||(f.work_binding_ref.source_owner_ref.domain!="military"||f.work_binding_ref.source_owner_ref.kind!="military_training"||f.work_binding_ref.source_owner_ref.id!=f.id)||f.terms.duration!=4||f.lessons.size()>4||f.success!=(f.lessons.size()==4&&!f.terminal_code))fail("MilitaryTrainingCompletionOwnerMismatch",f.id);
 if(f.success){if(f.lessons.empty()||!same_source(*f.terminal_source_ref,f.lessons.back().fee_effect_ref))fail("MilitaryTrainingCompletionCauseMismatch",f.id);}
 else if(!f.terminal_code||f.terminal_code->empty())fail("MilitaryTrainingCompletionCauseRequired",f.id);
}
void application_shape(const DynamicMilitaryTrainingFact2& f,const SourceRef& source){
 terminal_shape(f);if(source.domain!="military"||source.kind!="military_training_completion_applied"||source.id.empty()||source.id==f.id||source.id==f.terminal_source_ref->id)fail("MilitaryTrainingCompletionSourceMismatch",f.id);
}
const DynamicMilitaryTrainingFact2& find_application(const MilitaryActionTimeState& state,const SourceRef& source){
 const DynamicMilitaryTrainingFact2* found=nullptr;
 for(const auto& item:state.dynamic_training_facts.rows())if(item.second.completion_application_source&&item.second.completion_application_source->id==source.id){
  const auto& row=item.second;if(found||item.first!=row.id||!same_source(*row.completion_application_source,source))fail("MilitaryTrainingCompletionSourceAmbiguous",source.id);application_shape(row,source);found=&row;
 }
 if(!found)fail("MilitaryTrainingCompletionSourceRequired",source.id);return *found;
}
void legacy_collision(const MilitaryState& military,const StableId& id){for(const auto& row:military.training_facts)if(row.id==id)fail("DuplicateLegacyExactMilitaryAction",id);}
}
Status validate_military_training_completion_time_layout(const MilitaryActionTimeState& state,Integer selected){return status([&]{
 if(selected!=0&&selected!=1)fail("MilitaryTrainingCompletionTimeSelectedSchemaRequired");
 for(const auto& item:state.dynamic_training_facts.rows()){
  const auto& row=item.second;if(item.first!=row.id||row.rule_version!=2)fail("MilitaryTrainingCompletionOwnerMismatch",item.first);
  if(selected==0){if(row.completion_application_source)fail("UnexpectedMilitaryTrainingCompletionTime",row.id);continue;}
  if(row.phase==DynamicMilitaryTrainingPhase::completed){if(!row.completion_application_source)fail("MilitaryTrainingCompletionSourceRequired",row.id);application_shape(row,*row.completion_application_source);find_application(state,*row.completion_application_source);}
  else if(row.phase!=DynamicMilitaryTrainingPhase::started||row.completion_application_source)fail("MilitaryTrainingCompletionPhaseMismatch",row.id);
 }
});}
Status validate_military_training_completion_header_source(const FactRecord& head,const MilitaryState& military,const MilitaryActionTimeState& state,Integer selected){return status([&]{
 if(selected!=1||head.id.empty()||head.domain!="military"||head.kind!="military_training_completion_applied")fail("MilitaryTrainingCompletionHeaderRequired",head.id);
 const DynamicMilitaryTrainingFact2* found=nullptr;
 for(const auto& item:state.dynamic_training_facts.rows())if(item.second.completion_application_source&&item.second.completion_application_source->id==head.id){
  const auto& row=item.second;const auto& source=*row.completion_application_source;
  if(found||item.first!=row.id||source.domain!=head.domain||source.kind!=head.kind)fail("MilitaryTrainingCompletionHeaderMismatch",head.id);application_shape(row,source);legacy_collision(military,row.id);found=&row;
 }if(!found)fail("MilitaryTrainingCompletionSourceRequired",head.id);
});}
Status emit_military_training_completion_header_payload_fields(ProjectionWriter& sink,const FactRecord& head,const MilitaryState& military,const MilitaryActionTimeState& state,Integer selected){
 auto valid=validate_military_training_completion_header_source(head,military,state,selected);if(!valid.ok)return valid;auto kind=sink.text("kind",head.kind);if(!kind.ok)return kind;return sink.text("domain",head.domain);
}
Status validate_exact_training_completion_header(const CommonStore& common,const MilitaryState& military,const MilitaryActionTimeState& state,const DynamicMilitaryTrainingFact2& row){return status([&]{
 if(!row.completion_application_source)fail("MilitaryTrainingCompletionSourceRequired",row.id);const auto& source=*row.completion_application_source;
 if(&find_application(state,source)!=&row)fail("MilitaryTrainingCompletionOriginalRowRequired",row.id);
 const auto* head=common.state().facts.find(source.id);if(!head)fail("MilitaryTrainingCompletionHeaderRequired",source.id);take(validate_military_training_completion_header_source(*head,military,state,1));
 const auto start=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_training",row.id}));const auto end=take(read_actual_common_fact_position(common,source));if(start>=end)fail("MilitaryTrainingCompletionOrderMismatch",row.id);
 // An external eligibility cause may be an owned non-Common source. Its own
 // source reader remains responsible for occurrence; never promote it to a head.
 const auto& cause=*row.terminal_source_ref;if(const auto* actual=common.state().facts.find(cause.id)){
  if(actual->domain!=cause.domain||actual->kind!=cause.kind||take(read_actual_common_fact_position(common,cause))>=end)fail("MilitaryTrainingCompletionCauseOrderMismatch",row.id);
 }else if(cause.domain=="military"&&(cause.kind=="military_training_cancelled"||cause.kind=="military_training_lesson"||cause.kind=="military_supply"))fail("MilitaryTrainingCompletionCauseHeaderRequired",row.id);
});}
Outcome<const DynamicMilitaryTrainingFact2*> read_exact_training_completion_application(const MilitaryTrainingCompletionContext& x,const SourceRef& ref,Integer schema){return guard<const DynamicMilitaryTrainingFact2*>([&]{
 if(schema!=2||ref.domain!="military"||ref.kind!="military_training_completion_applied")fail("MilitaryTrainingCompletionSourceRequired",ref.id);const auto& row=find_application(x.actions,ref);take(validate_exact_training_completion_header(x.common,x.military,x.actions,row));
 // Reuse the one original five-kind occupation/history implementation. Its
 // training branch reads only row, actual S0 chain/binding and Common envelope;
 // it never calls professional acceptance, lessons or this reader recursively.
 MilitaryActionTimeReadSelection selected;selected.military_train=true;
 NativeMilitaryActionOccupationSourceRead original({x.common,x.military,x.actions,x.elapsed,x.scale,x.clock,selected,nullptr,nullptr,x.budget});
 const auto proof=take(original.read_original_occupation_source(row.id));
 if(!proof.ended_at||!proof.terminal_transition||!proof.elapsed||!same_moment(*proof.ended_at,*row.completed_at,x.budget))fail("MilitaryTrainingCompletionExecutionRequired",row.id);
 return &row;
});}
Status validate_military_training_completion_time_sources(const MilitaryTrainingCompletionContext& x,Integer selected){return status([&]{
 take(validate_military_training_completion_time_layout(x.actions,selected));
 for(const auto& item:x.common.state().facts.rows())if(item.second.domain=="military"&&item.second.kind=="military_training_completion_applied"){
  if(selected!=1||item.first!=item.second.id)fail("MilitaryTrainingCompletionHeaderMismatch",item.first);take(validate_military_training_completion_header_source(item.second,x.military,x.actions,selected));
 }
 if(selected==0)return;
 for(const auto& item:x.actions.dynamic_training_facts.rows())if(item.second.completion_application_source)take(read_exact_training_completion_application(x,*item.second.completion_application_source,2));
});}
namespace {
template<class Emit>Status pins(const MilitaryTrainingCompletionContext& x,Emit emit){return status([&]{
 take(validate_military_training_completion_time_sources(x,1));
 for(const auto& item:x.actions.dynamic_training_facts.rows())if(item.second.completion_application_source){const auto& row=item.second;const auto& source=*row.completion_application_source;const auto& cause=*row.terminal_source_ref;
  take(emit(SourcePinBuildView{"military","exact_training_action",row.id,"completion_application",source.domain,source.kind,source.id,SourcePinReason::unique_outcome}));
  take(emit(SourcePinBuildView{"military","training_completion_application",source.id,"original_training","military","military_training",row.id,SourcePinReason::unique_outcome}));
  take(emit(SourcePinBuildView{"military","training_completion_application",source.id,"original_terminal_cause",cause.domain,cause.kind,cause.id,SourcePinReason::dependent_state}));
 }
});}
}
Status collect_military_training_completion_time_pins(const MilitaryTrainingCompletionContext& x,SourcePinSink& sink){return pins(x,[&](const SourcePinBuildView& p){return sink.contains(p)?common_success():sink.append(p);});}
Status validate_military_training_completion_time_pin(const MilitaryTrainingCompletionContext& x,const SourcePin& input){return status([&]{bool found=false;take(pins(x,[&](const SourcePinBuildView& p){if(p.consumer_domain==input.consumer.domain&&p.consumer_kind==input.consumer.kind&&p.consumer_id==input.consumer.id&&p.consumer_slot==input.consumer.slot&&p.source_domain==input.source.domain&&p.source_kind==input.source.kind&&p.source_id==input.source.id&&p.reason==input.reason)found=true;return common_success();}));if(!found)fail("MilitaryTrainingCompletionPinRequired",input.consumer.id);});}
std::vector<RequiredSourceConsumerSlot> military_training_completion_time_consumer_slots(){return {{"military","exact_training_action","completion_application"},{"military","training_completion_application","original_training"},{"military","training_completion_application","original_terminal_cause"}};}
Outcome<bool> military_training_completion_time_source_may_release(const MilitaryTrainingCompletionContext& x,const SourceRef& ref,Integer schema){return guard<bool>([&]{take(read_exact_training_completion_application(x,ref,schema));return false;});}
}
