#pragma once
#include "treaty_resource_source_links2.hpp"
#include "resident/domains/interaction/report_official_content.hpp"
#include "resident/domains/interaction/official_content_knowledge.hpp"
namespace hanmo::resident {
Outcome<OfficialContentRef> read_treaty_aid_proposal_official_content(const TreatyResourceSourceLinks2&,const MomentOwnerSource&);
Outcome<const TreatyAidResourceProposalSource2*> read_treaty_aid_proposal_official_body(const TreatyResourceSourceLinks2&,const OfficialContentRef&);
bool native_treaty_aid_proposal_result_kind(const FactRecord&);
class NativeTreatyAidProposalResultRead final:public ExactResultOwnerRead {
 const TreatyResourceSourceLinks2& x_;
public:explicit NativeTreatyAidProposalResultRead(const TreatyResourceSourceLinks2&x):x_(x){}
 Outcome<ExactOwnerFactRead> read_selected_fact(const FactRecord&)const override;
 Outcome<WorldMoment> read_selected_source_moment(const MomentOwnerSource&)const override;
};
Outcome<OfficialContentRef> read_treaty_aid_proposal_report_official_content(const TreatyResourceSourceLinks2&,
 const SocialState&,const ExactResultTimeRead&,const ExactResultReadStamp&,const StableId& report_id);
class NativeTreatyAidProposalReportOfficialContentRead final:public ReportOfficialContentRead {
 const TreatyResourceSourceLinks2& x_;const SocialState& social_;
public:NativeTreatyAidProposalReportOfficialContentRead(const TreatyResourceSourceLinks2&x,const SocialState&s):x_(x),social_(s){}
 bool bound_to(const CommonStore&,const SocialState&)const noexcept override;
 Outcome<std::optional<OfficialContentRef>> read_report_official_content(const Report&,const ExactResultTimeRead&,const ExactResultReadStamp&)const override;
};
struct TreatyKnownAidProposalView {
 const TreatyAidResourceProposalSource2* original=nullptr;
 ActorOfficialContentReceiptView actual_receipt;
};
// Actual source validator for a signature already in the same selected root.
// Its success only proves selected-resource knowledge, not signatory authority.
Status validate_treaty_signature2_resource_knowledge(const TreatyResourceSourceLinks2&,
 const SocialState&,const CommunicationLinksState&,const CommunicationSourceRead&,const ExactResultTimeRead&,
 const ExactResultReadStamp&,const TreatyResourceSignatureSource2&);
// These verify actual personal disclosure, not consent to receive resources.
Outcome<TreatyKnownAidProposalView> read_actor_known_treaty_aid_proposal(const TreatyResourceSourceLinks2&,
 const SocialState&,const CommunicationLinksState&,const CommunicationSourceRead&,const ExactResultTimeRead&,
 const ExactResultReadStamp&,const StableId& actor,const OfficialContentRef&,const MomentOwnerSource& delivery);
Outcome<TreatyKnownAidProposalView> read_actor_known_treaty_aid_proposal_before_source(const TreatyResourceSourceLinks2&,
 const SocialState&,const CommunicationLinksState&,const CommunicationSourceRead&,const ExactResultTimeRead&,
 const ExactResultReadStamp&,const StableId& actor,const OfficialContentRef&,const MomentOwnerSource& delivery,const SourceRef& actual_consumer);
}
