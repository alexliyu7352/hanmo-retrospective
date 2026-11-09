// Full original authority/knowledge reads for explicitly selected signature/2.
#pragma once
#include "treaty_aid_proposal_official.hpp"
namespace hanmo::resident {
struct TreatyResourceSignatureReadContext {
 const TreatyResourceSourceLinks2& links;
 const TreatyRatificationTermAuthorityRead& term_authorities;
 const SocialState& social;const CommunicationLinksState& communications;
 const CommunicationSourceRead& communication_sources;const ExactResultTimeRead& times;
 const ExactResultReadStamp& result_stamp;
};
class NativeTreatyRatificationOriginalRead2 final:public TreatyRatificationOriginalRead {
 const TreatyResourceSignatureReadContext& context_;
public:explicit NativeTreatyRatificationOriginalRead2(const TreatyResourceSignatureReadContext&x):context_(x){}
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
 Outcome<TreatyRatificationAuthorityRequirements> read_original_ratification_requirements(const SourceRef&,Integer,const StableId&,const ActingAuthority&,const SourceRef*)const override;
 Outcome<TreatyRatificationOriginalView> read_actual_treaty_ratification(const SourceRef&)const override;
};
Outcome<const TreatyResourceSignatureSource2*> read_treaty_resource_signature2(const TreatyResourceSignatureReadContext&,const MomentOwnerSource&);
Status validate_treaty_resource_signature2_sources(const TreatyResourceSignatureReadContext&);
}
