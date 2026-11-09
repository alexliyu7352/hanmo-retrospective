#pragma once
#include "treaty_aid_resource_selection.hpp"
#include "treaty_signature_codec_fields.hpp"
namespace hanmo::resident::treaty_aid_selection_fields {
template<class C>Status fields(C&c,TreatyAidResourceSelection&r){
 c.field("term_id",r.term_id);
 c.sequence("item_routes",r.item_routes,[](auto&v,auto&route){v.field("item_id",route.item_id);v.field("source_inventory_id",route.source_inventory_id);return v.field("beneficiary_inventory_id",route.beneficiary_inventory_id);});
 c.optional("money_route",r.money_route,[](auto&v,auto&route){v.field("source_wallet_id",route.source_wallet_id);return v.field("beneficiary_wallet_id",route.beneficiary_wallet_id);});
 c.sequence("budgets",r.budgets,[](auto&v,auto&b){
  v.object("execution_ref",[&](auto&w){w.field("budget_id",b.execution_ref.budget_id);w.field("budget_revision",b.execution_ref.budget_revision);w.field("policy_revision",b.execution_ref.policy_revision);return w.field("approval_fact_id",b.execution_ref.approval_fact_id);});
  v.object("unit",[&](auto&w){static constexpr const char*names[]={"money_qian","item_quantity"};std::size_t tag=0;auto valid=treaty_terms_codec_fields::variant_tag(w,b.unit,names,tag);if(!valid.ok)return valid;if(tag==0){if(w.reading())b.unit=BudgetMoneyQian{};return w.status();}if(w.reading())b.unit=BudgetItemQuantity{};return w.field("item_id",std::get<BudgetItemQuantity>(b.unit).item_id);});
  v.object("period",[&](auto&w){w.field("period_from_tick",b.period.period_from_tick);return w.field("period_until_tick",b.period.period_until_tick);});
  return v.field("payer_container_id",b.payer_container_id);
 });
 if(c.status().ok){auto valid=validate_treaty_aid_resource_selection_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}
