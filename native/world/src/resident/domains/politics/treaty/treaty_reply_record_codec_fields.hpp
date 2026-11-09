// 尚未准入M-TREATY/1的replies三tag唯一字段表；旧初次答复保持其原子字段顺序。
#pragma once
#include "treaty_release_sources.hpp"
#include "treaty_recognition_codec_fields.hpp"
namespace hanmo::resident::treaty_reply_record_codec_fields {
template<class C>Status request(C& c,TreatyReleaseRequestSource& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);
 c.object("activation_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.activation_source);});
 c.object("terms_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.terms_source);});
 c.object("requester",[&](auto& v){return treaty_revision_codec_fields::acting(v,r.requester);});
 c.object("requested_at",[&](auto& v){return exact_fields::moment(v,r.requested_at);});
 if(c.status().ok){auto s=validate_treaty_release_request_shape(r);if(!s.ok)return c.reject(s.error.original.code,s.error.original.path);}return c.status();
}
template<class C>Status answer(C& c,TreatyReleaseAnswerSource& r){
 c.field("id",r.id);c.field("treaty_id",r.treaty_id);c.field("terms_revision",r.terms_revision);
 c.object("activation_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.activation_source);});
 c.object("request_source",[&](auto& v){return treaty_terms_codec_fields::source(v,r.request_source);});
 c.object("respondent",[&](auto& v){return treaty_revision_codec_fields::acting(v,r.respondent);});
 static constexpr const char* decisions[]={"accept","refuse"};treaty_terms_codec_fields::enum_field(c,"decision",r.decision,decisions);
 c.object("actual_request_receipt",[&](auto& v){return treaty_recognition_codec_fields::receipt(v,r.actual_request_receipt);});
 c.object("answered_at",[&](auto& v){return exact_fields::moment(v,r.answered_at);});
 if(c.status().ok){auto s=validate_treaty_release_answer_shape(r);if(!s.ok)return c.reject(s.error.original.code,s.error.original.path);}return c.status();
}
template<class C>Status fields(C& c,TreatyReplyRecord& r){
 static constexpr const char* tags[]={"terms_reply","release_request","release_answer"};std::size_t index=0;
 auto valid=treaty_terms_codec_fields::variant_tag(c,r,tags,index);if(!valid.ok)return valid;
 if(index==0){if(c.reading())r=TreatyReplySource{};return treaty_recognition_codec_fields::reply(c,std::get<TreatyReplySource>(r));}
 if(index==1){if(c.reading())r=TreatyReleaseRequestSource{};return request(c,std::get<TreatyReleaseRequestSource>(r));}
 if(c.reading())r=TreatyReleaseAnswerSource{};return answer(c,std::get<TreatyReleaseAnswerSource>(r));
}
}
