#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_action_resource_codec.hpp"
#include "military_action_resource_fields.hpp"
#include "military_state_codec_archive.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
struct Staging {OrderedTable<MilitaryActionResourceEffect1> formation_results,return_receipts;};
struct Keys {
 template<class T>static constexpr bool owns=std::is_same_v<T,MilitaryActionResourceEffect1>;
 bool operator()(const StableId& key,const MilitaryActionResourceEffect1&)const{return !key.empty();}
};
bool successful(const DynamicMilitaryActionFact2& row){return row.command=="army_form"&&row.rule_version==2&&row.phase=="completed"&&row.success&&row.completed_at&&row.result&&row.result->ok.value_or(false)&&row.terminal_source_ref;}
const DynamicMilitaryLogisticsFact2& return_owner(const MilitaryActionTimeState& state,const DynamicMilitaryReturnFact2& row){
 const auto* owner=state.dynamic_logistics_facts.find(row.logistics_action_ref);
 if(!owner||owner->command!="army_return"||owner->rule_version!=2||owner->phase!="completed"||!owner->success||!owner->completed_at||!owner->result||!owner->result->ok.value_or(false)||owner->result->receipt_id!=row.id||!owner->terminal_source_ref||!same_source(*owner->terminal_source_ref,row.completion_source_ref))fail("MilitaryReturnResourceOriginalResultRequired",row.id);
 return *owner;
}
template<class A>Status fields(A& archive,Staging& staging,Integer selected,const military_action_resource_fields::Layout& layout){
 if(selected!=1)return archive.reject("MilitaryActionResourceSelectedSchemaRequired");Integer schema=selected;archive.field("schema",schema);if(!archive.status().ok)return archive.status();if(schema!=selected)return archive.reject("MilitaryActionResourceSelectedSchemaMismatch");
 military_state_codec::RootArchive<A,Keys> root{archive};
 root.table("formation_results",staging.formation_results,[&](auto& child,auto& row){return military_action_resource_fields::value(child,row,layout);});
 root.table("return_receipts",staging.return_receipts,[&](auto& child,auto& row){return military_action_resource_fields::value(child,row,layout);});return root.status();
}
void collect(const MilitaryActionTimeState& state,Staging& staging){
 for(const auto& item:state.dynamic_action_facts.rows()){
  const auto& row=item.second;if(successful(row)){if(!row.actual_resource_effect||row.actual_resource_effect->return_opening)fail("MilitaryFormationResourceEffectRequired",row.id);if(!staging.formation_results.insert(item.first,*row.actual_resource_effect))fail("MilitaryResourceEffectDuplicate",row.id);}
  else if(row.actual_resource_effect)fail("UnexpectedMilitaryResourceEffect",row.id);
 }
 for(const auto& item:state.dynamic_return_facts.rows()){
  const auto& row=item.second;return_owner(state,row);if(!row.actual_resource_effect||!row.actual_resource_effect->return_opening)fail("MilitaryReturnResourceEffectRequired",row.id);if(!staging.return_receipts.insert(item.first,*row.actual_resource_effect))fail("MilitaryResourceEffectDuplicate",row.id);
 }
}
void empty_targets(const MilitaryActionTimeState& state){
 for(const auto& row:state.dynamic_action_facts.rows())if(row.second.actual_resource_effect)fail("MilitaryResourceAdjunctAlreadyAttached",row.first);
 for(const auto& row:state.dynamic_return_facts.rows())if(row.second.actual_resource_effect)fail("MilitaryResourceAdjunctAlreadyAttached",row.first);
}
template<class T>CheckpointMemoryCharge reserve_row(const T& v){return CheckpointMemoryCharge(0);}
template<class T>struct PreparedRow {CheckpointMemoryCharge copied_fields;T row;explicit PreparedRow(const T& v):copied_fields(reserve_row(v)),row(v){}};
void attach(MilitaryActionTimeState& state,const Staging& staging){
 std::size_t count=0;for(const auto& item:state.dynamic_action_facts.rows())if(successful(item.second))++count;
 if(count!=staging.formation_results.size()||state.dynamic_return_facts.size()!=staging.return_receipts.size())fail("MilitaryResourceAdjunctCoverageMismatch");
 for(const auto& item:staging.formation_results.rows()){const auto* row=state.dynamic_action_facts.find(item.first);if(!row||!successful(*row)||item.second.return_opening)fail("MilitaryFormationResourceOrphan",item.first);}
 for(const auto& item:staging.return_receipts.rows()){const auto* row=state.dynamic_return_facts.find(item.first);if(!row||!item.second.return_opening)fail("MilitaryReturnResourceOrphan",item.first);return_owner(state,*row);}
 // Each fresh transient whole row is charged before copying. The sole original
 // table separately owns its persistent replacement; reborrow after every set.
 for(const auto& item:staging.formation_results.rows()){PreparedRow<DynamicMilitaryActionFact2> candidate(*state.dynamic_action_facts.find(item.first));candidate.row.actual_resource_effect.emplace(item.second);state.dynamic_action_facts.set(item.first,candidate.row);}
 for(const auto& item:staging.return_receipts.rows()){PreparedRow<DynamicMilitaryReturnFact2> candidate(*state.dynamic_return_facts.find(item.first));candidate.row.actual_resource_effect.emplace(item.second);state.dynamic_return_facts.set(item.first,candidate.row);}
}
}
// 遍历军旅资源附录；保留原格式能力判定，拒绝不支持的版本与留存遍历。
Status visit_military_action_resources(CodecVisitor &c,
                                       MilitaryActionTimeState &state,
                                       Integer selected) {
  return status([&] {
    try {
      if (selected != 1)
        fail("MilitaryActionResourceSelectedSchemaRequired");

      const CommonResourceCodecContext resources{
          c.item_instance_refs_enabled, c.lot_property_refs_enabled,
          c.domain_activity_reservation_refs_enabled,
          c.carrier_location_enabled};
      const military_action_resource_fields::Layout layout{
          bool(c.schema_features &
               common_checkpoint::typed_resource_references),
          resources};
      if (c.reading())
        empty_targets(state);
      Staging staging;
      if (!c.reading())
        collect(state, staging);
      take(fields(c, staging, selected, layout));
      if (c.reading())
        attach(state, staging);
    } catch (const exact_fields::CodecFailure &e) {
      throw Failure{e.error};
    }
  });
}
Status decode_military_action_resources(const BoundaryValue& raw,MilitaryActionTimeState& state,Integer selected,bool typed,const CommonResourceCodecContext& resources){return status([&]{try{
 if(selected!=1)fail("MilitaryActionResourceSelectedSchemaRequired");if(!ScopedCheckpointJsonDecode::current())fail("CheckpointJsonContextRequired");empty_targets(state);Staging staging;ActorFoundationJsonVisitor c(raw);take(c.ordered_fields());take(fields(c,staging,selected,{typed,resources}));take(c.finish());attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_action_resources_fields(ProjectionWriter& sink,const MilitaryActionTimeState& state,Integer selected,bool typed,const CommonResourceCodecContext& resources){return status([&]{try{
 if(selected!=1)fail("MilitaryActionResourceSelectedSchemaRequired");Staging staging;collect(state,staging);ActorFoundationProjectionArchive c(sink);take(fields(c,staging,selected,{typed,resources}));
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_action_resources(ProjectionWriter& sink,std::string_view name,const MilitaryActionTimeState& state,Integer selected,bool typed,const CommonResourceCodecContext& resources){return sink.object(name,[&](auto& fields){return emit_military_action_resources_fields(fields,state,selected,typed,resources);});}
Outcome<BoundaryValue> encode_military_action_resources(const MilitaryActionTimeState& state,Integer selected,bool typed,const CommonResourceCodecContext& resources){return guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);take(emit_military_action_resources(sink,"",state,selected,typed,resources));take(sink.finish());return out;});}
Status validate_military_action_resource_sources(const CommonStore& common,const MilitaryActionTimeState& state,Integer selected){return status([&]{
 if(selected==0){empty_targets(state);return;}if(selected!=1)fail("MilitaryActionResourceSelectedSchemaRequired");
 for(const auto& item:state.dynamic_action_facts.rows()){
  const auto& row=item.second;if(!successful(row)){if(row.actual_resource_effect)fail("UnexpectedMilitaryResourceEffect",row.id);continue;}
  if(!row.actual_resource_effect||row.actual_resource_effect->return_opening||row.terminal_source_ref->domain!="military"||row.terminal_source_ref->kind!="military_action_completion_applied")fail("MilitaryFormationResourceEffectRequired",row.id);
  const auto begin=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_action",row.id}));const auto end=take(read_actual_common_fact_position(common,*row.terminal_source_ref));if(begin>=end)fail("MilitaryResourceCompletionOrderMismatch",row.id);take(validate_military_action_resource_effect(*row.actual_resource_effect));
 }
 for(const auto& item:state.dynamic_return_facts.rows()){
  const auto& row=item.second;const auto& action=return_owner(state,row);if(!row.actual_resource_effect||!row.actual_resource_effect->return_opening||row.completion_source_ref.domain!="military"||row.completion_source_ref.kind!="military_logistics_completion_applied")fail("MilitaryReturnResourceEffectRequired",row.id);
  const auto begin=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_action",action.id}));const auto receipt=take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_return",row.id}));const auto end=take(read_actual_common_fact_position(common,row.completion_source_ref));if(begin>=receipt||receipt>=end)fail("MilitaryResourceCompletionOrderMismatch",row.id);take(validate_military_action_resource_effect(*row.actual_resource_effect));
 }
});}
namespace {
template<class Emit>Status visit_resource_pins(const CommonStore& common,const MilitaryActionTimeState& state,Emit emit){return status([&]{
 take(validate_military_action_resource_sources(common,state,1));
 for(const auto& item:state.dynamic_action_facts.rows())if(item.second.actual_resource_effect){const auto& row=item.second;const auto& ref=*row.terminal_source_ref;
  take(emit(SourcePinBuildView{"military","formation_resource_result",row.id,"actual_completion",ref.domain,ref.kind,ref.id,SourcePinReason::unique_outcome}));
 }
 for(const auto& item:state.dynamic_return_facts.rows()){const auto& row=item.second;const auto& ref=row.completion_source_ref;
  take(emit(SourcePinBuildView{"military","return_resource_result",row.id,"actual_receipt","military","military_return",row.id,SourcePinReason::unique_outcome}));
  take(emit(SourcePinBuildView{"military","return_resource_result",row.id,"actual_completion",ref.domain,ref.kind,ref.id,SourcePinReason::dependent_state}));
 }
});}
}
Status collect_military_action_resource_pins(const CommonStore& common,const MilitaryActionTimeState& state,SourcePinSink& sink){return visit_resource_pins(common,state,[&](const SourcePinBuildView& pin){return sink.contains(pin)?common_success():sink.append(pin);});}
Status validate_military_action_resource_pin(const CommonStore& common,const MilitaryActionTimeState& state,const SourcePin& input){return status([&]{
 bool found=false;take(visit_resource_pins(common,state,[&](const SourcePinBuildView& pin){if(pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id&&pin.reason==input.reason)found=true;return common_success();}));if(!found)fail("MilitaryActionResourcePinRequired",input.consumer.id);
});}
std::vector<RequiredSourceConsumerSlot> military_action_resource_consumer_slots(){return {{"military","formation_resource_result","actual_completion"},{"military","return_resource_result","actual_receipt"},{"military","return_resource_result","actual_completion"}};}
Outcome<bool> military_action_resource_source_may_release(const CommonStore& common,const MilitaryActionTimeState& state,const SourceRef& source,Integer schema){return guard<bool>([&]{
 if(schema!=2)fail("MilitaryActionResourceSourceSchemaRequired",source.id);bool found=false;
 take(visit_resource_pins(common,state,[&](const SourcePinBuildView& pin){if(pin.source_domain==source.domain&&pin.source_kind==source.kind&&pin.source_id==source.id)found=true;return common_success();}));if(!found)fail("MilitaryActionResourceSourceRequired",source.id);return false;
});}
}
