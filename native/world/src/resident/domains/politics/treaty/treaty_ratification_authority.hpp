// P15.3: one actual signing source and one original AuthorityBasis. Every
// selected term is still checked against that same historical Grant.
#pragma once
#include "treaty_terms_contract.hpp"
#include "resident/domains/organization/authority_registry.hpp"
namespace hanmo::resident {
struct TreatyTermAuthorityRequirement {
 StableId term_id;TreatyTermKind term_kind=TreatyTermKind::truce;
 SingleGrantAuthorityRequest request;
};
struct TreatyRatificationAuthorityRequirements {
 const TreatyTermsV1* original_terms=nullptr;
 // The original rule/financial acceptance owner computes the complete exposure
// by unit, with all actual resource selections. No cap is used as a price.
 SingleGrantAuthorityRequest primary;
 std::vector<TreatyTermAuthorityRequirement> term_requirements;
};
struct TreatyRatificationOriginalView {
 SourceRef signature_source,terms_source;Integer terms_revision=0;StableId party_id;
 ActingAuthority acting;WorldMoment occurred_at;AuthorityBasisRef authority_basis;
};
class TreatyRatificationOriginalRead:public AuthorityIssueSourceRead {public:
 // Reads the original immutable offer/counteroffer revision and its compiled
// term-specific authority requirements. A cold boundary is the actual signature
// source; implementations must use original control/resources before that cut.
// This is not a command parser and cannot accept caller-supplied requirements.
 virtual Outcome<TreatyRatificationAuthorityRequirements> read_original_ratification_requirements(
  const SourceRef& terms_source,Integer revision,const StableId& party,
  const ActingAuthority&,const SourceRef* actual_signature_boundary)const=0;
 // Reads the real signed row and its actual knowledge/mandate/content source.
// It must not call the AuthorityBasis reader back recursively.
 virtual Outcome<TreatyRatificationOriginalView> read_actual_treaty_ratification(const SourceRef&)const=0;
};
struct TreatyRatificationAuthorityContext {
 const CommonStore& common;const OrganizationState& organization;
 const OrganizationCatalog& organization_catalog;const CatalogIdentity& identity;
 const AuthorityBasisRegistry& authority_bases;
 const TreatyRatificationOriginalRead& originals;
 const AuthorityBudgetRead* budgets=nullptr;
};
struct TreatyRatificationAuthorityCapture {
 SingleGrantAuthorityRequest primary;
 std::vector<SingleGrantAuthorityRequest> term_requirements;
 SingleGrantAuthorityRead authority;
};
Outcome<TreatyRatificationAuthorityCapture> capture_treaty_ratification_authority(
 const TreatyRatificationAuthorityContext&,const SourceRef& terms_source,Integer revision,
 const StableId& party,const ActingAuthority&);
Status validate_treaty_ratification_authority_capture(const TreatyRatificationAuthorityContext&,
 const SourceRef& terms_source,Integer revision,const StableId& party,const ActingAuthority&,
 const TreatyRatificationAuthorityCapture&);
Outcome<const AuthorityBasis*> read_treaty_ratification_authority(
 const TreatyRatificationAuthorityContext&,const SourceRef& actual_signature);
}
