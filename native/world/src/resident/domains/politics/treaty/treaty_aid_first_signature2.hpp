// Proposal2 is only an auxiliary freeze of the first actual ratification.
#pragma once
#include "treaty_resource_signature_write.hpp"
namespace hanmo::resident {
// The caller must discard the whole unpublished candidate on any failure.
// Later parties use the existing ratification command and actual disclosure.
Outcome<RatifyAndActivateTreatyResult> ratify_first_treaty_aid_selection_in_candidate2(
 CommonStore&,OrderedTable<TreatyAidResourceProposalSource2>&,
 OrderedTable<TreatyResourceSignatureSource2>&,AuthorityBasisRegistry&,
 OrderedTable<TreatyActivationSource>&,const TreatyResourceActivationContext&,
 TreatyActivationOriginalWrite&,const RatifyTreatyTermsCommand&,
 const std::vector<TreatyAidResourceSelection2>& explicit_selections);
}
