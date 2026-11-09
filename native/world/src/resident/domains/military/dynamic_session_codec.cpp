#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "dynamic_session_codec.hpp"
#include "dynamic_session_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_dynamic_military_session_current(CodecVisitor& c,DynamicMilitarySessionCurrent2& v){try{return dynamic_session_codec::value(c,v);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status visit_dynamic_military_session_source(CodecVisitor& c,DynamicSessionStateSource2& v){try{return dynamic_session_codec::value(c,v);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status visit_dynamic_military_random_slot(CodecVisitor& c,DynamicMilitaryRandomSlot& v){try{return dynamic_session_codec::value(c,v);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status visit_dynamic_military_session_payloads(CodecVisitor& c,MilitaryExactState& v){try{common_checkpoint::table(c,"session_current_states",v.session_current_states,[](auto& x,auto& row){return dynamic_session_codec::value(x,row);});return common_checkpoint::table(c,"session_state_sources",v.session_state_sources,[](auto& x,auto& row){return dynamic_session_codec::value(x,row);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
namespace {
template<class T>Outcome<T> decode_row(const BoundaryValue& input){return movement_detail::guard<T>([&]{try{ActorFoundationJsonVisitor c(input);movement_detail::take(prepare_checkpoint_default_values<T>(c));T value;movement_detail::take(dynamic_session_codec::value(c,value));movement_detail::take(c.finish());return value;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Status emit_row(ProjectionWriter& sink,std::string_view name,const T& value){try{return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,[](auto& c,auto& v){return dynamic_session_codec::value(c,v);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
template<class T>Outcome<BoundaryValue> encode_row(const T& value){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue result;BoundaryProjectionWriter sink(result);movement_detail::take(emit_row(sink,"",value));movement_detail::take(sink.finish());return result;});}
}
Outcome<DynamicMilitarySessionCurrent2> decode_dynamic_military_session_current(const BoundaryValue& input){return decode_row<DynamicMilitarySessionCurrent2>(input);}
Outcome<BoundaryValue> encode_dynamic_military_session_current(const DynamicMilitarySessionCurrent2& value){return encode_row(value);}
Status emit_dynamic_military_session_current(ProjectionWriter& sink,std::string_view name,const DynamicMilitarySessionCurrent2& value){return emit_row(sink,name,value);}
Outcome<DynamicSessionStateSource2> decode_dynamic_military_session_source(const BoundaryValue& input){return decode_row<DynamicSessionStateSource2>(input);}
Outcome<BoundaryValue> encode_dynamic_military_session_source(const DynamicSessionStateSource2& value){return encode_row(value);}
Status emit_dynamic_military_session_source(ProjectionWriter& sink,std::string_view name,const DynamicSessionStateSource2& value){return emit_row(sink,name,value);}
Outcome<DynamicMilitaryRandomSlot> decode_dynamic_military_random_slot(const BoundaryValue& input){return decode_row<DynamicMilitaryRandomSlot>(input);}
Outcome<BoundaryValue> encode_dynamic_military_random_slot(const DynamicMilitaryRandomSlot& value){return encode_row(value);}
Status emit_dynamic_military_random_slot(ProjectionWriter& sink,std::string_view name,const DynamicMilitaryRandomSlot& value){return emit_row(sink,name,value);}
}
