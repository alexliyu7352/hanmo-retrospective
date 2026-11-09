// One physical sequence for the original authority adjunct payloads.
#pragma once
#include "military_authority_time.hpp"
#include "military_action_time_record_fields.hpp"
#include "dynamic_action_codec_fields.hpp"
namespace hanmo::resident::military_authority_time_fields {
template<class C>Status value(C& c,MilitaryCampaignAcceptanceTime1& v){
 c.object("accepted_at",[&](auto& x){return exact_fields::moment(x,v.accepted_at);});
 c.object("actual_acceptance_source",[&](auto& x){return exact_fields::source(x,v.actual_acceptance_source);});
 c.object("original_context",[&](auto& x){return military_action_time_fields::value(x,v.original_context);});
 c.object("office_appointment_source",[&](auto& x){return dynamic_action_codec::value(x,v.office_appointment_source);});
 if(c.status().ok&&(v.actual_acceptance_source.domain!="military"||v.actual_acceptance_source.kind!="campaign_accepted"||v.office_appointment_source.source_schema!=2||v.office_appointment_source.source_ref.domain!="military"||v.office_appointment_source.source_ref.kind!="military_office_completion_applied"))return c.reject("MilitaryAuthoritySourceSchemaMismatch");
 return c.status();
}
template<class C>Status value(C& c,MilitaryOfficeResignationTime1& v){
 c.object("resigned_at",[&](auto& x){return exact_fields::moment(x,v.resigned_at);});
 c.object("actual_resignation_source",[&](auto& x){return exact_fields::source(x,v.actual_resignation_source);});
 c.object("original_terms",[&](auto& x){
  if constexpr(std::is_same_v<std::decay_t<decltype(x)>,CodecVisitor>)return military_original_action_codec::visit_office_terms(x,v.original_terms);
  else return x.foreign_record(v.original_terms,military_original_action_codec::decode_office_terms,military_original_action_codec::emit_office_terms_fields);
 });
 c.object("original_context",[&](auto& x){return military_action_time_fields::value(x,v.original_context);});
 c.object("office_appointment_source",[&](auto& x){return dynamic_action_codec::value(x,v.office_appointment_source);});
 if(c.status().ok&&(v.actual_resignation_source.domain!="military"||v.actual_resignation_source.kind!="office_resignation"||v.office_appointment_source.source_schema!=2||v.office_appointment_source.source_ref.domain!="military"||v.office_appointment_source.source_ref.kind!="military_office_completion_applied"))return c.reject("MilitaryAuthoritySourceSchemaMismatch");
 return c.status();
}
}
