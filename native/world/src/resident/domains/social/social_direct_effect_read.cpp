#include "social_direct_effect_read.hpp"
namespace hanmo::resident {namespace {
using Read=std::optional<OriginalSocialDirectEffectRead>;
Outcome<Read> from_result(const OriginalSocialRelationContext&c,const SocialRelationResult&r){
 if(std::holds_alternative<SocialNewsRelationCause>(r.original_cause))return {true,{}, {}};
 auto original=c.sources.read_original_application(r.original_cause,c.actual_now);if(!original.ok)return {false,{},original.error};
 auto business=original_social_relation_business_source(r.original_cause);if(!business.ok)return {false,{},business.error};
 return {true,OriginalSocialDirectEffectRead{&r,business.value,original.value.rule},{}};
}
}
Outcome<std::optional<OriginalSocialDirectEffectRead>> read_original_social_direct_effect(
 const OriginalSocialRelationContext&c,const SourceRef&ref,Integer schema){
 auto result=read_original_social_relation_result(c,ref,schema);if(!result.ok)return {false,{},result.error};
 return from_result(c,*result.value);
}
Outcome<std::optional<OriginalSocialDirectEffectRead>> read_original_social_direct_effect_at_cut(
 const OriginalSocialRelationContext&c,const SourceRef&ref,Integer schema,const OriginalSocialRelationCut&cut){
 auto result=read_original_social_relation_result(c,ref,schema);if(!result.ok)return {false,{},result.error};
 // Reuse the sole full relation-at-cut validator, including all original
 // source/M-prefix checks. No separate partial historical fold is introduced.
 auto history=read_original_social_relation_at_cut(c,result.value->from_actor,result.value->to_actor,cut);if(!history.ok)return {false,{},history.error};
 auto limit=read_actual_common_fact_prefix(c.common,cut.original_prefix);if(!limit.ok)return {false,{},limit.error};
 for(std::size_t i=0;i<limit.value;++i)if(c.common.state().facts.rows()[i].first==ref.id)return from_result(c,*result.value);
 return {false,{},common_failure("OriginalSocialDirectEffectNotBeforeAdoption",ref.id,ErrorKind::internal).error};
}
}
