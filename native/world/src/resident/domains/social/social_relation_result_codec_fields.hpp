#pragma once
#include "social_relation_result.hpp"
#include "resident/domains/family/birth_due_execution_codec_fields.hpp"
namespace hanmo::resident::social_relation_codec {
template<class C>Status cause(C&c,OriginalSocialRelationCause&r){Integer tag=static_cast<Integer>(r.index());c.field("tag",tag);if(tag<0||tag>3)return c.reject("InvalidOriginalRelationCauseTag");if(c.reading()){switch(tag){case 0:r=SocialExchangeRelationCause{};break;case 1:r=SocialNewsRelationCause{};break;case 2:r=SocialTaskReportRelationCause{};break;case 3:r=SocialGrowthServiceRelationCause{};break;}}
 if(tag==0){auto&v=std::get<SocialExchangeRelationCause>(r);c.field("event_id",v.event_id);Integer role=static_cast<Integer>(v.role);c.field("role",role);if(role<0||role>4)return c.reject("InvalidOriginalRelationExchangeRole");if(c.reading())v.role=static_cast<OriginalSocialExchangeRole>(role);return c.status();}
 if(tag==1)return c.field("news_event_id",std::get<SocialNewsRelationCause>(r).news_event_id);
 if(tag==2){auto&v=std::get<SocialTaskReportRelationCause>(r);c.field("task_id",v.task_id);c.field("settlement_id",v.settlement_id);return c.object("actual_terminal",[&](auto&x){return birth_due_codec::moment_source(x,v.actual_terminal);});}
 auto&v=std::get<SocialGrowthServiceRelationCause>(r);c.field("action_id",v.action_id);return c.object("actual_terminal",[&](auto&x){return birth_due_codec::moment_source(x,v.actual_terminal);});
}
template<class C>Status result(C&c,SocialRelationResult&r){c.field("result_id",r.result_id);c.field("relation_record_key",r.relation_record_key);c.field("from_actor",r.from_actor);c.field("to_actor",r.to_actor);c.object("original_cause",[&](auto&x){return cause(x,r.original_cause);});c.field("trust_before",r.trust_before);c.field("affinity_before",r.affinity_before);c.field("trust_delta",r.trust_delta);c.field("affinity_delta",r.affinity_delta);c.field("trust_after",r.trust_after);c.field("affinity_after",r.affinity_after);c.object("applied_at",[&](auto&x){return family_life4_codec::moment(x,r.applied_at);});if(!c.status().ok)return c.status();auto valid=validate_social_relation_result_shape(r);return valid.ok?c.status():c.reject(valid.error.original.code);}
}
