// Explicit successor: no Aid selection meaning is added to signature/1.
#pragma once
#include "treaty_aid_resource_selection.hpp"
namespace hanmo::resident {
// A proposal only names explicit choices. It is not the beneficiary's consent,
// historical container authority, ratification or a Schedule commitment.
struct TreatyAidResourceProposalSource {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef terms_source;
 ActingAuthority proposer;WorldMoment proposed_at;
 std::vector<TreatyAidResourceSelection> selections;
};
struct TreatyResourceOwnChoice {};
struct TreatyResourceDeliveredChoice {MomentOwnerSource actual_receipt;};
using TreatyResourceKnowledge=std::variant<TreatyResourceOwnChoice,TreatyResourceDeliveredChoice>;
struct TreatyResourceSignatureSource2 {
 TreatySignatureSource signature;
 std::optional<MomentOwnerSource> actual_aid_resource_proposal;
 // Explicit /2 draft successor: own same-candidate choice or actual disclosure.
 // A delivery is knowledge only, never recipient resource consent.
 std::optional<TreatyResourceKnowledge> resource_knowledge;
};
SourceRef treaty_aid_resource_proposal_source(const TreatyAidResourceProposalSource&);
Status validate_treaty_aid_resource_proposal_shape(const TreatyAidResourceProposalSource&);
Status validate_treaty_resource_signature2_shape(const TreatyResourceSignatureSource2&);
// Pure immutable payload binding, NOT a replacement for original historical
// signature authority, actual resource custody or beneficiary acceptance reads.
Status validate_treaty_aid_resource_proposal_terms(const TreatyAidResourceProposalSource&,const TreatyRevisionView&);
Status validate_treaty_resource_signature2_selection(const TreatyResourceSignatureSource2&,
 const TreatyRevisionView&,const TreatyAidResourceProposalSource*);
Status visit_treaty_aid_resource_proposal(CodecVisitor&,TreatyAidResourceProposalSource&);
Status emit_treaty_aid_resource_proposal_projection(ProjectionWriter&,std::string_view,const TreatyAidResourceProposalSource&);
Outcome<BoundaryValue> encode_treaty_aid_resource_proposal(const TreatyAidResourceProposalSource&);
Outcome<TreatyAidResourceProposalSource> decode_treaty_aid_resource_proposal(const BoundaryValue&);
Status visit_treaty_resource_signature2(CodecVisitor&,TreatyResourceSignatureSource2&);
Status emit_treaty_resource_signature2_projection(ProjectionWriter&,std::string_view,const TreatyResourceSignatureSource2&);
Outcome<BoundaryValue> encode_treaty_resource_signature2(const TreatyResourceSignatureSource2&);
Outcome<TreatyResourceSignatureSource2> decode_treaty_resource_signature2(const BoundaryValue&);
}
