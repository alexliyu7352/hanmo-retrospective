#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_supply_exact_codec.hpp"
#include "military_supply_exact_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_exact_military_supply_source(CodecVisitor& c,MilitarySupplyEffectSource2& row){try{return military_supply_exact_fields::value(c,row);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status emit_exact_military_supply_source(ProjectionWriter& sink,std::string_view name,const MilitarySupplyEffectSource2& row){try{return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,row,[](auto& c,auto& value){return military_supply_exact_fields::value(c,value);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_exact_military_supply_source(const MilitarySupplyEffectSource2& row){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter writer(out);movement_detail::take(emit_exact_military_supply_source(writer,"",row));movement_detail::take(writer.finish());return out;});}
Outcome<MilitarySupplyEffectSource2> decode_exact_military_supply_source(const BoundaryValue& input){return movement_detail::guard<MilitarySupplyEffectSource2>([&]{try{ActorFoundationJsonVisitor c(input);movement_detail::take(prepare_checkpoint_default_values<MilitarySupplyEffectSource2>(c));MilitarySupplyEffectSource2 row;movement_detail::take(military_supply_exact_fields::value(c,row));movement_detail::take(c.finish());return row;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
}
