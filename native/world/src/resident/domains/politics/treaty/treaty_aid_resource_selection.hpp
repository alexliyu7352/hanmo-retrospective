// Explicit original selection payload. This is not an acceptance/authority proof.
// Quantities, beneficiary, delivery site and occurrence times belong to the
// immutable TreatyMaterialAidTerm; no independent copies are authored here.
#pragma once
#include "treaty_signature_sources.hpp"
#include "resident/domains/local_economy/material_aid/material_aid.hpp"
namespace hanmo::resident {
// Deliberately cannot carry an S5 binding ID, responsibility, acceptance time,
// or claimed paid/held quantity. Those are assigned by the actual S5 writer.
struct TreatyAidBudgetSelection {
 BudgetExecutionRef execution_ref;BudgetUnit unit;BudgetPeriod period;
 StableId payer_container_id;
};
struct TreatyAidResourceSelection {
 StableId term_id;
 std::vector<AidItemContainerRoute> item_routes;
 std::optional<AidMoneyContainerRoute> money_route;
 std::vector<TreatyAidBudgetSelection> budgets;
};
// Pure exact-term payload validation only. A successful result cannot establish
// consent, historical custody, a signature, an accepted schedule or a debt.
Status validate_treaty_aid_resource_selection_shape(const TreatyAidResourceSelection&);
Status validate_treaty_aid_resource_selection_for_term(
 const TreatyAidResourceSelection&,const TreatyTerm&);
Status visit_treaty_aid_resource_selection(CodecVisitor&,TreatyAidResourceSelection&);
Status emit_treaty_aid_resource_selection_projection(ProjectionWriter&,std::string_view,const TreatyAidResourceSelection&);
Outcome<BoundaryValue> encode_treaty_aid_resource_selection(const TreatyAidResourceSelection&);
Outcome<TreatyAidResourceSelection> decode_treaty_aid_resource_selection(const BoundaryValue&);
}
