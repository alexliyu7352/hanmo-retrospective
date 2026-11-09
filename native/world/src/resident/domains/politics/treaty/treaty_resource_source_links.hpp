// Non-recursive original fact links for the explicit signature/2 profile.
// This layer deliberately does not expose an accepted-resources/authority API.
#pragma once
#include "treaty_resource_signature2.hpp"
namespace hanmo::resident {
struct TreatyResourceSourceLinks {
 const TreatyRevisionContext& revisions;
 const OrderedTable<TreatyAidResourceProposalSource>& proposals;
 const OrderedTable<TreatyResourceSignatureSource2>& signatures;
};
Outcome<const TreatyAidResourceProposalSource*> read_treaty_aid_resource_proposal_links(
 const TreatyResourceSourceLinks&,const MomentOwnerSource&);
Outcome<const TreatyResourceSignatureSource2*> read_treaty_resource_signature2_links(
 const TreatyResourceSourceLinks&,const MomentOwnerSource&);
// Whole selected /2 table forward + reverse Common source scan. Full selected
// root validation additionally requires authority/knowledge/consent validation.
Status validate_treaty_resource_source_links(const TreatyResourceSourceLinks&);
}
