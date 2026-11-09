#pragma once
#include "treaty_authority_mapping.hpp"
#include <memory>
namespace hanmo::resident {
// The caller passes the selected scenario from the actual installed catalog,
// its prepared identity, and that same job's original terms catalog. An absent
// block is not an empty mapping or an authority success. No runtime Grant,
// actor, selected resource or inferred scope is an input to this loader.
Outcome<TreatyAuthorityMappingCatalog>
load_treaty_authority_mapping_from_scenario(
    const BoundaryValue &actual_scenario, const CatalogIdentity &,
    const PoliticalTreatyTermsCatalog &);
// Owns the validated catalog; it neither registers a new world/profile nor
// creates a second checkpoint job. The central prepared owner must retain this
// pointer and its same-job terms catalog together for all borrowed reads.
Outcome<std::shared_ptr<const TreatyAuthorityMappingCatalog>>
make_treaty_authority_mapping_from_scenario(
    const BoundaryValue &actual_scenario, const CatalogIdentity &,
    const PoliticalTreatyTermsCatalog &);
} // namespace hanmo::resident
