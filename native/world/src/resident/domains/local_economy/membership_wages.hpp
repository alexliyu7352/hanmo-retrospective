// 任职工资来源与一次终态余数补偿；复用唯一RecurringObligation、欠期及支付表。
#pragma once
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
struct OrganizationState; struct OrganizationCatalog; struct LocalEconomyState; struct LocalEconomyCatalog; struct MembershipTerms;
// id同时作为动态机构付款authority_ref；来源终身绑定原雇主和原episode。
struct MembershipWageBinding {
    StableId id,member_episode_id,obligation_id,offer_id,decision_id,terms_id,organization_id,actor_id;
    StableId payer_wallet_id,payee_wallet_id,grant_id,office_id,issuer_actor_id;
    Integer authority_epoch=0,joined_tick=0,first_due_tick=0,credited_periods=0,last_credited_due_tick=0;
    Integer frozen_numerator=0,frozen_denominator=1,period_ticks=1,amount_limit=0;
    StableId terminal_fraction_policy_id; std::optional<Integer> ended_tick; StableId terminal_fact_id;
};
// 原余数r/d与补偿(d-r)/d永久保存，整数1钱应付只进入原欠期账本一次。
struct MembershipTerminalFractionReceipt {
    StableId id,wage_binding_id,member_episode_id,obligation_id,departure_fact_id,arrear_id;
    Integer tick=0,original_numerator=0,denominator=1,compensation_numerator=0,integer_due=0;
};
struct MembershipWageEffects { StableId binding_id,obligation_id; std::vector<StableId> fact_ids,receipt_ids; };
// 预览只核实原钱包及首期整数支付能力，不分配ID或冻结资金。
Outcome<Integer> preview_membership_wage(const CommonStore&,const LocalEconomyCatalog&,const MembershipTerms&,const StableId& actor_id,Integer joined_tick);
// 此内部入口只接受已经加入真实成员表的接受来源；共同事实回滚由外层命令保存点负责，经济表仅成功替换。
Outcome<MembershipWageEffects> register_membership_wage(CommonStore&,LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationState&,const OrganizationCatalog&,const StableId& member_episode_id);
// 结束未来义务并把已计提正余数一次折成1钱原雇主应付；无有效账期不补偿。
Outcome<MembershipWageEffects> finish_membership_wage(CommonStore&,LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationState&,const OrganizationCatalog&,const StableId& member_episode_id,Integer tick,const StableId& departure_fact_id,const CurrentMomentRead* actual_clock=nullptr);
// 即使成员已离开也只沿原接受来源授权旧债；不会借新episode或当前grant换付款人。
bool membership_wage_authorized(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const StableId& authority_ref,const StableId& obligation_id,const StableId& principal,const StableId& source,Integer amount,Integer tick);
// 发生应付/历史接受来源校验：保原条款、额度与时效，只不把今天的钱包Owner当旧接受来源。
// 这不是转账授权；真正付款仍走当前Owner/实际承接决定。
bool membership_wage_source_authorized(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const LocalEconomyState&,const StableId& authority_ref,const StableId& obligation_id,const StableId& principal,const StableId& source,Integer amount,Integer tick);
// 持久验证逐项重算服务期、余数、终态补偿及原始付款绑定。
Status validate_membership_wages(const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,const OrganizationState&,const OrganizationCatalog&,const CurrentMomentRead* actual_clock=nullptr);
} // 终态不足是普通整数欠薪，不能据此扣住人物。
