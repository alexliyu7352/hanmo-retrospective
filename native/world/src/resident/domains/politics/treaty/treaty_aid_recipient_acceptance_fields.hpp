#pragma once
#include "treaty_aid_recipient_acceptance.hpp"
#include "treaty_aid_resource_selection_fields.hpp"
namespace hanmo::resident::treaty_aid_recipient_acceptance_fields {
template<class C>Status fields(C&c,TreatyAidRecipientAcceptance&r){
 Integer schema=1;c.field("schema_version",schema);if(!c.status().ok)return c.status();
 if(schema!=1)return c.reject("UnsupportedTreatyAidRecipientAcceptanceSchema");
 c.field("id",r.id);c.field("actor_id",r.actor_id);c.field("term_id",r.term_id);
 c.object("proposal",[&](auto&v){return treaty_signature_codec_fields::actual_source(v,r.proposal);});
 c.object("knowledge",[&](auto&v){
  static constexpr const char*names[]={"own_original_choice","actual_disclosure"};std::size_t index=0;
  auto valid=treaty_terms_codec_fields::variant_tag(v,r.knowledge,names,index);if(!valid.ok)return valid;
  if(index==0){if(v.reading())r.knowledge=TreatyResourceOwnChoice{};return v.status();}
  if(v.reading())r.knowledge=TreatyResourceDeliveredChoice{};
  return v.object("actual_receipt",[&](auto&w){return treaty_signature_codec_fields::actual_source(w,std::get<TreatyResourceDeliveredChoice>(r.knowledge).actual_receipt);});
 });
 c.optional("household_steward_source",r.household_steward_source,[](auto&v,auto&s){return treaty_terms_codec_fields::source(v,s);});
 c.object("accepted_at",[&](auto&v){return exact_fields::moment(v,r.accepted_at);});
 if(c.status().ok){auto valid=validate_treaty_aid_recipient_acceptance_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}
 return c.status();
}
}
