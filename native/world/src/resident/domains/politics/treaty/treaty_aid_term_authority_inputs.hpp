#pragma once
#include "treaty_authority_mapping.hpp"
#include "treaty_resource_source_links2.hpp"
#include "treaty_ratification_authority.hpp"
namespace hanmo::resident {
// Original inputs, not an authority grant or accepted-resources view. The
// concrete owner must additionally prove every primary resource's source-party
// title at this exact current/signature boundary before returning requirements.
struct TreatyAidTermAuthorityInputs {
  TreatyRatificationAuthorityRequirements requirements;
  const TreatyAidResourceProposalSource2 *proposal = nullptr;
  const TreatyResourceSignatureSource2 *actual_signature = nullptr;
  WorldMoment authority_at;
};
Outcome<TreatyAidTermAuthorityInputs> read_treaty_aid_term_authority_inputs(
    const TreatyResourceSourceLinks2 &, const TreatyAuthorityMappingCatalog &,
    const TreatyRevisionView &, const StableId &party, const ActingAuthority &,
    const SourceRef *actual_signature_boundary);
} // namespace hanmo::resident
