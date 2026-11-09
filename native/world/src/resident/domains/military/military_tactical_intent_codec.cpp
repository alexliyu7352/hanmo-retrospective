#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_tactical_intent_codec.hpp"
#include "military_tactical_intent_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_military_accepted_intent3(CodecVisitor& c,MilitaryAcceptedIntent3& v){
 return movement_detail::status([&]{try{movement_detail::take(military_tactical_intent_fields::value(c,v));}
 catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Outcome<MilitaryAcceptedIntent3> decode_military_accepted_intent3(const BoundaryValue& input){
 return movement_detail::guard<MilitaryAcceptedIntent3>([&]{try{
  ActorFoundationJsonVisitor c(input);movement_detail::take(prepare_checkpoint_default_values<MilitaryAcceptedIntent3>(c));MilitaryAcceptedIntent3 value;
  movement_detail::take(military_tactical_intent_fields::value(c,value));movement_detail::take(c.finish());return value;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_military_accepted_intent3(ProjectionWriter& sink,std::string_view name,const MilitaryAcceptedIntent3& value){
 return movement_detail::status([&]{try{movement_detail::take(emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,
  [](auto& c,auto& v){return military_tactical_intent_fields::value(c,v);}));}
 catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_military_accepted_intent3_fields(ProjectionWriter& sink,const MilitaryAcceptedIntent3& value){
 return movement_detail::status([&]{try{
  ActorFoundationProjectionArchive c(sink);
  movement_detail::take(military_tactical_intent_fields::value(c,const_cast<MilitaryAcceptedIntent3&>(value)));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Outcome<BoundaryValue> encode_military_accepted_intent3(const MilitaryAcceptedIntent3& value){
 return movement_detail::guard<BoundaryValue>([&]{BoundaryValue result;BoundaryProjectionWriter sink(result);
  movement_detail::take(emit_military_accepted_intent3(sink,"",value));movement_detail::take(sink.finish());return result;});
}
} // namespace hanmo::resident
