#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "mobile_inventory_codec_fields.hpp"
#include "movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_mobile_inventory_adoption(CodecVisitor& c,MobileInventoryAdoptionView& value){try{return mobile_inventory_codec::adoption(c,value);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status visit_mobile_inventory_adoption(CodecVisitor& c,std::string_view name,MobileInventoryAdoptionView& value){return c.object(name,[&](auto& x){return visit_mobile_inventory_adoption(x,value);});}
Outcome<MobileInventoryAdoptionView> decode_mobile_inventory_adoption(const BoundaryValue& input){return movement_detail::guard<MobileInventoryAdoptionView>([&]{try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<MobileInventoryAdoptionView>();MobileInventoryAdoptionView value;ActorFoundationJsonVisitor c(input);movement_detail::take(mobile_inventory_codec::adoption(c,value));movement_detail::take(c.finish());return value;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
Outcome<MobileInventoryAdoptionView> decode_mobile_inventory_adoption(const BoundaryValue& input,ScopedCheckpointJsonDecode& budget){
 if(ScopedCheckpointJsonDecode::current()!=&budget)return {false,{},common_failure("CheckpointJsonContextMismatch").error};
 // Strings/sources transfer through their original moves; WorldMoment keeps
 // its own exact allocator. No ordinary credit substitutes for exact limbs.
 return decode_mobile_inventory_adoption(input);
}
Status emit_mobile_inventory_adoption(ProjectionWriter& sink,std::string_view name,const MobileInventoryAdoptionView& value){try{return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,[](auto& c,auto& v){return mobile_inventory_codec::adoption(c,v);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_mobile_inventory_adoption(const MobileInventoryAdoptionView& value){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue result;BoundaryProjectionWriter sink(result);movement_detail::take(emit_mobile_inventory_adoption(sink,"",value));movement_detail::take(sink.finish());return result;});}
Outcome<OrderedTable<MobileInventoryDefinition>> decode_mobile_inventory_definitions(const BoundaryValue& input){return movement_detail::guard<OrderedTable<MobileInventoryDefinition>>([&]{try{const auto* entries=input.as<BoundaryValue::Dict>();if(!entries)movement_detail::fail("InvalidMobileInventoryAuthorObject");OrderedTable<MobileInventoryDefinition> result;for(const auto& row:*entries){MobileInventoryDefinition value;ActorFoundationJsonVisitor c(row.second);movement_detail::take(mobile_inventory_codec::definition(c,value));movement_detail::take(c.finish());if(row.first!=value.id||value.id.empty()||value.inventory_id.empty()||value.initial_site_id.empty()||value.version<1||value.container_maximum_weight<1||!result.insert(row.first,std::move(value)))movement_detail::fail("InvalidMobileInventoryAuthorRow",row.first);}return result;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
Status emit_mobile_inventory_definition(ProjectionWriter& sink,std::string_view name,const MobileInventoryDefinition& value){try{return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,[](auto& c,auto& v){return mobile_inventory_codec::definition(c,v);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
}
