#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "dynamic_effect_codec.hpp"
#include "dynamic_effect_codec_fields.hpp"
#include "military_exact_sources.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {
Status visit_dynamic_military_effect_group(CodecVisitor& c,DynamicBattleEffectGroup2& v){try{return dynamic_effect_codec::value(c,v);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status visit_dynamic_military_effect_groups(CodecVisitor& c,MilitaryExactState& state){try{return common_checkpoint::table(c,"effect_groups",state.effect_groups,[](auto& x,auto& row){return dynamic_effect_codec::value(x,row);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
namespace {
template<class T>Outcome<T> decode_row(const BoundaryValue& input){return movement_detail::guard<T>([&]{try{ActorFoundationJsonVisitor c(input);movement_detail::take(prepare_checkpoint_default_values<T>(c));T value;movement_detail::take(dynamic_effect_codec::value(c,value));movement_detail::take(c.finish());return value;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Status emit_row(ProjectionWriter& sink,std::string_view name,const T& value){try{return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,[](auto& c,auto& v){return dynamic_effect_codec::value(c,v);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
template<class T>Outcome<BoundaryValue> encode_row(const T& value){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue result;BoundaryProjectionWriter sink(result);movement_detail::take(emit_row(sink,"",value));movement_detail::take(sink.finish());return result;});}
}
Outcome<DynamicBattleEffectGroup2> decode_dynamic_military_effect_group(const BoundaryValue& input){return decode_row<DynamicBattleEffectGroup2>(input);}
Outcome<BoundaryValue> encode_dynamic_military_effect_group(const DynamicBattleEffectGroup2& value){return encode_row(value);}
Status emit_dynamic_military_effect_group(ProjectionWriter& sink,std::string_view name,const DynamicBattleEffectGroup2& value){return emit_row(sink,name,value);}
}
