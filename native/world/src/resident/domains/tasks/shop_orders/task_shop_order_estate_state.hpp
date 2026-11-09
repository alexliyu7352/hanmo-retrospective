// 原店单的身后取消独立来源尾；旧task_shop_orders/1 visitor不编码此表。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/domains/commerce/liquidation/private_shop_liquidation_contract.hpp"
namespace hanmo::resident {
inline constexpr const char* task_shop_order_estate_module="task_shop_order_estate_cancellations";
inline constexpr const char* task_shop_order_estate_cancelled_kind="task_shop_order_estate_cancelled";
inline constexpr const char* task_shop_order_estate_termination="cancelled_by_estate_executor";
// 本片只支持玩家逐单执行；实际Common result后回绑，完整冷门拒未绑定来源。
struct TaskShopOrderEstatePlayerReceipt {StableId result_id;Integer request_sequence=0,result_revision=0;};
// 只绑定真实执行人/原委托/已收文书与唯一原结算；权源须由同World S5 before-settlement重新证明。
struct TaskShopOrderEstateCancellation {
    StableId fact_id,task_id,settlement_fact_id,settlement_id;
    PrivateShopEstateCancellationRequest request;
    Integer tick=0;
    std::optional<TaskShopOrderEstatePlayerReceipt> player_receipt;
};
struct TaskShopOrderEstateCancellationState {Integer version=1;OrderedTable<TaskShopOrderEstateCancellation> cancellations;};
Outcome<TaskShopOrderEstateCancellationState> decode_task_shop_order_estate_cancellations(const BoundaryValue&);
BoundaryValue encode_task_shop_order_estate_cancellations(const TaskShopOrderEstateCancellationState&);
Status visit_task_shop_order_estate_cancellations_checkpoint(CodecVisitor&,TaskShopOrderEstateCancellationState&);
Outcome<BoundaryValue> encode_task_shop_order_estate_fact(const TaskShopOrderEstateCancellationState&,const StableId&);
}
