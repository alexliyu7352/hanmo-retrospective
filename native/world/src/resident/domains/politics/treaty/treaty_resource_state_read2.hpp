#pragma once
#include "treaty_aid_accepted_resource_read2.hpp"
#include "treaty_aid_term_authority.hpp"
#include "treaty_resource_release.hpp"
#include "treaty_terms_state.hpp"
namespace hanmo::resident {
// 只核同一物理拥有体/作者身份，不调用权源或履约；原公产 lease 可用此口防回环。
Status validate_political_treaty_resource_body_bindings2(
    const PoliticalTreatyTermsState &, const TreatyResourceSourceLinks2 &,
    const PoliticalTreatyTermsCatalog &actual_catalog,
    const CatalogIdentity &actual_identity);
struct PoliticalTreatyResourceReadContext2 {
  const PoliticalTreatyTermsState &state;
  const PoliticalTreatyTermsCatalog &catalog;
  const CatalogIdentity &identity;
  const TreatyAuthorityMappingCatalog &mapping;
  const NativeTreatyAidTermAuthorityRead &term_authorities;
  const TreatyMandateContext &mandates;
  const TreatyRecognitionContext &recognition;
  const TreatyResourceSignatureReadContext &signatures;
  const TreatyResourceActivationContext &activations;
  const TreatyResourceTerminationContext &terminations;
  const TreatyResourceReleaseContext &releases;
  const TreatyAidRecipientAcceptanceContext &recipients;
  const TreatyAidAcceptedResourceContext2 &accepted;
};
// 每个真实源组全部调用，不能用空表或只验codec替代完整原owner准入。
Status validate_political_treaty_resources_state2(
    const PoliticalTreatyResourceReadContext2 &);

} // namespace hanmo::resident
