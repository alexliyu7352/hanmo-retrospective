#pragma once
#include "treaty_activation_sources.hpp"
#include "treaty_revision_codec_fields.hpp"
namespace hanmo::resident::treaty_activation_codec_fields {
template<class C>Status fields(C& c,TreatyActivationSource& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);
 c.object("terms_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.terms_source);});
 treaty_terms_codec_fields::sources(c,"signature_refs",r.signature_refs);
 c.object("activated_at",[&](auto& v){return exact_fields::moment(v,r.activated_at);});
 if(c.status().ok){auto valid=validate_treaty_activation_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}
