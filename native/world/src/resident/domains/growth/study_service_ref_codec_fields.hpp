// Original ST address fields, shared with the one ActualService scope codec.
// This does not encode the external service slot/schema or a second payload.
#pragma once
#include "study_service_model.hpp"
#include <type_traits>
namespace hanmo::resident::study_service_ref_codec_fields {
template<class C,class V>Status text(C& c,const char* name,V& value){
 c.field(name,value);if(c.status().ok&&value.empty())return c.reject("StudyCapabilityReferenceMissing",name);return c.status();
}
template<class C,class V>Status accepted(C& c,V& v){
 static_assert(std::is_same_v<std::remove_const_t<V>,StudyAcceptedExecutionRef>);
 Integer tag=static_cast<Integer>(v.index());c.field("tag",tag);if(!c.status().ok)return c.status();
 if(tag<0||tag>1)return c.reject("StudyAcceptedExecutionTagInvalid");
 if(c.reading()){
  if constexpr(std::is_const_v<V>)return c.reject("ImmutableStudyAcceptedExecutionRead");
  else v=tag==0?StudyAcceptedExecutionRef{StudySkillStart{}}:StudyAcceptedExecutionRef{TeachStudyRequestStart{}};
 }
 if(tag==0)return text(c,"action_id",std::get<StudySkillStart>(v).action_id);
 auto& row=std::get<TeachStudyRequestStart>(v);text(c,"action_id",row.action_id);text(c,"request_id",row.request_id);return text(c,"session_id",row.session_id);
}
template<class C,class V>Status terms(C& c,V& v){static_assert(std::is_same_v<std::remove_const_t<V>,StudyFactTermsRef>);return text(c,"action_id",v.action_id);}
template<class C,class V>Status terminal(C& c,V& v){static_assert(std::is_same_v<std::remove_const_t<V>,StudyTerminal>);return text(c,"action_id",v.action_id);}
template<class C,class V>Status charge(C& c,V& v){
 static_assert(std::is_same_v<std::remove_const_t<V>,StudyLessonCharge>);
 text(c,"action_id",v.action_id);c.field("lesson_ordinal",v.lesson_ordinal);
 return c.object("accepted_terms_ref",[&](auto& x){return terms(x,v.accepted_terms_ref);});
}
}
