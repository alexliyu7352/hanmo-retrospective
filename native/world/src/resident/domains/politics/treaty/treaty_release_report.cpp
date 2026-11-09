#include "treaty_release_report.hpp"
#include "resident/domains/social/social_time2.hpp"
namespace hanmo::resident {
Outcome<OfficialContentRef> read_treaty_release_report_official_content(const TreatyReleaseContext& x,const SocialState& social,const ExactResultTimeRead& results,const ExactResultReadStamp& stamp,const StableId& id){
 const auto& common=x.treaties.activations.signatures.revisions.common;
 if(!social.dynamic||!social.dynamic->time2)return {false,{},common_failure("TreatyReleaseReportTime2Required",id).error};
 const auto* report=social.dynamic->reports.find(id);if(!report||report->id!=id)return {false,{},common_failure("TreatyReleaseReportMissing",id).error};
 auto checked=validate_exact_report_source(common,*report,results,stamp);if(!checked.ok)return {false,{},checked.error};
 const auto* head=common.state().facts.find(report->fact_id);if(!head||head->domain!="politics"||head->kind!="treaty_release_requested"||report->payload_kind!=head->kind)return {false,{},common_failure("TreatyReleaseReportOriginalRequired",id,ErrorKind::unsupported_capability).error};
 const SourceRef ref{head->domain,head->kind,head->id};auto original=read_treaty_release_request(x,ref,1);if(!original.ok)return {false,{},original.error};
 auto fact=results.fact_v2_borrowed(head->id,stamp);if(!fact.ok)return {false,{},fact.error};const auto* source=fact.value?std::get_if<OwnerFactMoment>(&fact.value->time_basis):nullptr;
 if(!source||source->source_schema!=1||!same_source_ref(source->source,ref))return {false,{},common_failure("TreatyReleaseReportSourceMismatch",id).error};
 auto at=compare_world_moments(original.value->requested_at,fact.value->occurred_at);if(!at.ok)return {false,{},at.error};if(at.value)return {false,{},common_failure("TreatyReleaseReportMomentMismatch",id).error};
 return read_treaty_release_request_official_content(x,ref,1);
}
bool NativeTreatyReleaseReportOfficialContentRead::bound_to(const CommonStore& common,const SocialState& social)const noexcept{return &common==&x_.treaties.activations.signatures.revisions.common&&&social==&social_;}
Outcome<std::optional<OfficialContentRef>> NativeTreatyReleaseReportOfficialContentRead::read_report_official_content(const Report& report,const ExactResultTimeRead& results,const ExactResultReadStamp& stamp)const{
 if(!social_.dynamic||social_.dynamic->reports.find(report.id)!=&report)return {false,{},common_failure("TreatyReleaseReportCandidateMismatch",report.id).error};
 const auto* head=x_.treaties.activations.signatures.revisions.common.state().facts.find(report.fact_id);if(!head)return {false,{},common_failure("TreatyReleaseReportOriginalMissing",report.id).error};
 if(head->domain!="politics"||head->kind!="treaty_release_requested")return {true,std::nullopt,{}};
 auto r=read_treaty_release_report_official_content(x_,social_,results,stamp,report.id);if(!r.ok)return {false,{},r.error};return {true,std::move(r.value),{}};
}
}
