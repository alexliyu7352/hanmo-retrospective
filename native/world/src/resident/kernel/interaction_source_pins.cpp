#include "interaction_source_pins.hpp"
#include "native_interaction_world.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include <array>
#include <charconv>
#include "resident/domains/interaction/interaction_response_sources.hpp"
#include "resident/domains/interaction/interaction_action_confirmation.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {namespace {
using Slot=InteractionSessionLifePinSlot;
struct Spec {Slot slot;const char*name;const char*domain;const char*kind;Integer schema;};
constexpr std::array<Spec,6> specs{{
 {Slot::death,"death","family_life","family_death",4},
 {Slot::opening,"opening","interaction","actor_social_occupancy_changed",1},
 {Slot::closing,"closing","interaction","actor_social_occupancy_changed",1},
 {Slot::preserved_result,"preserved_result","common","command_result",1},
 {Slot::original_action_terminal,"original_action_terminal","growth","growth_fixed_service_boundary",1},
 {Slot::selected_response_tail,"selected_response_tail","interaction","response_selected",1}
}};
const Spec* spec(Slot slot){for(const auto&value:specs)if(value.slot==slot)return &value;return nullptr;}
Status shape(const StableId&row,Slot slot,Integer ordinal,const SourceRef&ref,Integer schema){
 const auto*s=spec(slot);
 if(row.empty()||!s||ordinal<0||(slot!=Slot::preserved_result&&ordinal!=0)||
    ref.id.empty()||ref.domain!=s->domain||ref.kind!=s->kind||
    (slot==Slot::selected_response_tail?(schema!=1&&schema!=2):schema!=s->schema))
  return common_failure("InteractionLifePinIdentityMismatch",row,ErrorKind::internal);
 return common_success();
}
class Checked final:public InteractionSessionLifeSourcePinSink {
 InteractionSessionLifeSourcePinSink& out_;Integer response_schema_;
public:Checked(InteractionSessionLifeSourcePinSink&out,Integer schema):out_(out),response_schema_(schema){}
 Status original_source(const StableId&row,Slot slot,Integer ordinal,const SourceRef&ref,Integer schema)override{
  auto valid=shape(row,slot,ordinal,ref,schema);if(!valid.ok)return valid;
  if(slot==Slot::selected_response_tail&&schema!=response_schema_)return common_failure("InteractionLifePinSelectedSchemaMismatch",row,ErrorKind::internal);
  return out_.original_source(row,slot,ordinal,ref,schema);
 }
};
bool same(const SourcePinBuildView&a,const SourcePin&b){return a.consumer_domain==b.consumer.domain&&a.consumer_kind==b.consumer.kind&&a.consumer_id==b.consumer.id&&a.consumer_slot==b.consumer.slot&&a.source_domain==b.source.domain&&a.source_kind==b.source.kind&&a.source_id==b.source.id&&a.reason==b.reason;}
// Each original array element retains its established length-prefixed identity.
SourcePinConsumerId cell(const StableId&row,Integer ordinal){
 char size[32],number[32];const auto a=std::to_chars(size,size+sizeof size,row.size());const auto b=std::to_chars(number,number+sizeof number,ordinal);
 if(a.ec!=std::errc{}||b.ec!=std::errc{})throw std::length_error("InteractionLifeConsumerIdOverflow");
 return SourcePinConsumerId{{size,static_cast<std::size_t>(a.ptr-size)},":",row,":",{number,static_cast<std::size_t>(b.ptr-number)}};
}
class Pins final:public InteractionSessionLifeSourcePinSink {
 Slot slot_;SourcePinSink*output_;const SourcePin*requested_;
public:bool found=false;
 Pins(Slot slot,SourcePinSink*output,const SourcePin*requested):slot_(slot),output_(output),requested_(requested){}
 Status original_source(const StableId&row,Slot slot,Integer ordinal,const SourceRef&ref,Integer schema)override{
  auto valid=shape(row,slot,ordinal,ref,schema);if(!valid.ok)return valid;if(slot!=slot_)return common_success();
  const auto id=cell(row,ordinal);
  const SourcePinBuildView pin{"interaction","session_life",id.value(),spec(slot)->name,ref.domain,ref.kind,ref.id,SourcePinReason::unique_outcome};
  if(requested_&&same(pin,*requested_))found=true;return output_?output_->append(pin):common_success();
 }
};
using ConfirmationRole=InteractionActionConfirmationPinRole;
struct ConfirmationSpec {
 ConfirmationRole role;const char* slot;const char* domain;const char* kind;
 std::size_t maximum;
};
constexpr std::array<ConfirmationSpec,6> confirmation_specs{{
 {ConfirmationRole::outer_result,"outer_result","common","command_result",1},
 {ConfirmationRole::session_opening,"session_opening","interaction","actor_social_occupancy_changed",1},
 {ConfirmationRole::checked_session_close,"checked_session_close","interaction","actor_social_occupancy_changed",CodecVisitor::max_items},
 {ConfirmationRole::checked_response,"checked_response","interaction","response_selected",CodecVisitor::max_items},
 {ConfirmationRole::checked_response_commit,"checked_response_commit","common","command_result",CodecVisitor::max_items},
 {ConfirmationRole::earlier_response_consumption,"earlier_response_consumption","common","command_result",CodecVisitor::max_items}
}};
const ConfirmationSpec* confirmation_spec(ConfirmationRole role){
 for(const auto& item:confirmation_specs)if(item.role==role)return &item;
 return nullptr;
}
class ConfirmationPins final:public InteractionActionConfirmationPinSink {
 const StableId& result_id_;ConfirmationRole role_;Integer response_schema_;
 SourcePinSink* output_;const SourcePin* requested_;
public:
 bool found=false;
 ConfirmationPins(const StableId& result_id,ConfirmationRole role,Integer schema,
                  SourcePinSink* output,const SourcePin* requested):
  result_id_(result_id),role_(role),response_schema_(schema),output_(output),requested_(requested){}
 Status original_source(ConfirmationRole role,const SourceRefBuildView& source,Integer schema)override{
  const auto* item=confirmation_spec(role);
  const Integer expected=role==ConfirmationRole::checked_response?response_schema_:1;
  if(!item||result_id_.empty()||source.id.empty()||source.domain!=item->domain||
     source.kind!=item->kind||schema!=expected)
   return common_failure("InteractionConfirmationPinIdentityMismatch",result_id_,ErrorKind::internal);
  if(role!=role_)return common_success();
  const SourcePinBuildView pin{"interaction","action_confirmation",result_id_,item->slot,
   source.domain,source.kind,source.id,SourcePinReason::unique_outcome};
  if(requested_&&same(pin,*requested_))found=true;
  // Multiple original checks may use the same source. Preserve the sole
  // collector's established identity instead of allocating a local seen set.
  if(output_&&!output_->contains(pin))return output_->append(pin);
  return common_success();
 }
};
Status confirmation_pins(const WorldBundle& world,ConfirmationRole role,
                         SourcePinSink* output,const SourcePin* requested,bool& found){
 if(!world.catalog||!world.catalog->exact_time||!exact_complete_selected(*world.catalog->exact_time))
  return common_failure("InteractionConfirmationPinSelectionRequired",{},ErrorKind::unsupported_capability);
 auto layout=validate_native_interaction_response_layout(*world.catalog,world.interaction);
 if(!layout.ok)return layout;
 auto selected=selected_native_interaction_response_version(world.catalog->identity);
 if(!selected.ok)return {false,{},std::move(selected.error)};
 if(!interaction_action_confirmation_schema_version(selected.value))return common_success();
 if(!world.catalog->interaction||!world.interaction.response_sources)
  return common_failure("InteractionConfirmationOriginalOwnerRequired",{},ErrorKind::internal);
 for(const auto& row:world.interaction.response_sources->action_confirmation_sources.rows()){
  if(row.first!=row.second.committed_result_id)
   return common_failure("InteractionConfirmationSourceKeyMismatch",row.first);
  ConfirmationPins sink(row.first,role,interaction_response_source_schema_version(selected.value),output,requested);
  auto status=visit_interaction_action_confirmation_pins(world.common,world.interaction,
   *world.catalog->interaction,row.second,sink);
  if(!status.ok)return status;
  found=found||sink.found;
 }
 return common_success();
}
}
Status visit_complete_interaction_life_pins(const WorldBundle&w,InteractionSessionLifeSourcePinSink&sink){
 if(!w.catalog||!w.catalog->exact_time||!exact_complete_selected(*w.catalog->exact_time))
  return common_failure("InteractionLifePinSelectionRequired",{},ErrorKind::unsupported_capability);
 auto layout=validate_native_interaction_response_layout(*w.catalog,w.interaction);if(!layout.ok)return layout;
 auto selected=selected_native_interaction_response_version(w.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(!selected.value)return common_success();
 auto lease=w.complete_exact_time_bindings();if(!lease.ok)return {false,{},std::move(lease.error)};
 if(!lease.value)return common_failure("InteractionLifePinOwningLeaseRequired",{},ErrorKind::unsupported_capability);
 auto source=read_native_interaction_world(w,lease.value->reads());if(!source.ok)return {false,{},std::move(source.error)};
 Checked checked(sink,interaction_response_source_schema_version(selected.value));return source.value->visit_session_life_pins(checked);
}
Outcome<std::vector<SourceConsumerRegistration>> make_interaction_session_life_source_consumers(){try{
 std::vector<SourceConsumerRegistration>out;out.reserve(specs.size());
 for(const auto&s:specs){
  SourceConsumerRegistration r;r.domain="interaction";r.kind="session_life";r.slot=s.name;r.max_references_per_consumer=1;r.allowed_source_kinds={{s.domain,s.kind}};
  const auto slot=s.slot;
  r.enumerate_actual_pins=[slot](const WorldBundle&w,SourcePinSink&output)->Status{
   Pins sink(slot,&output,nullptr);return visit_complete_interaction_life_pins(w,sink);
  };
  r.validate_actual_pin=[slot](const WorldBundle&w,const SourcePin&pin){
   Pins sink(slot,nullptr,&pin);auto valid=visit_complete_interaction_life_pins(w,sink);
   return !valid.ok?valid:sink.found?common_success():common_failure("InteractionLifePinNotActual",pin.consumer.id);
  };
  out.push_back(std::move(r));
 }
 return {true,std::move(out),{}};
 }catch(const std::bad_alloc&){return {false,{},common_failure("InteractionLifePinResourceExhausted",{},ErrorKind::internal).error};}
 catch(const std::length_error&){return {false,{},common_failure("InteractionLifePinResourceExhausted",{},ErrorKind::internal).error};}
}
Outcome<std::vector<SourceConsumerRegistration>> make_interaction_action_confirmation_source_consumers(){try{
 std::vector<SourceConsumerRegistration> out;out.reserve(confirmation_specs.size());
 for(const auto& spec:confirmation_specs){
  SourceConsumerRegistration registration;
  registration.domain="interaction";registration.kind="action_confirmation";registration.slot=spec.slot;
  registration.max_references_per_consumer=spec.maximum;
  registration.allowed_source_kinds={{spec.domain,spec.kind}};
  const auto role=spec.role;
  registration.enumerate_actual_pins=[role](const WorldBundle& world,SourcePinSink& output){
   bool found=false;return confirmation_pins(world,role,&output,nullptr,found);
  };
  registration.validate_actual_pin=[role](const WorldBundle& world,const SourcePin& pin){
   bool found=false;auto status=confirmation_pins(world,role,nullptr,&pin,found);
   return !status.ok?status:found?common_success():common_failure("InteractionConfirmationPinNotActual",pin.consumer.id);
  };
  out.push_back(std::move(registration));
 }
 return {true,std::move(out),{}};
 }catch(const std::bad_alloc&){return {false,{},common_failure("InteractionConfirmationPinResourceExhausted",{},ErrorKind::internal).error};}
 catch(const std::length_error&){return {false,{},common_failure("InteractionConfirmationPinResourceExhausted",{},ErrorKind::internal).error};}
}
}
