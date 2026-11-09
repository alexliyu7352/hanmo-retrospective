#pragma once
#include "political_budget_policy.hpp"
namespace hanmo::resident {
Status visit_public_budget_policy_catalog(CodecVisitor &,
                                          PublicBudgetPolicyCatalog &);
Status emit_public_budget_policy_catalog_projection(
    ProjectionWriter &, std::string_view, const PublicBudgetPolicyCatalog &);
Outcome<BoundaryValue>
encode_public_budget_policy_catalog(const PublicBudgetPolicyCatalog &);
// Caller holds this exact catalog's same-job finite JSON scope; identity is the
// final original PreparedCatalog identity, not synthesized from author fields.
Outcome<PublicBudgetPolicyCatalog>
decode_public_budget_policy_catalog(const BoundaryValue &,
                                    const CatalogIdentity &);
} // namespace hanmo::resident
