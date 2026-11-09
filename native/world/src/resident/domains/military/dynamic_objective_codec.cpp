#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "dynamic_objective_codec.hpp"
#include "dynamic_objective_codec_fields.hpp"
#include "military_exact_sources.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_dynamic_military_objective_evidence(CodecVisitor& c,DynamicMilitaryObjectiveEvidence2& v){
 try{return dynamic_objective_codec::value(c,v);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}
}
Status visit_dynamic_military_objective_evidence_table(CodecVisitor& c,MilitaryExactState& state){
 try{return common_checkpoint::table(c,"objective_evidence",state.objective_evidence,
  [](auto& x,auto& row){return dynamic_objective_codec::value(x,row);});}
 catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}
}
Outcome<DynamicMilitaryObjectiveEvidence2> decode_dynamic_military_objective_evidence(const BoundaryValue& input){
 return movement_detail::guard<DynamicMilitaryObjectiveEvidence2>([&]{try{
  ActorFoundationJsonVisitor c(input);movement_detail::take(prepare_checkpoint_default_values<DynamicMilitaryObjectiveEvidence2>(c));DynamicMilitaryObjectiveEvidence2 value;
  movement_detail::take(dynamic_objective_codec::value(c,value));movement_detail::take(c.finish());return value;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_dynamic_military_objective_evidence(ProjectionWriter& sink,std::string_view name,const DynamicMilitaryObjectiveEvidence2& value){
 try{return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,
  [](auto& c,auto& v){return dynamic_objective_codec::value(c,v);});}
 catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}
}
Outcome<BoundaryValue> encode_dynamic_military_objective_evidence(const DynamicMilitaryObjectiveEvidence2& value){
 return movement_detail::guard<BoundaryValue>([&]{BoundaryValue result;BoundaryProjectionWriter sink(result);
  movement_detail::take(emit_dynamic_military_objective_evidence(sink,"",value));movement_detail::take(sink.finish());return result;});
}
}
