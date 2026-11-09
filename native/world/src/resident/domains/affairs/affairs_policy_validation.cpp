// 与原采购政策共用唯一金额边界；坏条款沿原错误码返回，不读取或修改世界。
#include "affairs_policy_validation.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
// 非负乘加必须先证明可表示；拒绝溢出而不截断合同上限。
bool add_ok(Integer a,Integer b,Integer& out){if(a<0||b<0||a>std::numeric_limits<Integer>::max()-b)return false;out=a+b;return true;}
bool mul_ok(Integer a,Integer b,Integer& out){if(a<0||b<0||(b&&a>std::numeric_limits<Integer>::max()/b))return false;out=a*b;return true;}
}
// 原policy_valid语义原样提取，模板存在和真实钱包仍由各实际调用者验证。
Status validate_need_policy(const NeedPolicy& p){if(p.id.empty()||(p.measure!=NeedMeasure::stock_gap&&p.measure!=NeedMeasure::cumulative_delivery)||p.source_domain!="local_economy"||p.source_key.empty()||p.semantic_key.empty()||p.organization_id.empty()||p.site_id.empty()||p.inventory_id.empty()||p.item_id.empty()||p.unit.empty()||p.template_id.empty()||p.funding_wallet_id.empty()||p.reward_wallet_id.empty()||p.terms_version.empty()||p.max_claim_quantity<=0||p.purchase_budget_per_unit<0||p.reward_per_unit<0||(p.reward_total&&(*p.reward_total<0||p.reward_per_unit!=0))||p.deadline_ticks<=0||p.accept_role.empty()||p.report_role.empty()||p.cancellation_policy_id!="return_actual_cargo_v1")return common_failure("InvalidNeedPolicy",p.id);Integer budget=0,reward=0,total=0;if(!mul_ok(p.max_claim_quantity,p.purchase_budget_per_unit,budget)||!mul_ok(p.max_claim_quantity,p.reward_per_unit,reward)||!add_ok(budget,p.reward_total.value_or(reward),total))return common_failure("NeedTermsOverflow",p.id);return common_success();}
// 校验先行，随后返回同一原政策的最大单合同本金、报酬与总承诺。
Outcome<NeedPolicyCommitment> quote_need_policy_commitment(const NeedPolicy& p){
    auto valid=validate_need_policy(p);if(!valid.ok)return {false,{},valid.error};
    NeedPolicyCommitment out;out.purchase_budget_cap=p.max_claim_quantity*p.purchase_budget_per_unit;
    out.reward_total=p.reward_total.value_or(p.max_claim_quantity*p.reward_per_unit);
    out.total=out.purchase_budget_cap+out.reward_total;return {true,out,{}};
}
}
