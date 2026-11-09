// M-TREATY signatures: explicit same-version personal ratification. The one
// persistent authority value remains OrganizationRuntime.authority_bases.
#pragma once
#include "treaty_official_content.hpp"
#include "treaty_ratification_authority.hpp"
namespace hanmo::resident {
// 同一签署只有一种原签发身份；使者不借用Office身份或另存一份AuthorityBasis。
struct TreatyDirectOfficeSigner {ActingAuthority authority;};
struct TreatyMandatedSigner {
 StableId actor_id;
 MomentOwnerSource actual_mandate_ref,actual_task_acceptance_ref;
};
using TreatySigner=std::variant<TreatyDirectOfficeSigner,TreatyMandatedSigner>;
struct TreatySignatureSource {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef terms_source;
 StableId party_id;TreatySigner signer;MomentOwnerSource actual_content_receipt;
 WorldMoment signed_at;
};
class TreatyRatificationTermAuthorityRead {public:virtual ~TreatyRatificationTermAuthorityRead()=default;
 // Real term handlers derive scope/exposure from this exact immutable original
 // revision and the original historical control/accepted resource choices.
 virtual Outcome<TreatyRatificationAuthorityRequirements> read_original_term_authority_requirements(
  const TreatyRevisionView&,const StableId& party,const ActingAuthority&,
  const SourceRef* actual_signature_boundary)const=0;
};
struct TreatySignatureContext {
 const TreatyRevisionContext& revisions;const OrderedTable<TreatySignatureSource>& signatures;
 const TreatyRatificationTermAuthorityRead& term_authorities;
 const SocialState& social;const CommunicationLinksState& communications;
 const CommunicationSourceRead& communication_sources;const ExactResultTimeRead& times;
 const ExactResultReadStamp& result_stamp;
};
class NativeTreatyRatificationOriginalRead final:public TreatyRatificationOriginalRead {
 const TreatySignatureContext& context_;
public:explicit NativeTreatyRatificationOriginalRead(const TreatySignatureContext& c):context_(c){}
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
 Outcome<TreatyRatificationAuthorityRequirements> read_original_ratification_requirements(
  const SourceRef&,Integer,const StableId&,const ActingAuthority&,const SourceRef*)const override;
 Outcome<TreatyRatificationOriginalView> read_actual_treaty_ratification(const SourceRef&)const override;
};
SourceRef treaty_signature_source(const TreatySignatureSource&);
// 只读实际本人身份；此投影不证明mandate/Task接受或代签范围。
const StableId& treaty_signature_actor(const TreatySignatureSource&);
Status validate_treaty_signature_source_shape(const TreatySignatureSource&);
Outcome<const TreatySignatureSource*> read_treaty_signature_source(const TreatySignatureContext&,const SourceRef&,Integer schema);
Status validate_treaty_signature_sources(const TreatySignatureContext&);
struct RatifyTreatyTermsCommand {ActingAuthority authority;StableId treaty_id;Integer terms_revision=0;SourceRef terms_source;MomentOwnerSource actual_content_receipt;};
// The direct Office/Grant signing path only. External-mandate signing requires
// its actual original mandate/acceptance source; it is not silently inferred here.
Outcome<SourceRef> ratify_treaty_terms_in_candidate(CommonStore&,OrderedTable<TreatySignatureSource>&,
 AuthorityBasisRegistry&,const TreatySignatureContext&,const RatifyTreatyTermsCommand&);
Status visit_treaty_signature_source(CodecVisitor&,TreatySignatureSource&);
Status emit_treaty_signature_source_projection(ProjectionWriter&,std::string_view,const TreatySignatureSource&);
Outcome<BoundaryValue> encode_treaty_signature_source(const TreatySignatureSource&);
Outcome<TreatySignatureSource> decode_treaty_signature_source(const BoundaryValue&);
}
