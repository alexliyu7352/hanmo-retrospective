#include "condition_capacity.hpp"
#include "authored_origin.hpp"
#include "condition_source_read.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>
namespace hanmo::resident { namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
bool same(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b))==0;}
Integer original_serial(const CommonStore& common,const StableId& id,std::string_view prefix){if(id.size()<=prefix.size()||id.compare(0,prefix.size(),prefix)!=0)fail("ConditionCapacityOriginalIdRequired",id);const std::string_view text(id.data()+prefix.size(),id.size()-prefix.size());if(text.front()=='0')fail("ConditionCapacityOriginalIdRequired",id);Integer value=0;auto read=std::from_chars(text.data(),text.data()+text.size(),value);if(read.ec!=std::errc{}||read.ptr!=text.data()+text.size()||value<=0||value>=common.state().next_instance_id)fail("ConditionCapacityOriginalIdRequired",id);return value;}

void selected(const CharacterConditionSources& body){if(!condition_purpose_capacity_selected(body.catalog))fail("ConditionPurposeCapacityVersion4Required",{},ErrorKind::unsupported_capability);take(validate_character_capability_catalog(body.catalog));}
const ConditionPurposeCapacityCause* cause(const CapacityTransitionV2& value){return std::get_if<ConditionPurposeCapacityCause>(&value.cause_ref);}
const CapacityTransitionV2* actual_for(const CharacterConditionSources& body,const ConditionReceiptV2& receipt,const ActorFoundationRuleRef& purpose){
 const CapacityTransitionV2* found=nullptr;
 for(const auto& pair:body.state.character_condition_extensions.capacity_transitions.rows())if(const auto* original=cause(pair.second);original&&original->condition_receipt_ref.result_id==receipt.result_id&&actor_foundation_rule_equal(pair.second.purpose,purpose)){
  if(found||pair.first!=pair.second.transition_id)fail("DuplicateConditionPurposeCapacity",receipt.result_id);found=&pair.second;
 }return found;
}
void row(const CharacterConditionSources& body,const CapacityTransitionV2& value){
 selected(body);const auto* original=cause(value);if(!original)fail("ConditionPurposeCapacityCauseRequired",value.transition_id);
 const auto actual=take(read_character_purpose_change_at_receipt(body,original->condition_receipt_ref,value.purpose));
 if(!actual.receipt||!actual.purpose_binding||original_serial(body.common,value.transition_id,"capacity:")<=original_serial(body.common,actual.receipt->result_id,"condition:")||value.actor_id!=actual.receipt->actor_id||value.from==value.to||value.from!=actual.before_allowed||value.to!=actual.after_allowed||value.expected_recovery_at||value.effective_at.kind!=ActorFoundationAt::Kind::exact_moment||!same(value.effective_at.exact_moment,actual.receipt->effective_at.exact_moment))fail("ConditionPurposeCapacityOriginalMismatch",value.transition_id);
 if(body.common.state().facts.find(value.transition_id))fail("ConditionCapacityMustNotHaveCommonHeader",value.transition_id);
 if(std::count(actual.receipt->capacity_transition_refs.begin(),actual.receipt->capacity_transition_refs.end(),value.transition_id)!=1)fail("ConditionPurposeCapacityReceiptLinkMissing",value.transition_id);
}
void receipt_outputs(const CharacterConditionSources& body,const ConditionReceiptV2& receipt){
 if(!original_condition_capacity_refs_shape(body.catalog,receipt))fail("ConditionCapacityReceiptReferenceShape",receipt.result_id);
 std::size_t ordinal=0;Integer previous_serial=original_serial(body.common,receipt.result_id,"condition:");
 for(const auto& binding:body.catalog.capability_purpose_bindings.rows()){
  const auto actual=take(read_character_purpose_change_at_receipt(body,{receipt.result_id},binding.second.purpose_ref));
  const auto* transition=actual_for(body,receipt,binding.second.purpose_ref);
  if(actual.before_allowed==actual.after_allowed){if(transition)fail("UnchangedPurposeHasCapacityTransition",transition->transition_id);continue;}
  if(!transition||ordinal>=receipt.capacity_transition_refs.size()||receipt.capacity_transition_refs[ordinal]!=transition->transition_id)fail("ConditionPurposeCapacityCoverageMismatch",receipt.result_id);
  row(body,*transition);const auto serial=original_serial(body.common,transition->transition_id,"capacity:");if(serial<=previous_serial)fail("ConditionCapacityOutputOrderMismatch",transition->transition_id);previous_serial=serial;++ordinal;
 }
 if(ordinal!=receipt.capacity_transition_refs.size())fail("ConditionPurposeCapacityExtraOutput",receipt.result_id);
}
}
SourceRef character_capacity_transition_source(const StableId& id){return {"actor_foundation","capacity_transition",id};}
Status record_actual_condition_capacity_changes_in_candidate(CommonStore& common,AuthoredActorFoundationState& state,const CharacterConditionSources& body,const BoundActorConditionWrite& token){try{
 selected(body);if(&common!=&body.common||&state!=&body.state)fail("ConditionCapacitySameCandidateRequired");
 const auto* captured=take(read_actual_actor_condition_write(common,state,token));
 auto* receipt=state.character_condition_extensions.applied_receipts.find(captured->result_id);
 if(!receipt||receipt!=captured||receipt->effective_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("ConditionCapacityActualReceiptRequired",token.receipt_ref.result_id);
 if(!receipt->capacity_transition_refs.empty()){receipt_outputs(body,*receipt);return common_success();}
 bool any=false;
 for(const auto& binding:body.catalog.capability_purpose_bindings.rows()){
  const auto actual=take(read_character_purpose_change_at_receipt(body,token.receipt_ref,binding.second.purpose_ref));
  if(actual_for(body,*receipt,binding.second.purpose_ref))fail("ConditionCapacityOrphanOutput",receipt->result_id);
  any=any||actual.before_allowed!=actual.after_allowed;
 }
 if(!any)return common_success();
 const auto* head=state.character_condition_extensions.actor_condition_heads.find(receipt->actor_id);
 if(!head||!head->latest_receipt_ref||head->latest_receipt_ref->result_id!=receipt->result_id||head->condition_revision!=receipt->condition_revision_after||!same(receipt->effective_at.exact_moment,body.actual_now))fail("ConditionCapacityImmediateActualWriteRequired",receipt->result_id);
 for(const auto& binding:body.catalog.capability_purpose_bindings.rows()){
  const auto actual=take(read_character_purpose_change_at_receipt(body,token.receipt_ref,binding.second.purpose_ref));if(actual.before_allowed==actual.after_allowed)continue;
  CapacityTransitionV2 result;result.transition_id=take(common.next_id("capacity"));result.actor_id=receipt->actor_id;result.purpose=binding.second.purpose_ref;result.from=actual.before_allowed;result.to=actual.after_allowed;result.effective_at=receipt->effective_at;result.cause_ref=ConditionPurposeCapacityCause{token.receipt_ref};
  const auto id=result.transition_id;if(!state.character_condition_extensions.capacity_transitions.insert(id,std::move(result)))fail("DuplicateCapacityTransition",id);receipt->capacity_transition_refs.push_back(id);
 }
 receipt_outputs(body,*receipt);return common_success();
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Status validate_condition_purpose_capacity_coverage(const CharacterConditionSources& body){try{
 const bool enabled=condition_purpose_capacity_selected(body.catalog);
 if(enabled)selected(body);
 for(const auto& pair:body.state.character_condition_extensions.capacity_transitions.rows())if(cause(pair.second)){
  if(!enabled)fail("ConditionPurposeCapacityVersion4Required",pair.first,ErrorKind::unsupported_capability);if(pair.first!=pair.second.transition_id)fail("ConditionCapacityKeyMismatch",pair.first);row(body,pair.second);
 }
 Integer previous_output=0;
 for(const auto& pair:body.state.character_condition_extensions.applied_receipts.rows()){
  if(enabled){receipt_outputs(body,pair.second);for(const auto& id:pair.second.capacity_transition_refs){const auto serial=original_serial(body.common,id,"capacity:");if(serial<=previous_output)fail("ConditionCapacityReceiptApplicationOrderMismatch",id);previous_output=serial;}}else if(!pair.second.capacity_transition_refs.empty())fail("ConditionPurposeCapacityVersion4Required",pair.first,ErrorKind::unsupported_capability);
 }
 return common_success();
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Outcome<const CapacityTransitionV2*> read_verified_condition_purpose_capacity_transition(const CharacterConditionSources& body,const SourceRef& ref,Integer schema){try{
 if(ref.domain!="actor_foundation"||ref.kind!="capacity_transition"||schema!=2)fail("ConditionCapacityTypedSourceRequired",ref.id,ErrorKind::unsupported_capability);
 selected(body);const auto* value=body.state.character_condition_extensions.capacity_transitions.find(ref.id);if(!value||value->transition_id!=ref.id)fail("ConditionCapacityTransitionMissing",ref.id);row(body,*value);
 const auto* original=cause(*value);const auto* receipt=body.state.character_condition_extensions.applied_receipts.find(original->condition_receipt_ref.result_id);if(!receipt)fail("ConditionCapacityOriginalReceiptMissing",ref.id);receipt_outputs(body,*receipt);return {true,value,{}};
 }catch(Failure& error){return {false,nullptr,std::move(error.error)};}}
Status visit_condition_purpose_capacity_source_pins(const CharacterConditionSources& body,CharacterConditionPinSink& sink){try{
 take(validate_condition_purpose_capacity_coverage(body));for(const auto& pair:body.state.character_condition_extensions.capacity_transitions.rows())if(const auto* original=cause(pair.second)){
  take(sink.actual_pin({{"actor_foundation","capacity_transition",pair.first,"condition_receipt_ref"},character_condition_receipt_source(original->condition_receipt_ref),SourcePinReason::dependent_state}));
 }
 for(const auto& pair:body.state.character_condition_extensions.applied_receipts.rows())for(const auto& id:pair.second.capacity_transition_refs)
  take(sink.actual_pin({{"actor_foundation","condition_receipt",pair.first,"capacity_transition_refs"},character_capacity_transition_source(id),SourcePinReason::unique_outcome}));
 return common_success();
 }catch(Failure& error){return {false,{},std::move(error.error)};}}
Status validate_condition_purpose_capacity_source_pin(const CharacterConditionSources& body,const SourcePin& pin){
 struct Find final:CharacterConditionPinSink{const SourcePin& target;std::size_t count=0;explicit Find(const SourcePin& p):target(p){}Status actual_pin(const SourcePin& p)override{if(p.consumer.domain==target.consumer.domain&&p.consumer.kind==target.consumer.kind&&p.consumer.id==target.consumer.id&&p.consumer.slot==target.consumer.slot&&same_source_ref(p.source,target.source)&&p.reason==target.reason)++count;return common_success();}} find(pin);
 auto valid=visit_condition_purpose_capacity_source_pins(body,find);if(!valid.ok)return valid;return find.count==1?common_success():common_failure("ConditionCapacityPinNotActual",pin.consumer.id,ErrorKind::internal);
}
Outcome<bool> condition_purpose_capacity_source_owner_may_release(const CharacterConditionSources& body,const SourceRef& ref){
 auto original=read_verified_condition_purpose_capacity_transition(body,ref,2);if(!original.ok)return {false,false,original.error};
 struct Find final:CharacterConditionPinSink{const SourceRef& target;bool used=false;explicit Find(const SourceRef& r):target(r){}Status actual_pin(const SourcePin& pin)override{if(same_source_ref(pin.source,target))used=true;return common_success();}} find(ref);
 auto status=visit_condition_purpose_capacity_source_pins(body,find);if(!status.ok)return {false,false,status.error};return {true,!find.used,{}};
}

}
