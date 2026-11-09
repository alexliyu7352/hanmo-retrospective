#include "treaty_aid_resource_selection2.hpp"
namespace hanmo::resident {
Status validate_treaty_aid_resource_selection2_shape(const TreatyAidResourceSelection2&r){
 auto valid=validate_treaty_aid_resource_selection_shape(r.aid);if(!valid.ok)return valid;
 if(r.compensation){const auto&c=*r.compensation;const auto&b=c.budget;const auto&e=b.execution_ref;
  if(c.money_route.source_wallet_id.empty()||c.money_route.beneficiary_wallet_id.empty()||c.money_route.source_wallet_id==c.money_route.beneficiary_wallet_id||!std::holds_alternative<BudgetMoneyQian>(b.unit)||b.payer_container_id!=c.money_route.source_wallet_id||e.budget_id.empty()||e.budget_revision<1||e.policy_revision<0||e.approval_fact_id.empty()||b.period.period_from_tick<0||b.period.period_until_tick<=b.period.period_from_tick)return common_failure("InvalidTreatyAidCompensationSelection",r.aid.term_id);
 }return common_success();
}
Status validate_treaty_aid_resource_selection2_for_term(const TreatyAidResourceSelection2&r,const TreatyTerm&term){
 auto valid=validate_treaty_aid_resource_selection2_shape(r);if(!valid.ok)return valid;
 valid=validate_treaty_aid_resource_selection_for_term(r.aid,term);if(!valid.ok)return valid;
 const auto&aid=std::get<TreatyMaterialAidTerm>(term.terms);
 if(aid.money&&aid.money->currency_id!="qian")return common_failure("MaterialAidCurrencyRequired",aid.money->currency_id,ErrorKind::unsupported_capability);
 const auto*comp=std::get_if<TreatyBoundedCompensation>(&aid.late_policy);
 if(bool(comp)!=bool(r.compensation))return common_failure("TreatyAidCompensationExplicitSelectionRequired",term.term_id);
 if(comp){if(comp->currency_id!="qian")return common_failure("MaterialAidCompensationCurrencyRequired",comp->currency_id,ErrorKind::unsupported_capability);if(comp->currency_id.empty()||comp->obligation_owner.empty()||comp->maximum_amount<0)return common_failure("InvalidTreatyCompensationCap",term.term_id);
  const auto&b=r.compensation->budget.execution_ref;const auto&original=aid.budget_ref;
  if(b.budget_id!=original.authorization_id||b.budget_revision!=original.authorization_revision||b.approval_fact_id!=original.approval_source.id)return common_failure("TreatyAidCompensationOriginalBudgetMismatch",term.term_id);
 }
 return common_success();
}
}
