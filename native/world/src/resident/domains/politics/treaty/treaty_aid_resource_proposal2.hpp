#pragma once
#include "treaty_resource_signature2.hpp"
#include "treaty_aid_resource_selection2.hpp"
namespace hanmo::resident {
// Explicit proposal/2 carries the bounded-compensation choice as well as the
// ordinary Aid routes. No mutation or silent reinterpretation of proposal/1.
struct TreatyAidResourceProposalSource2 {
 StableId id,treaty_id;Integer terms_revision=0;SourceRef terms_source;
 ActingAuthority proposer;WorldMoment proposed_at;
 std::vector<TreatyAidResourceSelection2> selections;
};
SourceRef treaty_aid_resource_proposal2_source(const TreatyAidResourceProposalSource2&);
Status validate_treaty_aid_resource_proposal2_shape(const TreatyAidResourceProposalSource2&);
Status validate_treaty_aid_resource_proposal2_terms(const TreatyAidResourceProposalSource2&,const TreatyRevisionView&);
Status validate_treaty_resource_signature2_selection(const TreatyResourceSignatureSource2&,const TreatyRevisionView&,const TreatyAidResourceProposalSource2*);
Status visit_treaty_aid_resource_proposal2(CodecVisitor&,TreatyAidResourceProposalSource2&);
Status emit_treaty_aid_resource_proposal2_projection(ProjectionWriter&,std::string_view,const TreatyAidResourceProposalSource2&);
Outcome<BoundaryValue> encode_treaty_aid_resource_proposal2(const TreatyAidResourceProposalSource2&);
Outcome<TreatyAidResourceProposalSource2> decode_treaty_aid_resource_proposal2(const BoundaryValue&);
}
