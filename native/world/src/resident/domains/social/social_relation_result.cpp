#include "social_relation_result.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
Integer clamp_original_social_relation(Integer before,Integer delta)noexcept{return std::clamp(before+delta,Integer(0),Integer(100));}
Outcome<OriginalSocialRelationBusinessSourceView> original_social_relation_business_source(const OriginalSocialRelationCause&cause){
 if(const auto*exchange=std::get_if<SocialExchangeRelationCause>(&cause))return {true,{"social","social_event_applied",&exchange->event_id,1},{}};
 if(const auto*news=std::get_if<SocialNewsRelationCause>(&cause))return {true,{"social","social_event_applied",&news->news_event_id,1},{}};
 const MomentOwnerSource*source=nullptr;
 if(const auto*task=std::get_if<SocialTaskReportRelationCause>(&cause))source=&task->actual_terminal;
 else if(const auto*work=std::get_if<SocialGrowthServiceRelationCause>(&cause))source=&work->actual_terminal;
 if(!source)return {false,{},common_failure("InvalidOriginalRelationCause",{},ErrorKind::internal).error};
 return {true,{source->source_ref.domain,source->source_ref.kind,&source->source_ref.id,source->source_schema},{}};
}
namespace {bool source_equal(const MomentOwnerSource&a,const MomentOwnerSource&b)noexcept{return a.source_schema==b.source_schema&&a.source_ref.domain==b.source_ref.domain&&a.source_ref.kind==b.source_ref.kind&&a.source_ref.id==b.source_ref.id;}}
bool same_original_social_relation_cause(const OriginalSocialRelationCause&a,const OriginalSocialRelationCause&b)noexcept{
 if(a.valueless_by_exception()||b.valueless_by_exception()||a.index()!=b.index())return false;
 if(const auto*x=std::get_if<SocialExchangeRelationCause>(&a)){const auto&y=std::get<SocialExchangeRelationCause>(b);return x->event_id==y.event_id&&x->role==y.role;}
 if(const auto*x=std::get_if<SocialNewsRelationCause>(&a))return x->news_event_id==std::get<SocialNewsRelationCause>(b).news_event_id;
 if(const auto*x=std::get_if<SocialTaskReportRelationCause>(&a)){const auto&y=std::get<SocialTaskReportRelationCause>(b);return x->task_id==y.task_id&&x->settlement_id==y.settlement_id&&source_equal(x->actual_terminal,y.actual_terminal);}
 const auto&x=std::get<SocialGrowthServiceRelationCause>(a);const auto&y=std::get<SocialGrowthServiceRelationCause>(b);return x.action_id==y.action_id&&source_equal(x.actual_terminal,y.actual_terminal);
}
Status validate_social_relation_result_shape(const SocialRelationResult&r){
 auto bad=[&](const char*code){return common_failure(code,r.result_id,ErrorKind::internal);};
 if(r.original_cause.valueless_by_exception())return bad("InvalidOriginalRelationCause");
 if(r.result_id.empty()||r.relation_record_key.empty()||r.from_actor.empty()||r.to_actor.empty()||r.from_actor==r.to_actor)return bad("InvalidOriginalRelationResultIdentity");
 for(auto v:{r.trust_before,r.affinity_before,r.trust_after,r.affinity_after})if(v<0||v>100)return bad("InvalidOriginalRelationResultValue");
 // All actual legacy branches are within this finite range; shape is not an
 // authorization. The source reader must re-derive the exact original delta.
 if(r.trust_delta < -30||r.trust_delta>10||r.affinity_delta < -10||r.affinity_delta>5||r.trust_after!=clamp_original_social_relation(r.trust_before,r.trust_delta)||r.affinity_after!=clamp_original_social_relation(r.affinity_before,r.affinity_delta))return bad("OriginalRelationResultClampMismatch");
 auto time=compare_world_moments(r.applied_at,r.applied_at);if(!time.ok)return {false,{},time.error};
 if(const auto*x=std::get_if<SocialExchangeRelationCause>(&r.original_cause)){auto role=static_cast<Integer>(x->role);if(x->event_id.empty()||role<0||role>4)return bad("InvalidOriginalRelationExchangeCause");}
 else if(const auto*x=std::get_if<SocialNewsRelationCause>(&r.original_cause)){if(x->news_event_id.empty())return bad("InvalidOriginalRelationNewsCause");}
 else if(const auto*x=std::get_if<SocialTaskReportRelationCause>(&r.original_cause)){if(x->task_id.empty()||x->settlement_id.empty()||x->actual_terminal.source_schema!=1||x->actual_terminal.source_ref.domain!="tasks"||x->actual_terminal.source_ref.kind!="task_action_finished"||x->actual_terminal.source_ref.id.empty())return bad("InvalidOriginalRelationTaskCause");
 } else {
   const auto &growth =
       std::get<SocialGrowthServiceRelationCause>(r.original_cause);
   if (growth.action_id.empty() || growth.actual_terminal.source_schema != 1 ||
       growth.actual_terminal.source_ref.domain != "growth" ||
       growth.actual_terminal.source_ref.kind != "growth_result_terminal" ||
       growth.actual_terminal.source_ref.id.empty())
     return bad("InvalidOriginalRelationGrowthCause");
 }
 return common_success();
}
}
