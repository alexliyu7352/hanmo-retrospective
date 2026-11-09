// 店员持独立双方雇约，服务来源必须为真实私铺营业Action，不能把庄园工时改名套入。
#pragma once
#include "private_shop_contract.hpp"
namespace hanmo::resident {
struct PrivateShopStaffPolicy {
    StableId id,premises_id,worker_actor_id,wage_rate_profile;
    StableId employer_dialogue_id,employer_offer_node_id,worker_dialogue_id,worker_accept_node_id;
    Integer service_ticks=60;StableId service_item_id;
};
// 雇主请求真实店员本人say接受，或受雇本人接受原雇主say邀约；请求人不能代另一方决定。
struct PrivateShopEmploy {StableId shop_id,policy_id;};
struct PrivateShopStaffAccept {StableId shop_id,policy_id;};
struct PrivateShopStaffDecision {StableId contract_id,decision;};
using PrivateShopStaffCommand=std::variant<PrivateShopEmploy,PrivateShopStaffAccept,PrivateShopStaffDecision>;
struct PrivateShopStaffTerms {
    StableId policy_id,shop_id,employer_actor_id,worker_actor_id,payer_wallet_id,payee_wallet_id,rate_profile;
    Integer wage_numerator=3,wage_denominator=2,service_ticks=60;StableId service_item_id;
};
struct PrivateShopStaffSource {
    WorldIdentity source_identity;PrivateShopStaffTerms terms;
    StableId contract_id,status,acceptance_fact_id,terminal_fact_id,obligation_id;
    Integer accepted_tick=0,through_tick=0,served_ticks=0,earned_cash=0,paid_cash=0,reserved_cash=0;
};
struct PrivateShopStaffContract {
    StableId id,status="active",terminal_fact_id,obligation_id;
    PrivateShopStaffTerms terms;PrivateShopMeetingSource agreement;
    Integer accepted_tick=0,through_tick=0;bool accepted_by_worker=false;
};
struct PrivateShopStaffTermination {StableId id,contract_id,actor_id,decision;Integer tick=0;StableId cause_fact_id;};
// 每个工资事实只认原营业Action的真实到岗边界；中断只向上结清已赚分数，不支付未到岗时间。
struct PrivateShopStaffService {
    StableId id,contract_id,operation_id,arrear_id,payment_id;
    Integer tick=0,attendance_index=0,amount=0,remainder_before=0,remainder_after=0;bool closing_fraction=false;
};
struct PrivateShopStaffState {OrderedTable<PrivateShopStaffContract> contracts;OrderedTable<PrivateShopStaffTermination> decisions;OrderedTable<PrivateShopStaffService> services;};
struct PrivateShopStaffCatalog {OrderedTable<PrivateShopStaffPolicy> policies;};
class PrivateShopStaffRead {
public:
    virtual ~PrivateShopStaffRead()=default;
    // 规则用纯本账来源；不反调完整Shop客户/市场/Task/政治资格，防劳务交叉冷验递归。
    virtual Outcome<std::optional<PrivateShopStaffSource>> private_shop_staff_source(const StableId& contract_id)const=0;
    virtual Outcome<std::vector<StableId>> private_shop_worker_contracts(const StableId& worker_actor_id)const=0;
};
}
