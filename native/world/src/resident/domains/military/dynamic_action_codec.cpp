#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "dynamic_action_codec.hpp"
#include "dynamic_action_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_dynamic_military_action(CodecVisitor& c,DynamicMilitaryAction2& v){try{return dynamic_action_codec::value(c,v);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status visit_military_action_timing_definition2(CodecVisitor& c,MilitaryActionTimingDefinition2& v){try{return dynamic_action_codec::value(c,v);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status visit_dynamic_military_action_payloads(CodecVisitor& c,MilitaryExactState& state){try{return common_checkpoint::table(c,"actions",state.actions,[](auto& x,auto& v){return dynamic_action_codec::value(x,v);},[](auto& destination,auto& source)->Status{destination.emplace_back(std::move(source));return common_success();});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
namespace {
template<class T>Outcome<T> decode(const BoundaryValue& input){return movement_detail::guard<T>([&]{try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<T>();T value;ActorFoundationJsonVisitor c(input);movement_detail::take(dynamic_action_codec::value(c,value));movement_detail::take(c.finish());return value;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Status emit(ProjectionWriter& sink,std::string_view name,const T& value){try{return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,[](auto& c,auto& v){return dynamic_action_codec::value(c,v);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
}
Outcome<DynamicMilitaryAction2> decode_dynamic_military_action(const BoundaryValue& v){return decode<DynamicMilitaryAction2>(v);}
Outcome<MilitaryActionTimingDefinition2> decode_military_action_timing_definition2(const BoundaryValue& v){return decode<MilitaryActionTimingDefinition2>(v);}
Status emit_dynamic_military_action(ProjectionWriter& sink,std::string_view name,const DynamicMilitaryAction2& value){return emit(sink,name,value);}
Status emit_military_action_timing_definition2(ProjectionWriter& sink,std::string_view name,const MilitaryActionTimingDefinition2& value){return emit(sink,name,value);}
Outcome<BoundaryValue> encode_dynamic_military_action(const DynamicMilitaryAction2& v){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue result;BoundaryProjectionWriter sink(result);movement_detail::take(emit_dynamic_military_action(sink,"",v));movement_detail::take(sink.finish());return result;});}
Outcome<DynamicMilitaryActionCatalog2> decode_dynamic_military_action_catalog2(const BoundaryValue& timings,ScopedCheckpointJsonDecode& budget){
 if(ScopedCheckpointJsonDecode::current()!=&budget)budget.fail("CheckpointJsonContextMismatch","timings");
 return movement_detail::guard<DynamicMilitaryActionCatalog2>([&]{try{
  budget.default_values<DynamicMilitaryActionCatalog2>();DynamicMilitaryActionCatalog2 catalog;ActorFoundationJsonVisitor original(timings);
  movement_detail::take(original.root_table(catalog.timings,"timings",[](auto& c,auto& row){return dynamic_action_codec::value(c,row);},
   [](auto& destination,std::pair<StableId,MilitaryActionTimingDefinition2>& source)->Status{destination.emplace_back(std::move(source));return common_success();}));
  movement_detail::take(original.finish());
  for(const auto& row:catalog.timings.rows())if(row.first.empty()||row.first!=row.second.id)movement_detail::take(original.reject("MilitaryActionTimingDefinitionKeyMismatch","timings"));
  return catalog;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Outcome<DynamicMilitaryActionCatalog2> decode_dynamic_military_action_catalog2(const BoundaryValue& timings){
 auto* budget=ScopedCheckpointJsonDecode::current();
 if(!budget)return {false,{},common_failure("CheckpointJsonContextRequired","dynamic_military/timings",ErrorKind::internal).error};
 return decode_dynamic_military_action_catalog2(timings,*budget);
}

}
