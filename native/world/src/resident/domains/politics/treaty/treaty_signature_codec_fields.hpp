#pragma once
#include "treaty_signature_sources.hpp"
#include "treaty_revision_codec_fields.hpp"
namespace hanmo::resident::treaty_signature_codec_fields {
// 尚未启用的M-TREATY/1采用唯一tagged signer；旧外交签署行不走此字段表。
template<class C>Status actual_source(C& c,MomentOwnerSource& r){c.object("source_ref",[&](auto& v){return treaty_terms_codec_fields::source(v,r.source_ref);});return c.field("source_schema",r.source_schema);}
template<class C>Status signer(C& c,TreatySigner& value){
 static constexpr const char* names[]={"direct_office","mandated"};std::size_t index=0;
 auto valid=treaty_terms_codec_fields::variant_tag(c,value,names,index);if(!valid.ok)return valid;
 if(index==0){if(c.reading())value=TreatyDirectOfficeSigner{};return c.object("authority",[&](auto& v){return treaty_revision_codec_fields::acting(v,std::get<TreatyDirectOfficeSigner>(value).authority);});}
 if(c.reading())value=TreatyMandatedSigner{};auto& r=std::get<TreatyMandatedSigner>(value);
 c.field("actor_id",r.actor_id);c.object("actual_mandate_ref",[&](auto& v){return actual_source(v,r.actual_mandate_ref);});
 return c.object("actual_task_acceptance_ref",[&](auto& v){return actual_source(v,r.actual_task_acceptance_ref);});
}
template<class C>Status fields(C& c,TreatySignatureSource& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);
 c.object("terms_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.terms_source);});
 c.field("party_id",r.party_id);c.object("signer",[&](auto& v){return signer(v,r.signer);});
 c.object("actual_content_receipt",[&](auto& v){v.object("source_ref",[&](auto& a){return treaty_terms_codec_fields::source(a,r.actual_content_receipt.source_ref);});return v.field("source_schema",r.actual_content_receipt.source_schema);});
 c.object("signed_at",[&](auto& v){return exact_fields::moment(v,r.signed_at);});
 if(c.status().ok){auto valid=validate_treaty_signature_source_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}
