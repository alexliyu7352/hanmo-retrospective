#include "social_original_result_projection.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> invalid(const char*code,const StableId&id={}){return {false,{},common_failure(code,id,ErrorKind::internal).error};}
Status bindings(const OriginalSocialRelationContext&c,const OriginalSocialEventSources&e){
 if(&c.common!=&e.common||&c.social!=&e.social||!c.sources.bound_to(c.common,c.social))return common_failure("OriginalSocialResultCandidateMismatch",{},ErrorKind::internal);
 return common_success();
}
}
bool original_social_supporting_fact_kind(const FactRecord&h)noexcept{return h.domain=="social"&&(h.kind=="social_event_applied"||h.kind=="relation_applied");}
Outcome<WorldMoment> read_original_social_supporting_source_moment(const OriginalSocialRelationContext&c,const OriginalSocialEventSources&e,const MomentOwnerSource&source){
 auto valid=bindings(c,e);if(!valid.ok)return {false,{},valid.error};const auto&r=source.source_ref;
 if(source.source_schema!=1||r.domain!="social")return invalid<WorldMoment>("OriginalSocialResultSourceMismatch",r.id);
 if(r.kind=="social_event_applied"){
  auto read=read_original_social_calendar_event(e,r,source.source_schema,c.actual_now);if(!read.ok)return {false,{},read.error};return {true,read.value->original_application->occurred_at,{}};
 }
 if(r.kind=="relation_applied"){
  auto read=read_original_social_relation_result(c,r,source.source_schema);if(!read.ok)return {false,{},read.error};return {true,read.value->applied_at,{}};
 }
 return invalid<WorldMoment>("OriginalSocialResultSourceMismatch",r.id);
}
Outcome<ExactOwnerFactRead> read_original_social_supporting_fact(const OriginalSocialRelationContext&c,const OriginalSocialEventSources&e,const FactRecord&h){
 if(!original_social_supporting_fact_kind(h))return invalid<ExactOwnerFactRead>("OriginalSocialResultKindMismatch",h.id);
 const auto*actual=c.common.state().facts.find(h.id);if(!actual||actual!=&h||actual->id!=h.id||actual->domain!=h.domain||actual->kind!=h.kind)return invalid<ExactOwnerFactRead>("OriginalSocialResultHeaderMismatch",h.id);
 const MomentOwnerSource source{{h.domain,h.kind,h.id},1};auto at=read_original_social_supporting_source_moment(c,e,source);if(!at.ok)return {false,{},at.error};
 ExactOwnerFactRead out;out.disposition=ExactOwnerFactRead::Disposition::no_dynamic_projection;out.supporting_sources.push_back(source);return {true,std::move(out),{}};
}
}
