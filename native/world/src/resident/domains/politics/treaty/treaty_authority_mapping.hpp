// Author-owned finite term mapping. Never inferred from a selected Grant.
#pragma once
#include "treaty_revision_sources.hpp"
namespace hanmo::resident {
struct TreatyAuthoredTermAuthority {
 StableId term_id;TreatyTermKind term_kind=TreatyTermKind::truce;
 StableId command_kind;ScopeRef scope;
};
struct TreatyAuthoredPartyAuthority {
 StableId party_id;ScopeRef ratification_scope;
 std::vector<TreatyAuthoredTermAuthority> terms;
};
struct TreatyAuthoredAuthorityDefinition {
 StableId terms_definition_id;
 std::vector<TreatyAuthoredPartyAuthority> parties;
};
// Identity is supplied by the original prepared author job, never decoded from
// a caller's command. No monetary exposure, selected Grant or current owner is
// stored here; those remain real original-source reads at the signature cut.
struct TreatyAuthorityMappingCatalog {
 CatalogIdentity identity;
 std::vector<TreatyAuthoredAuthorityDefinition> definitions;
};
Status validate_treaty_authority_mapping_shape(const TreatyAuthorityMappingCatalog&);
Status validate_treaty_authority_mapping_catalog(const TreatyAuthorityMappingCatalog&,const PoliticalTreatyTermsCatalog&);
// Resolves exact authored fields only, not authority success. Pointer identity
// pins the revision definition to the same immutable prepared author catalog.
Outcome<const TreatyAuthoredPartyAuthority*> read_treaty_authored_authority_mapping(
 const TreatyAuthorityMappingCatalog&,const PoliticalTreatyTermsCatalog&,
 const TreatyRevisionView&,const StableId& party);
Status visit_treaty_authority_mapping(CodecVisitor&,TreatyAuthorityMappingCatalog&);
Status emit_treaty_authority_mapping_projection(ProjectionWriter&,std::string_view,const TreatyAuthorityMappingCatalog&);
Outcome<BoundaryValue> encode_treaty_authority_mapping(const TreatyAuthorityMappingCatalog&);
Outcome<TreatyAuthorityMappingCatalog> decode_treaty_authority_mapping(const BoundaryValue&,const CatalogIdentity&,const PoliticalTreatyTermsCatalog&);
}
