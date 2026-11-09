#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "transport_section_basis_fields.hpp"
#include "transport_section_group_fields.hpp"
#include "transport_section_manifest_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
using namespace movement_detail;
namespace {
using Section=TransportPhysicalSection;
void enter(TransportPhysicalCodecContext::State& state,Section section,bool reading){
 if(state.finished||state.reading!=reading)fail("TransportPhysicalCodecModeMismatch");
 if(state.write_section){
  if(reading||*state.write_section!=section)fail("TransportPhysicalSingleWriteSectionMismatch");
  if(state.travel_seen||state.group_seen||state.manifest_seen)fail("TransportPhysicalSingleWriteAlreadyVisited");
  return;
 }
 if(section==Section::travel){if(state.travel_seen||state.group_seen||state.manifest_seen)fail("TravelPhysicalSectionOrderMismatch");}
 else if(section==Section::group_transport){if(!state.travel_seen||state.group_seen||state.manifest_seen||!state.selected.group_transport_version)fail("GroupPhysicalSectionOrderMismatch");}
 else if(!state.travel_seen||state.group_seen!=(state.selected.group_transport_version!=0)||state.manifest_seen||state.selected.transport_manifest_bindings_version!=2)fail("ManifestPhysicalSectionOrderMismatch");
}
void completed(TransportPhysicalCodecContext::State& state,Section section){if(section==Section::travel)state.travel_seen=true;else if(section==Section::group_transport)state.group_seen=true;else state.manifest_seen=true;}
template<class C>Status fields(C& codec,TransportPhysicalCodecContext::State& state,Section section){
 if(section==Section::travel)return transport_section_fields::travel_exact(codec,state);
 if(section==Section::group_transport)return transport_section_fields::group_transport(codec,state);
 return transport_section_fields::manifest_bindings(codec,state);
}
Status visit(CodecVisitor& codec,TransportPhysicalCodecContext& context,Section section){return status([&]{
 auto& state=context.state();enter(state,section,codec.reading());
 if(section==Section::travel)take(visit_travel_checkpoint(codec,state.roots.travel));
 take(fields(codec,state,section));take(codec.status());completed(state,section);
});}
Status emit(ProjectionWriter& sink,std::string_view name,TransportPhysicalCodecContext& context,Section section){return status([&]{
 auto& state=context.state();enter(state,section,false);
 take(sink.object(name,[&](auto& writer){ActorFoundationProjectionArchive codec(writer);return fields(codec,state,section);}));
 take(sink.status());completed(state,section);
});}
Outcome<BoundaryValue> encode(TransportPhysicalCodecContext& context,Section section){return guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter writer(out);take(emit(writer,"",context,section));take(writer.finish());return out;});}
Status decode(const BoundaryValue& input,TransportPhysicalCodecContext& context,Section section){return status([&]{
 auto& state=context.state();enter(state,section,true);ActorFoundationJsonVisitor codec(input);take(fields(codec,state,section));take(codec.finish());completed(state,section);
});}
}
Status visit_travel_physical_checkpoint(CodecVisitor& c,TransportPhysicalCodecContext& x){return visit(c,x,Section::travel);}
Status visit_group_transport_physical_checkpoint(CodecVisitor& c,TransportPhysicalCodecContext& x){return visit(c,x,Section::group_transport);}
Status visit_transport_manifest_bindings_physical_checkpoint(CodecVisitor& c,TransportPhysicalCodecContext& x){return visit(c,x,Section::transport_manifest_bindings);}
Status emit_travel_physical_projection(ProjectionWriter& w,std::string_view n,TransportPhysicalCodecContext& x){return emit(w,n,x,Section::travel);}
Status emit_group_transport_physical_projection(ProjectionWriter& w,std::string_view n,TransportPhysicalCodecContext& x){return emit(w,n,x,Section::group_transport);}
Status emit_transport_manifest_bindings_physical_projection(ProjectionWriter& w,std::string_view n,TransportPhysicalCodecContext& x){return emit(w,n,x,Section::transport_manifest_bindings);}
Outcome<BoundaryValue> encode_travel_physical(TransportPhysicalCodecContext& x){return encode(x,Section::travel);}
Outcome<BoundaryValue> encode_group_transport_physical(TransportPhysicalCodecContext& x){return encode(x,Section::group_transport);}
Outcome<BoundaryValue> encode_transport_manifest_bindings_physical(TransportPhysicalCodecContext& x){return encode(x,Section::transport_manifest_bindings);}
Status decode_travel_physical(const BoundaryValue& original,const BoundaryValue& exact,TransportPhysicalCodecContext& x){return status([&]{
 auto& state=x.state();enter(state,Section::travel,true);
 // Original domain decoding is driven by Common's true action/fact kinds. The
 // new exact section never guesses a kind from the presence of params fields.
 state.roots.travel=take(decode_travel_seed(original));
 take(decode(exact,x,Section::travel));
});}
Status decode_group_transport_physical(const BoundaryValue& v,TransportPhysicalCodecContext& x){return decode(v,x,Section::group_transport);}
Status decode_transport_manifest_bindings_physical(const BoundaryValue& v,TransportPhysicalCodecContext& x){return decode(v,x,Section::transport_manifest_bindings);}
}
