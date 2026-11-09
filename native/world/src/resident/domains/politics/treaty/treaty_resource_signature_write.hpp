// Original direct Office ratification, with explicit selected proposal/2.
#pragma once
#include "treaty_resource_activation.hpp"
namespace hanmo::resident {
struct RatifyTreatyResourceTermsCommand2 {
 RatifyTreatyTermsCommand ratification;
 std::optional<MomentOwnerSource> actual_aid_resource_proposal;
 std::optional<TreatyResourceKnowledge> resource_knowledge;
};
// The actual author/term owner must bind the hot requirements to this explicit
// immutable proposal. After append the same requests are re-derived through
// the actual signature cut; a different hot proposal cannot survive cold read.
Outcome<SourceRef> ratify_treaty_resource_terms_in_candidate(CommonStore&,
 OrderedTable<TreatyResourceSignatureSource2>&,AuthorityBasisRegistry&,
 const TreatyResourceSignatureReadContext&,const RatifyTreatyResourceTermsCommand2&);
Outcome<RatifyAndActivateTreatyResult> ratify_and_activate_treaty_resource_in_candidate(
 CommonStore&,OrderedTable<TreatyResourceSignatureSource2>&,AuthorityBasisRegistry&,
 OrderedTable<TreatyActivationSource>&,const TreatyResourceActivationContext&,
 TreatyActivationOriginalWrite&,const RatifyTreatyResourceTermsCommand2&);
Status validate_ratify_treaty_resource_command2_shape(const RatifyTreatyResourceTermsCommand2&);
Outcome<BoundaryValue> encode_ratify_treaty_resource_command2(const RatifyTreatyResourceTermsCommand2&);
Outcome<RatifyTreatyResourceTermsCommand2> decode_ratify_treaty_resource_command2(const BoundaryValue&);

}
