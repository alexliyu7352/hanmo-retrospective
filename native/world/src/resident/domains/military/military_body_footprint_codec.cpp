#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_body_footprint_codec.hpp"
#include "military_body_footprint_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_military_body_footprint_definition2(CodecVisitor& c,MilitaryBodyFootprintDefinition2& row){
 return movement_detail::status([&]{try{
  movement_detail::take(military_body_footprint_fields::value(c,row));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Outcome<MilitaryBodyFootprintDefinition2> decode_military_body_footprint_definition2(const BoundaryValue& raw){
 return movement_detail::guard<MilitaryBodyFootprintDefinition2>([&]{try{
  MilitaryBodyFootprintDefinition2 row;ActorFoundationJsonVisitor c(raw);
  movement_detail::take(military_body_footprint_fields::value(c,row));movement_detail::take(c.finish());
  movement_detail::take(validate_military_convex_footprint(row.offsets));return row;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_military_body_footprint_definition2_fields(ProjectionWriter& sink,
 const MilitaryBodyFootprintDefinition2& row){return movement_detail::status([&]{try{
  ActorFoundationProjectionArchive c(sink);
  movement_detail::take(military_body_footprint_fields::value(c,const_cast<MilitaryBodyFootprintDefinition2&>(row)));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
Status emit_military_body_footprint_definition2(ProjectionWriter& sink,std::string_view name,
 const MilitaryBodyFootprintDefinition2& row){return sink.object(name,[&](ProjectionWriter& body){
  return emit_military_body_footprint_definition2_fields(body,row);
 });}
Outcome<BoundaryValue> encode_military_body_footprint_definition2(const MilitaryBodyFootprintDefinition2& row){
 return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);
  movement_detail::take(emit_military_body_footprint_definition2(sink,"",row));
  movement_detail::take(sink.finish());return out;
 });}
}
