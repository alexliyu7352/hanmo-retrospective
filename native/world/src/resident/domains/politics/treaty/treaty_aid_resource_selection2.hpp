#pragma once
#include "treaty_aid_resource_selection.hpp"
namespace hanmo::resident {
// A separate original liability, still the exact term's material_aid budget.
// No copied maximum or current balance; amount comes only from the frozen term.
struct TreatyAidCompensationSelection {
 AidMoneyContainerRoute money_route;
 TreatyAidBudgetSelection budget;
};
struct TreatyAidResourceSelection2 {
 TreatyAidResourceSelection aid;
 std::optional<TreatyAidCompensationSelection> compensation;
};
Status validate_treaty_aid_resource_selection2_shape(const TreatyAidResourceSelection2&);
Status validate_treaty_aid_resource_selection2_for_term(const TreatyAidResourceSelection2&,const TreatyTerm&);
Status visit_treaty_aid_resource_selection2(CodecVisitor&,TreatyAidResourceSelection2&);
Status emit_treaty_aid_resource_selection2_projection(ProjectionWriter&,std::string_view,const TreatyAidResourceSelection2&);
Outcome<BoundaryValue> encode_treaty_aid_resource_selection2(const TreatyAidResourceSelection2&);
Outcome<TreatyAidResourceSelection2> decode_treaty_aid_resource_selection2(const BoundaryValue&);
}
