#include "social_knowledge_cut.hpp"
namespace hanmo::resident {namespace {
const FactRecord* actual(const CommonStore&c,const SourceRef&s){const auto*h=c.state().facts.find(s.id);return h&&h->id==s.id&&h->domain==s.domain&&h->kind==s.kind?h:nullptr;}
Status ordered(const CommonStore&c,const SourceRef&left,const SourceRef&right,bool strict){
 if(!actual(c,left)||!actual(c,right))return common_failure("KnowledgeCausalCommonSourceMissing",right.id,ErrorKind::internal);
 const auto&rows=c.state().facts.order();auto a=std::find(rows.begin(),rows.end(),left.id),b=std::find(rows.begin(),rows.end(),right.id);
 if(a==rows.end()||b==rows.end()||(strict?a>=b:a>b))return common_failure("KnowledgeCausalCommonOrderMismatch",right.id,ErrorKind::internal);
 return common_success();
}
}
Outcome<Integer> select_social_time2_physical_version(Integer base,bool knowledge,bool adoption){if(adoption&&!knowledge)return {false,0,common_failure("PropagationKnowledgeCutSelectionRequired",{},ErrorKind::unsupported_capability).error};auto original=select_social_time2_physical_version(base,knowledge);if(!original.ok||!adoption)return original;return {true,original.value+2,{}};}
Outcome<Integer> select_social_time2_physical_version(Integer base,bool cut){if(base!=1&&base!=2)return {false,0,common_failure("InvalidOriginalDynamicSocialVersion",{},ErrorKind::unsupported_capability).error};return {true,base+(cut?4:2),{}};}
Status capture_social_knowledge_cut(const CommonStore&common,Integer version,SocialSourceTimeBinding&binding){
 if(!social_time2_version(version))return common_failure("ExactSocialPhysicalSelectionRequired",{},ErrorKind::unsupported_capability);
 if(binding.learned_after_common_fact)return common_failure("KnowledgeCausalCutAlreadyCaptured",binding.actual_source_ref.id,ErrorKind::internal);
 if(!std::holds_alternative<SocialKnowledgeKey>(binding.key))return common_failure("KnowledgeCausalCutKeyRequired",binding.actual_source_ref.id,ErrorKind::internal);
 if(!social_knowledge_cut_version(version))return common_success();
 const auto&order=common.state().facts.order();if(order.empty())return common_failure("KnowledgeActualCommonPrefixRequired",{},ErrorKind::unsupported_capability);
 const auto*h=common.state().facts.find(order.back());if(!h||h->id!=order.back()||h->domain.empty()||h->kind.empty())return common_failure("KnowledgeCausalCommonTailMissing",{},ErrorKind::internal);
 binding.learned_after_common_fact=SourceRef{h->domain,h->kind,h->id};return common_success();
}
Status validate_social_knowledge_cut_source(const CommonStore&common,const SocialSourceTimeBinding&binding,const SourceRef&source){
 if(!binding.learned_after_common_fact)return common_failure("KnowledgeSameMomentAcquisitionCutRequired",source.id,ErrorKind::unsupported_capability);
 return ordered(common,source,*binding.learned_after_common_fact,false);
}
Status validate_social_knowledge_cut_before_source(const CommonStore&common,const SocialSourceTimeBinding&binding,const SourceRef&consumer){
 if(!binding.learned_after_common_fact)return common_failure("KnowledgeSameMomentAcquisitionCutRequired",consumer.id,ErrorKind::unsupported_capability);
 return ordered(common,*binding.learned_after_common_fact,consumer,true);
}
Status validate_social_knowledge_cut(const CommonStore&common,const DynamicSocialState&state,const SocialSourceTimeBinding&binding){
 const auto*key=std::get_if<SocialKnowledgeKey>(&binding.key);
 if(!social_knowledge_cut_version(state.version)||!key)return binding.learned_after_common_fact?common_failure("UnselectedKnowledgeCommonCut",binding.actual_source_ref.id,ErrorKind::internal):common_success();
 if(!binding.learned_after_common_fact||!actual(common,*binding.learned_after_common_fact))return common_failure("KnowledgeActualCommonPrefixRequired",key->report_id,ErrorKind::internal);
 const auto*r=state.reports.find(key->report_id);const auto*h=r?common.state().facts.find(r->fact_id):nullptr;
 if(!h||!find_social_knowledge(state,*key))return common_failure("KnowledgeCausalReportMissing",key->report_id,ErrorKind::internal);
 auto valid=validate_social_knowledge_cut_source(common,binding,{h->domain,h->kind,h->id});if(!valid.ok)return valid;
 // Most acquisition refs are typed owner rows without Common headers. Their
 // original channel validator checks the actual witness against this same cut.
 if(actual(common,binding.actual_source_ref))return validate_social_knowledge_cut_source(common,binding,binding.actual_source_ref);
 if((binding.actual_source_ref.domain=="social"&&(binding.actual_source_ref.kind=="report_participant"||binding.actual_source_ref.kind=="social_period_close"))||(binding.actual_source_ref.domain=="interaction"&&(binding.actual_source_ref.kind=="communication_delivery"||binding.actual_source_ref.kind=="communication_dossier_access")))return common_success();
 return common_failure("KnowledgeOriginalAcquisitionCutRequired",binding.actual_source_ref.id,ErrorKind::unsupported_capability);
}
}
