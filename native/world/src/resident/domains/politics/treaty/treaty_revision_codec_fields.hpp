#pragma once
#include "treaty_revision_sources.hpp"
#include "treaty_terms_codec_fields.hpp"
namespace hanmo::resident::treaty_revision_codec_fields {
template<class C>Status acting(C& c,ActingAuthority& r){c.field("organization_id",r.organization_id);c.field("office_id",r.office_id);c.field("authority_epoch",r.authority_epoch);return c.field("actor_id",r.actor_id);}
template<class C>Status offer(C& c,TreatyOfferSource& r){
 c.field("id",r.id);c.field("terms_definition_id",r.terms_definition_id);c.field("offering_party",r.offering_party);
 c.object("issuer",[&](auto& v){return acting(v,r.issuer);});c.object("issued_at",[&](auto& v){return exact_fields::moment(v,r.issued_at);});
 if(c.status().ok){auto valid=validate_treaty_offer_source_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
template<class C>Status counteroffer(C& c,TreatyCounterofferSource& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);
 c.object("previous_terms_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.previous_terms_source);});
 c.field("terms_definition_id",r.terms_definition_id);c.field("offering_party",r.offering_party);
 c.object("issuer",[&](auto& v){return acting(v,r.issuer);});c.object("issued_at",[&](auto& v){return exact_fields::moment(v,r.issued_at);});
 if(c.status().ok){auto valid=validate_treaty_counteroffer_source_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}
