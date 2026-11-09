#pragma once
#include "treaty_recognition_sources.hpp"
#include "treaty_revision_codec_fields.hpp"
namespace hanmo::resident::treaty_recognition_codec_fields {
template<class C>Status receipt(C& c,MomentOwnerSource& r){c.object("source_ref",[&](auto& v){return treaty_terms_codec_fields::source(v,r.source_ref);});return c.field("source_schema",r.source_schema);}
template<class C>Status presentation(C& c,TreatyPresentationSource& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);c.object("terms_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.terms_source);});c.object("recipient",[&](auto& v){return treaty_revision_codec_fields::acting(v,r.recipient);});c.object("actual_content_receipt",[&](auto& v){return receipt(v,r.actual_content_receipt);});c.object("presented_at",[&](auto& v){return exact_fields::moment(v,r.presented_at);});if(c.status().ok){auto valid=validate_treaty_presentation_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
template<class C>Status reply(C& c,TreatyReplySource& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);c.object("terms_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.terms_source);});c.object("presentation_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.presentation_source);});c.object("respondent",[&](auto& v){return treaty_revision_codec_fields::acting(v,r.respondent);});static constexpr const char* decisions[]={"accept","refuse"};treaty_terms_codec_fields::enum_field(c,"decision",r.decision,decisions);c.object("replied_at",[&](auto& v){return exact_fields::moment(v,r.replied_at);});if(c.status().ok){auto valid=validate_treaty_reply_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}
