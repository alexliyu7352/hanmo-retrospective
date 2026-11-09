#include "treaty_aid_proposal_official.hpp"
namespace hanmo::resident {
Outcome<OfficialContentRef> read_treaty_aid_proposal_official_content(const TreatyResourceSourceLinks2&x,const MomentOwnerSource&ref){
 auto original=read_treaty_aid_resource_proposal2_links(x,ref);if(!original.ok)return {false,{},original.error};
 OfficialContentRef out;out.kind=OfficialContentKind::treaty_aid_resource_proposal;out.id=original.value->id;out.terms_revision=Revision{original.value->terms_revision};auto valid=validate_official_content_ref(out);return {valid.ok,std::move(out),valid.error};
}
Outcome<const TreatyAidResourceProposalSource2*> read_treaty_aid_proposal_official_body(const TreatyResourceSourceLinks2&x,const OfficialContentRef&content){
 auto valid=validate_official_content_ref(content);if(!valid.ok)return {false,nullptr,valid.error};const auto*rev=std::get_if<Revision>(&content.terms_revision);
 if(content.kind!=OfficialContentKind::treaty_aid_resource_proposal||!rev)return {false,nullptr,common_failure("TreatyAidProposalOfficialSourceRequired",content.id).error};
 auto original=read_treaty_aid_resource_proposal2_links(x,{{"politics","treaty_aid_resources_proposed",content.id},2});if(!original.ok)return original;
 if(original.value->terms_revision!=rev->revision)return {false,nullptr,common_failure("TreatyAidProposalOfficialRevisionMismatch",content.id).error};return original;
}
bool native_treaty_aid_proposal_result_kind(const FactRecord&h){return h.domain=="politics"&&h.kind=="treaty_aid_resources_proposed";}
Outcome<WorldMoment> NativeTreatyAidProposalResultRead::read_selected_source_moment(const MomentOwnerSource&ref)const{auto r=read_treaty_aid_resource_proposal2_links(x_,ref);if(!r.ok)return {false,{},r.error};return {true,r.value->proposed_at,{}};}
Outcome<ExactOwnerFactRead> NativeTreatyAidProposalResultRead::read_selected_fact(const FactRecord&h)const{
 if(!native_treaty_aid_proposal_result_kind(h))return {false,{},common_failure("TreatyAidProposalResultUnsupported",h.id,ErrorKind::unsupported_capability).error};
 const SourceRef source{h.domain,h.kind,h.id};auto r=read_treaty_aid_resource_proposal2_links(x_,{source,2});if(!r.ok)return {false,{},r.error};auto terms=read_treaty_revision_source(x_.revisions,r.value->terms_source,1);if(!terms.ok)return {false,{},terms.error};if(!terms.value.definition)return {false,{},common_failure("TreatyAidProposalOriginalTermsMissing",h.id).error};
 DynamicFactViewV2 fact;fact.id=h.id;fact.kind=h.kind;fact.rule_version=terms.value.definition->terms.rule_version;fact.occurred_at=r.value->proposed_at;fact.time_basis=OwnerFactMoment{source,2};fact.cause_fact_ids.push_back(r.value->terms_source.id);fact.subject_id=r.value->treaty_id;fact.outcome="resources_proposed";fact.publicly_visible=false;
 const SubjectRef actor{SubjectKind::actor,r.value->proposer.actor_id};fact.participants.push_back({actor,"issuer"});ExactOwnerFactRead out;out.fact=std::move(fact);out.reportable=true;out.direct_observers.push_back(actor);return {true,std::move(out),{}};
}
Outcome<OfficialContentRef> read_treaty_aid_proposal_report_official_content(const TreatyResourceSourceLinks2&x,const SocialState&social,const ExactResultTimeRead&results,const ExactResultReadStamp&stamp,const StableId&id){
 const auto&common=x.revisions.common;if(!social.dynamic||!social.dynamic->time2)return {false,{},common_failure("TreatyAidProposalReportTime2Required",id).error};const auto*report=social.dynamic->reports.find(id);if(!report||report->id!=id)return {false,{},common_failure("TreatyAidProposalReportMissing",id).error};
 auto valid=validate_exact_report_source(common,*report,results,stamp);if(!valid.ok)return {false,{},valid.error};const auto*h=common.state().facts.find(report->fact_id);
 if(!h||!native_treaty_aid_proposal_result_kind(*h)||report->payload_kind!=h->kind)return {false,{},common_failure("TreatyAidProposalReportOriginalRequired",id).error};const SourceRef ref{h->domain,h->kind,h->id};auto original=read_treaty_aid_resource_proposal2_links(x,{ref,2});if(!original.ok)return {false,{},original.error};
 auto fact=results.fact_v2_borrowed(h->id,stamp);if(!fact.ok)return {false,{},fact.error};const auto*owner=fact.value?std::get_if<OwnerFactMoment>(&fact.value->time_basis):nullptr;
 if(!owner||owner->source_schema!=2||!same_source_ref(owner->source,ref))return {false,{},common_failure("TreatyAidProposalReportTimeSourceMismatch",id).error};auto at=compare_world_moments(original.value->proposed_at,fact.value->occurred_at);if(!at.ok)return {false,{},at.error};if(at.value)return {false,{},common_failure("TreatyAidProposalReportMomentMismatch",id).error};return read_treaty_aid_proposal_official_content(x,{ref,2});
}
bool NativeTreatyAidProposalReportOfficialContentRead::bound_to(const CommonStore&c,const SocialState&s)const noexcept{return &c==&x_.revisions.common&& &s==&social_;}
Outcome<std::optional<OfficialContentRef>> NativeTreatyAidProposalReportOfficialContentRead::read_report_official_content(const Report&report,const ExactResultTimeRead&results,const ExactResultReadStamp&stamp)const{
 if(!social_.dynamic||social_.dynamic->reports.find(report.id)!=&report)return {false,{},common_failure("TreatyAidProposalReportCandidateMismatch",report.id).error};const auto*h=x_.revisions.common.state().facts.find(report.fact_id);if(!h)return {false,{},common_failure("TreatyAidProposalReportOriginalMissing",report.id).error};if(!native_treaty_aid_proposal_result_kind(*h))return {true,std::nullopt,{}};auto r=read_treaty_aid_proposal_report_official_content(x_,social_,results,stamp,report.id);if(!r.ok)return {false,{},r.error};return {true,std::move(r.value),{}};
}
namespace {
Outcome<TreatyKnownAidProposalView> known(const TreatyResourceSourceLinks2&x,const SocialState&social,const CommunicationLinksState&links,const CommunicationSourceRead&communications,const ExactResultTimeRead&times,const ExactResultReadStamp&stamp,const StableId&actor,const OfficialContentRef&content,const MomentOwnerSource&delivery,const SourceRef*consumer){
 auto original=read_treaty_aid_proposal_official_body(x,content);if(!original.ok)return {false,{},original.error};
 auto receipt=consumer?read_actor_official_content_receipt_before_source(x.revisions.common,social,links,communications,times,stamp,actor,content,delivery,*consumer):read_actor_official_content_receipt(x.revisions.common,social,links,communications,times,stamp,actor,content,delivery);
 if(!receipt.ok)return {false,{},receipt.error};if(!receipt.value.business||!receipt.value.delivery||!receipt.value.report||!receipt.value.business->official_content_ref)return {false,{},common_failure("TreatyAidProposalDisclosureRequired",content.id).error};const auto&actual=*receipt.value.business->official_content_ref;
 if(actual.kind!=content.kind||actual.id!=content.id||!same_terms_revision(actual.terms_revision,content.terms_revision))return {false,{},common_failure("TreatyAidProposalDisclosureContentMismatch",content.id).error};auto order=compare_world_moments(original.value->proposed_at,receipt.value.delivery->delivered_at);if(!order.ok)return {false,{},order.error};if(order.value>0)return {false,{},common_failure("TreatyAidProposalDisclosureBeforeOriginal",content.id).error};return {true,{original.value,std::move(receipt.value)},{}};
}
}
Outcome<TreatyKnownAidProposalView> read_actor_known_treaty_aid_proposal(const TreatyResourceSourceLinks2&x,const SocialState&s,const CommunicationLinksState&l,const CommunicationSourceRead&c,const ExactResultTimeRead&t,const ExactResultReadStamp&stamp,const StableId&a,const OfficialContentRef&content,const MomentOwnerSource&d){return known(x,s,l,c,t,stamp,a,content,d,nullptr);}
Outcome<TreatyKnownAidProposalView> read_actor_known_treaty_aid_proposal_before_source(const TreatyResourceSourceLinks2&x,const SocialState&s,const CommunicationLinksState&l,const CommunicationSourceRead&c,const ExactResultTimeRead&t,const ExactResultReadStamp&stamp,const StableId&a,const OfficialContentRef&content,const MomentOwnerSource&d,const SourceRef&consumer){return known(x,s,l,c,t,stamp,a,content,d,&consumer);}
}
