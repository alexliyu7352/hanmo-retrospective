// S5只消费原完整Task接触先验后的具名执行来源；不反调Shop或成本全验。
#pragma once
#include "task_shop_order_contact_state.hpp"
namespace hanmo::resident {
struct TaskShopOrderDeliveryContactSource {
 WorldIdentity source_identity;
 TaskShopOrderContactStart contact;
 StableId settlement_fact_id,settlement_id;
 Integer completed_tick=0;
};
class TaskShopOrderContactRead {
public:
 virtual ~TaskShopOrderContactRead()=default;
 // 精确原Task及成功凭据；不返回另一单或最新一份接触，不把未交/取消当履约。
 virtual Outcome<std::optional<TaskShopOrderDeliveryContactSource>> task_shop_order_delivery_contact(const StableId& task_id,const StableId& exact_settlement_fact_id)const=0;
};
}
