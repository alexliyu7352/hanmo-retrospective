#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_duty_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {
Status visit_military_duty_payloads(CodecVisitor& c,MilitaryDutyState& state){try{return military_duty_codec::state(c,state);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status visit_military_duty_rule(CodecVisitor& c,MilitaryDutyRule& rule){try{return military_duty_codec::value(c,rule);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Status visit_military_authority_ref(CodecVisitor& c,MilitaryAuthorityRef& value){return military_duty_codec::value(c,value);}
Outcome<MilitaryDutyState> decode_military_duty_payloads(const BoundaryValue& input){return movement_detail::guard<MilitaryDutyState>([&]{try{ActorFoundationJsonVisitor c(input);movement_detail::take(prepare_checkpoint_default_values<MilitaryDutyState>(c));MilitaryDutyState value;movement_detail::take(military_duty_codec::state(c,value));movement_detail::take(c.finish());return value;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
Status emit_military_duty_payloads(ProjectionWriter& sink,std::string_view name,const MilitaryDutyState& input){try{return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,input,[](auto& c,auto& value){return military_duty_codec::state(c,value);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
Outcome<BoundaryValue> encode_military_duty_payloads(const MilitaryDutyState& input){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);movement_detail::take(emit_military_duty_payloads(sink,"",input));movement_detail::take(sink.finish());return out;});}
Outcome<MilitaryDutyRule> decode_military_duty_rule(const BoundaryValue& input){return movement_detail::guard<MilitaryDutyRule>([&]{ActorFoundationJsonVisitor c(input);movement_detail::take(prepare_checkpoint_default_values<MilitaryDutyRule>(c));MilitaryDutyRule value;movement_detail::take(military_duty_codec::value(c,value));movement_detail::take(c.finish());MilitaryDutyCatalog catalog;catalog.rules.set(value.id,value);movement_detail::take(validate_military_duty_catalog(catalog));return value;});}
Status emit_military_duty_rule(ProjectionWriter& sink,std::string_view name,const MilitaryDutyRule& value){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,value,[](auto& c,auto& item){return military_duty_codec::value(c,item);});}
}
