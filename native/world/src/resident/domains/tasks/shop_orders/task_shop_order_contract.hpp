// 私店订单接受来源只读Task自身冻结账，不回调完整店铺/客户资格；同候选绑定后再做两域完整冷验。
#pragma once
#include "resident/domains/commerce/private_shop_order_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* task_shop_order_acceptance_prefix="task_shop_order_accepted";
inline constexpr const char* task_shop_order_acceptance_kind="task_shop_order_accepted";
inline constexpr const char* task_shop_order_domain="task_shop_orders";
struct TaskShopOrderAcceptanceSource {
    WorldIdentity source_identity;
    StableId task_id,acceptance_fact_id,order_issue_fact_id;
    PrivateShopOrderTerms terms;
};
class TaskShopOrderAcceptanceRead {
public:
    virtual ~TaskShopOrderAcceptanceRead()=default;
    // 仅按原order ID取本域真实接受；空表示尚未接受，损坏来源必须error。
    virtual Outcome<std::optional<TaskShopOrderAcceptanceSource>> task_shop_order_acceptance(const StableId& order_id)const=0;
};
}
