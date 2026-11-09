#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_return_detachment_codec.hpp"
#include "military_return_detachment_fields.hpp"
#include "military_state_codec_archive.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
struct Staging {OrderedTable<MilitaryReturnDetachmentReleases1> return_receipts;};
struct Keys {
 template<class T>static constexpr bool owns=std::is_same_v<T,MilitaryReturnDetachmentReleases1>;
 bool operator()(const StableId& key,const MilitaryReturnDetachmentReleases1&)const{return !key.empty();}
};
const DynamicMilitaryLogisticsFact2& return_owner(const MilitaryActionTimeState& state,const DynamicMilitaryReturnFact2& row){
 const auto* owner=state.dynamic_logistics_facts.find(row.logistics_action_ref);
 if(!owner||owner->id!=row.logistics_action_ref||owner->command!="army_return"||owner->rule_version!=2||owner->phase!="completed"||!owner->success||!owner->completed_at||!owner->result||!owner->result->ok.value_or(false)||owner->result->receipt_id!=row.id||!owner->terminal_source_ref||!same_source(*owner->terminal_source_ref,row.completion_source_ref))fail("MilitaryReturnDetachmentOriginalResultRequired",row.id);
 return *owner;
}
void payload(const MilitaryReturnDetachmentReleases1& value){
 for(const auto& release:value.releases)take(validate_original_lost_estate_retainer_release({release.estate_id,release.actor_id,release.source_id,release.detached_before?&*release.detached_before:nullptr,release.detached_after?&*release.detached_after:nullptr}));
}
template<class A>Status fields(A& archive,Staging& staging,Integer selected){
 if(selected!=1)return archive.reject("MilitaryReturnDetachmentSelectedSchemaRequired");
 Integer schema=selected;archive.field("schema",schema);if(!archive.status().ok)return archive.status();if(schema!=selected)return archive.reject("MilitaryReturnDetachmentSelectedSchemaMismatch");
 military_state_codec::RootArchive<A,Keys> root{archive};
 root.table("return_receipts",staging.return_receipts,[](auto& child,auto& row){return military_return_detachment_fields::value(child,row);});return root.status();
}
void empty_targets(const MilitaryActionTimeState& state){
 for(const auto& row:state.dynamic_return_facts.rows())if(row.second.actual_detachment_releases)fail("MilitaryReturnDetachmentAdjunctAlreadyAttached",row.first);
}
void collect(const MilitaryActionTimeState& state,Staging& staging){
 take(validate_military_return_detachment_layout(state,1));
 for(const auto& item:state.dynamic_return_facts.rows())if(!staging.return_receipts.insert(item.first,*item.second.actual_detachment_releases))fail("MilitaryReturnDetachmentDuplicate",item.first);
}
CheckpointMemoryCharge reserve_row(const DynamicMilitaryReturnFact2& v){return CheckpointMemoryCharge(0);}
struct PreparedRow {CheckpointMemoryCharge copied_fields;DynamicMilitaryReturnFact2 row;explicit PreparedRow(const DynamicMilitaryReturnFact2& v):copied_fields(reserve_row(v)),row(v){}};
void attach(MilitaryActionTimeState& state,const Staging& staging){
 if(state.dynamic_return_facts.size()!=staging.return_receipts.size())fail("MilitaryReturnDetachmentAdjunctCoverageMismatch");
 for(const auto& item:staging.return_receipts.rows()){
  const auto* row=state.dynamic_return_facts.find(item.first);
  if(!row||row->id!=item.first)fail("MilitaryReturnDetachmentOrphan",item.first);
  return_owner(state,*row);payload(item.second);
 }
 for(const auto& item:staging.return_receipts.rows()){
  PreparedRow candidate(*state.dynamic_return_facts.find(item.first));
  candidate.row.actual_detachment_releases.emplace(item.second);
  state.dynamic_return_facts.set(item.first,candidate.row);
 }
}
}
Status validate_military_return_detachment_layout(const MilitaryActionTimeState& state,Integer selected){return status([&]{
 if(selected==0){empty_targets(state);return;}if(selected!=1)fail("MilitaryReturnDetachmentSelectedSchemaRequired");
 for(const auto& item:state.dynamic_return_facts.rows()){
  const auto& row=item.second;if(row.id.empty()||item.first!=row.id)fail("MilitaryReturnDetachmentOwnerKeyMismatch",item.first);
  return_owner(state,row);if(!row.actual_detachment_releases)fail("MilitaryReturnDetachmentCaptureRequired",row.id);payload(*row.actual_detachment_releases);
 }
});}
Status visit_military_return_detachments(CodecVisitor& c,MilitaryActionTimeState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryReturnDetachmentSelectedSchemaRequired");
 if(c.reading())empty_targets(state);Staging staging;if(!c.reading())collect(state,staging);take(fields(c,staging,selected));if(c.reading())attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status decode_military_return_detachments(const BoundaryValue& raw,MilitaryActionTimeState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryReturnDetachmentSelectedSchemaRequired");if(!ScopedCheckpointJsonDecode::current())fail("CheckpointJsonContextRequired");empty_targets(state);Staging staging;ActorFoundationJsonVisitor c(raw);take(c.ordered_fields());take(fields(c,staging,selected));take(c.finish());attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_return_detachments_fields(ProjectionWriter& sink,const MilitaryActionTimeState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryReturnDetachmentSelectedSchemaRequired");Staging staging;collect(state,staging);ActorFoundationProjectionArchive c(sink);take(fields(c,staging,selected));
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_return_detachments(ProjectionWriter& sink,std::string_view name,const MilitaryActionTimeState& state,Integer selected){return sink.object(name,[&](auto& child){return emit_military_return_detachments_fields(child,state,selected);});}
Outcome<BoundaryValue> encode_military_return_detachments(const MilitaryActionTimeState& state,Integer selected){return guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);take(emit_military_return_detachments(sink,"",state,selected));take(sink.finish());return out;});}
Status validate_military_return_detachment_sources(const CommonStore& common,const MilitaryActionTimeState& state,Integer selected){return status([&]{
 take(validate_military_return_detachment_layout(state,selected));if(selected==0)return;
 for(const auto& item:state.dynamic_return_facts.rows()){
  const auto& row=item.second;const auto& action=return_owner(state,row);
  if(row.completion_source_ref.domain!="military"||row.completion_source_ref.kind!="military_logistics_completion_applied")fail("MilitaryReturnDetachmentOriginalResultRequired",row.id);
  const auto begin=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_action",action.id}));
  const auto receipt=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_return",row.id}));
  const auto end=take(read_actual_common_fact_position(common,row.completion_source_ref));
  if(begin>=receipt||receipt>=end)fail("MilitaryReturnDetachmentCompletionOrderMismatch",row.id);
 }
});}
namespace {
template<class Emit>Status visit_detachment_pins(const CommonStore& common,const MilitaryActionTimeState& state,Emit emit){return status([&]{
 take(validate_military_return_detachment_sources(common,state,1));
 for(const auto& item:state.dynamic_return_facts.rows()){
  const auto& row=item.second;const auto& ref=row.completion_source_ref;
  take(emit(SourcePinBuildView{"military","return_detachment_result",row.id,"actual_receipt","military","military_return",row.id,SourcePinReason::unique_outcome}));
  take(emit(SourcePinBuildView{"military","return_detachment_result",row.id,"actual_completion",ref.domain,ref.kind,ref.id,SourcePinReason::dependent_state}));
 }
});}
}
Status collect_military_return_detachment_pins(const CommonStore& common,const MilitaryActionTimeState& state,SourcePinSink& sink){return visit_detachment_pins(common,state,[&](const SourcePinBuildView& pin){return sink.contains(pin)?common_success():sink.append(pin);});}
Status validate_military_return_detachment_pin(const CommonStore& common,const MilitaryActionTimeState& state,const SourcePin& input){return status([&]{
 bool found=false;take(visit_detachment_pins(common,state,[&](const SourcePinBuildView& pin){if(pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id&&pin.reason==input.reason)found=true;return common_success();}));if(!found)fail("MilitaryReturnDetachmentPinRequired",input.consumer.id);
});}
std::vector<RequiredSourceConsumerSlot> military_return_detachment_consumer_slots(){return {{"military","return_detachment_result","actual_receipt"},{"military","return_detachment_result","actual_completion"}};}
Outcome<bool> military_return_detachment_source_may_release(const CommonStore& common,const MilitaryActionTimeState& state,const SourceRef& source,Integer schema){return guard<bool>([&]{
 if(schema!=2)fail("MilitaryReturnDetachmentSourceSchemaRequired",source.id);bool found=false;
 take(visit_detachment_pins(common,state,[&](const SourcePinBuildView& pin){if(pin.source_domain==source.domain&&pin.source_kind==source.kind&&pin.source_id==source.id)found=true;return common_success();}));if(!found)fail("MilitaryReturnDetachmentSourceRequired",source.id);return false;
});}
}
