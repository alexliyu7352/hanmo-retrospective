#include "treaty_result_projection.hpp"
#include "resident/domains/social/social_time2.hpp"
namespace hanmo::resident {
bool native_treaty_revision_result_kind(const FactRecord& h){return h.domain=="politics"&&(h.kind=="treaty_terms_offered"||h.kind=="treaty_terms_counteroffered");}
Outcome<WorldMoment> NativeTreatyRevisionResultRead::read_selected_source_moment(const MomentOwnerSource& s)const{
 auto r=read_treaty_revision_source(context_,s.source_ref,s.source_schema);if(!r.ok)return {false,{},r.error};return {true,r.value.issued_at,{}};
}
Outcome<ExactOwnerFactRead> NativeTreatyRevisionResultRead::read_selected_fact(const FactRecord& h)const{
 if(!native_treaty_revision_result_kind(h))return {false,{},common_failure("TreatyRevisionResultUnsupported",h.id,ErrorKind::unsupported_capability).error};
 const SourceRef source{h.domain,h.kind,h.id};auto r=read_treaty_revision_source(context_,source,1);if(!r.ok)return {false,{},r.error};
 if(!r.value.definition||!r.value.issuer)return {false,{},common_failure("TreatyRevisionOriginalMissing",h.id).error};
 DynamicFactViewV2 v;v.id=h.id;v.kind=h.kind;v.rule_version=r.value.definition->terms.rule_version;v.occurred_at=r.value.issued_at;v.time_basis=OwnerFactMoment{source,1};
 if(h.kind=="treaty_terms_counteroffered"){const auto* row=context_.counteroffers.find(h.id);if(!row)return {false,{},common_failure("TreatyCounterofferMissing",h.id).error};v.cause_fact_ids.push_back(row->previous_terms_source.id);}
 // An original proposer knows the actual proposal they issued. Other parties,
 // the institution and remote officers need their own actual communication.
 const SubjectRef proposer{SubjectKind::actor,r.value.issuer->actor_id};v.participants.push_back({proposer,"issuer"});v.subject_id=r.value.treaty_id;v.outcome=h.kind=="treaty_terms_offered"?"offered":"counteroffered";v.publicly_visible=false;
 ExactOwnerFactRead out;out.fact=std::move(v);out.reportable=true;out.direct_observers.push_back(proposer);return {true,std::move(out),{}};
}
Outcome<OfficialContentRef> read_treaty_report_official_content(const TreatyRevisionContext& x,const SocialState& social,const ExactResultTimeRead& results,const ExactResultReadStamp& stamp,const StableId& id){
 if(!social.dynamic||!social.dynamic->time2)return {false,{},common_failure("TreatyReportTime2Required",id).error};
 const auto* report=social.dynamic->reports.find(id);if(!report||report->id!=id)return {false,{},common_failure("TreatyActualReportMissing",id).error};
 auto checked=validate_exact_report_source(x.common,*report,results,stamp);if(!checked.ok)return {false,{},checked.error};
 const auto* h=x.common.state().facts.find(report->fact_id);if(!h||!native_treaty_revision_result_kind(*h)||report->payload_kind!=h->kind)return {false,{},common_failure("TreatyReportOriginalSourceRequired",id,ErrorKind::unsupported_capability).error};
 const SourceRef source{h->domain,h->kind,h->id};auto original=read_treaty_revision_source(x,source,1);if(!original.ok)return {false,{},original.error};
 auto fact=results.fact_v2_borrowed(h->id,stamp);if(!fact.ok)return {false,{},fact.error};
 const auto* owner=fact.value?std::get_if<OwnerFactMoment>(&fact.value->time_basis):nullptr;
 if(!owner||owner->source_schema!=1||!same_source_ref(owner->source,source))return {false,{},common_failure("TreatyReportTimeSourceMismatch",id).error};
 auto at=compare_world_moments(original.value.issued_at,fact.value->occurred_at);if(!at.ok)return {false,{},at.error};if(at.value)return {false,{},common_failure("TreatyReportMomentMismatch",id).error};
 return read_treaty_revision_official_content(x,source,1);
}
bool NativeTreatyReportOfficialContentRead::bound_to(const CommonStore& c,const SocialState& s)const noexcept{return &c == &context_.common && &s == &social_;}
Outcome<std::optional<OfficialContentRef>> NativeTreatyReportOfficialContentRead::read_report_official_content(const Report& report,const ExactResultTimeRead& results,const ExactResultReadStamp& stamp)const{
 if(!social_.dynamic||social_.dynamic->reports.find(report.id)!=&report)return {false,{},common_failure("TreatyOfficialReportCandidateMismatch",report.id).error};
 const auto* h=context_.common.state().facts.find(report.fact_id);if(!h)return {false,{},common_failure("TreatyOfficialReportOriginalMissing",report.id).error};
 if(!native_treaty_revision_result_kind(*h))return {true,std::nullopt,{}};
 auto r=read_treaty_report_official_content(context_,social_,results,stamp,report.id);if(!r.ok)return {false,{},r.error};return {true,std::move(r.value),{}};
}

}
