// 原政策最大本金/报酬边界导出门；不拿纯数学校验替代真实公权和钱包授权。
#include "resident/domains/affairs/affairs_policy_validation.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hanmo::resident;
namespace {
int checks=0;
// 首个失败保具名诊断，避免后续整数运算掩盖真实边界。
void need(bool good,const char* detail){++checks;if(!good)throw std::runtime_error(detail);}
// 只声明原有完整政策条款，不创建人物、公库或经济来源。
NeedPolicy fixture(){NeedPolicy p;p.id="policy";p.source_key="supply";p.semantic_key="grain";p.organization_id="org";p.site_id="site";p.inventory_id="depot";p.item_id="rice";p.template_id="procurement";p.funding_wallet_id="cash";p.reward_wallet_id="cash";p.max_claim_quantity=5;p.purchase_budget_per_unit=8;p.reward_per_unit=3;p.deadline_ticks=10;p.accept_role="clerk";p.report_role="clerk";return p;}
// 校验与报价必须同码同详情拒绝，不能由报价函数放宽作者边界。
void rejects(const NeedPolicy& p,const char* code){auto valid=validate_need_policy(p);auto quote=quote_need_policy_commitment(p);need(!valid.ok&&!quote.ok&&valid.error.original.code==code&&quote.error.original.code==code&&quote.error.original.path==p.id,"validator and cost quote reject equally");}
}
// 最大承诺的零额、两种报酬、整数边缘与溢出均沿原规则验证。
int main(){try{
 const auto max=std::numeric_limits<Integer>::max();auto p=fixture();auto q=quote_need_policy_commitment(p);need(validate_need_policy(p).ok&&q.ok&&q.value.purchase_budget_cap==40&&q.value.reward_total==15&&q.value.total==55,"per unit maximum");
 p.reward_per_unit=0;p.reward_total=7;q=quote_need_policy_commitment(p);need(q.ok&&q.value.reward_total==7&&q.value.total==47,"fixed total reward not multiplied");
 p.purchase_budget_per_unit=0;p.reward_total=0;q=quote_need_policy_commitment(p);need(q.ok&&q.value.total==0,"real zero commitment permitted");
 p=fixture();p.max_claim_quantity=1;p.purchase_budget_per_unit=max;p.reward_per_unit=0;q=quote_need_policy_commitment(p);need(q.ok&&q.value.total==max,"maximum representable principal");
 p.max_claim_quantity=2;rejects(p,"NeedTermsOverflow");p=fixture();p.max_claim_quantity=max;rejects(p,"NeedTermsOverflow");
 p=fixture();p.max_claim_quantity=1;p.purchase_budget_per_unit=max;p.reward_per_unit=1;rejects(p,"NeedTermsOverflow");
 p.reward_per_unit=0;p.reward_total=1;rejects(p,"NeedTermsOverflow");p=fixture();p.reward_total=0;rejects(p,"InvalidNeedPolicy");
 p=fixture();p.source_domain="invented";rejects(p,"InvalidNeedPolicy");p=fixture();p.max_claim_quantity=0;rejects(p,"InvalidNeedPolicy");
 p=fixture();p.reward_per_unit=-1;rejects(p,"InvalidNeedPolicy");p=fixture();p.measure=NeedMeasure::outstanding_obligation;rejects(p,"InvalidNeedPolicy");
 p=fixture();p.measure=NeedMeasure::cumulative_delivery;need(validate_need_policy(p).ok,"original cumulative delivery supported");
 p=fixture();p.cancellation_policy_id="erase";rejects(p,"InvalidNeedPolicy");p=fixture();p.reward_per_unit=0;p.reward_total=-1;rejects(p,"InvalidNeedPolicy");
 std::cout<<checks<<" need policy commitment gates, 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
