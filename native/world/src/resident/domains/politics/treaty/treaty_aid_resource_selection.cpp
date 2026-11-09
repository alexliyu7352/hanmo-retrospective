#include "treaty_aid_resource_selection.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
bool same_unit(const BudgetUnit&a,const BudgetUnit&b){if(a.index()!=b.index())return false;const auto*item=std::get_if<BudgetItemQuantity>(&a);return !item||item->item_id==std::get<BudgetItemQuantity>(b).item_id;}
}
Status validate_treaty_aid_resource_selection_shape(const TreatyAidResourceSelection&r){
 if(r.term_id.empty()||(r.item_routes.empty()&&!r.money_route))return common_failure("InvalidTreatyAidResourceSelection",r.term_id);
 for(std::size_t n=0;n<r.item_routes.size();++n){const auto&route=r.item_routes[n];if(route.item_id.empty()||route.source_inventory_id.empty()||route.beneficiary_inventory_id.empty())return common_failure("TreatyAidExplicitInventoryRouteRequired",r.term_id);for(std::size_t i=0;i<n;++i)if(r.item_routes[i].item_id==route.item_id)return common_failure("DuplicateTreatyAidSelectedItem",route.item_id);}
 if(r.money_route&&(r.money_route->source_wallet_id.empty()||r.money_route->beneficiary_wallet_id.empty()||r.money_route->source_wallet_id==r.money_route->beneficiary_wallet_id))return common_failure("TreatyAidExplicitWalletRouteRequired",r.term_id);
 if(r.budgets.size()!=r.item_routes.size()+bool(r.money_route))return common_failure("TreatyAidSelectedBudgetCoverageMismatch",r.term_id);
 for(std::size_t n=0;n<r.budgets.size();++n){const auto&b=r.budgets[n];const auto&e=b.execution_ref;
  if(e.budget_id.empty()||e.budget_revision<1||e.policy_revision<0||e.approval_fact_id.empty()||b.period.period_from_tick<0||b.period.period_until_tick<=b.period.period_from_tick||b.payer_container_id.empty())return common_failure("InvalidTreatyAidSelectedBudget",r.term_id);
  for(std::size_t i=0;i<n;++i)if(same_unit(r.budgets[i].unit,b.unit))return common_failure("DuplicateTreatyAidSelectedBudgetUnit",r.term_id);
  if(const auto*item=std::get_if<BudgetItemQuantity>(&b.unit)){const auto route=std::find_if(r.item_routes.begin(),r.item_routes.end(),[&](const auto&v){return v.item_id==item->item_id;});if(route==r.item_routes.end()||route->source_inventory_id!=b.payer_container_id)return common_failure("TreatyAidSelectedBudgetInventoryMismatch",r.term_id);}
  else if(!r.money_route||r.money_route->source_wallet_id!=b.payer_container_id)return common_failure("TreatyAidSelectedBudgetWalletMismatch",r.term_id);
 }
 return common_success();
}
Status validate_treaty_aid_resource_selection_for_term(const TreatyAidResourceSelection&r,const TreatyTerm&term){
 auto valid=validate_treaty_aid_resource_selection_shape(r);if(!valid.ok)return valid;
 const auto*aid=std::get_if<TreatyMaterialAidTerm>(&term.terms);if(!aid||r.term_id!=term.term_id||bool(r.money_route)!=bool(aid->money))return common_failure("TreatyAidSelectedTermMismatch",r.term_id);
 std::vector<StableId> items;for(const auto&item:aid->items){if(item.item_id.empty()||item.quantity<=0)return common_failure("InvalidTreatyAidOriginalItem",r.term_id);if(std::find(items.begin(),items.end(),item.item_id)==items.end())items.push_back(item.item_id);}
 if(items.size()!=r.item_routes.size())return common_failure("TreatyAidSelectedItemCoverageMismatch",r.term_id);
 for(const auto&route:r.item_routes)if(std::find(items.begin(),items.end(),route.item_id)==items.end())return common_failure("TreatyAidUnpromisedSelectedItem",route.item_id);
 for(const auto&b:r.budgets)if(b.execution_ref.budget_id!=aid->budget_ref.authorization_id||b.execution_ref.budget_revision!=aid->budget_ref.authorization_revision||b.execution_ref.approval_fact_id!=aid->budget_ref.approval_source.id)return common_failure("TreatyAidSelectedBudgetOriginalMismatch",r.term_id);
 return common_success();
}
}
