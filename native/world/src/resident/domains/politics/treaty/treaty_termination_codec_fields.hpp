#pragma once
#include "treaty_termination_sources.hpp"
#include "treaty_revision_codec_fields.hpp"
namespace hanmo::resident::treaty_termination_codec_fields {
template<class C>Status cause(C& c,TreatyTerminationCause& r){
 static constexpr const char* tags[]={"by_expiry","mutual_release","material_breach_after_notice"};std::size_t index=0;
 auto valid=treaty_terms_codec_fields::variant_tag(c,r,tags,index);if(!valid.ok)return valid;
 if(index==0){if(c.reading())r=TreatyExpiryTermination{};return c.status();}
 if(index==1){if(c.reading())r=TreatyMutualReleaseTermination{};return treaty_terms_codec_fields::sources(c,"release_acceptance_refs",std::get<TreatyMutualReleaseTermination>(r).release_acceptance_refs);}
 if(c.reading())r=TreatyBreachAfterNoticeTermination{};auto& v=std::get<TreatyBreachAfterNoticeTermination>(r);
 c.object("incident_ref",[&](auto& a){return treaty_terms_codec_fields::source(a,v.incident_ref);});treaty_terms_codec_fields::sources(c,"notice_refs",v.notice_refs);return c.object("actual_decision_ref",[&](auto& a){return treaty_terms_codec_fields::source(a,v.actual_decision_ref);});
}
template<class C>Status fields(C& c,TreatyTerminationSource& r){c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);c.object("activation_ref",[&](auto& v){return treaty_terms_codec_fields::source(v,r.activation_ref);});c.object("cause",[&](auto& v){return cause(v,r.cause);});c.object("terminated_at",[&](auto& v){return exact_fields::moment(v,r.terminated_at);});if(c.status().ok){auto valid=validate_treaty_termination_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();}
}
