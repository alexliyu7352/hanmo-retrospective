#pragma once
#include "treaty_aid_resource_selection2.hpp"
#include "treaty_aid_resource_selection_fields.hpp"
namespace hanmo::resident::treaty_aid_selection2_fields {
template<class C>Status fields(C&c,TreatyAidResourceSelection2&r){
 Integer schema=2;c.field("schema_version",schema);if(!c.status().ok)return c.status();if(schema!=2)return c.reject("UnsupportedTreatyAidResourceSelectionSchema");
 c.object("aid",[&](auto&v){return treaty_aid_selection_fields::fields(v,r.aid);});
 c.optional("compensation",r.compensation,[](auto&v,auto&x){
  v.object("money_route",[&](auto&w){w.field("source_wallet_id",x.money_route.source_wallet_id);return w.field("beneficiary_wallet_id",x.money_route.beneficiary_wallet_id);});
  return v.object("budget",[&](auto&w){auto&b=x.budget;
   w.object("execution_ref",[&](auto&a){a.field("budget_id",b.execution_ref.budget_id);a.field("budget_revision",b.execution_ref.budget_revision);a.field("policy_revision",b.execution_ref.policy_revision);return a.field("approval_fact_id",b.execution_ref.approval_fact_id);});
   w.object("unit",[&](auto&a){StableId kind="money_qian";if(!a.reading()&&!std::holds_alternative<BudgetMoneyQian>(b.unit))return a.reject("InvalidTreatyAidCompensationUnit");a.field("kind",kind);if(kind!="money_qian")return a.reject("InvalidTreatyAidCompensationUnit");if(a.reading())b.unit=BudgetMoneyQian{};return a.status();});
   w.object("period",[&](auto&a){a.field("period_from_tick",b.period.period_from_tick);return a.field("period_until_tick",b.period.period_until_tick);});
   return w.field("payer_container_id",b.payer_container_id);
  });
 });
 if(c.status().ok){auto valid=validate_treaty_aid_resource_selection2_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}return c.status();
}
}
