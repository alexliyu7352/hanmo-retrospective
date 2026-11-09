#pragma once
#include "task_shop_order_contact_state.hpp"
namespace hanmo::resident {
struct ReceiptDetails;
// 只原实际外层deliver命令；不得由旧裁剪cache或NPC空绑定推新开工来源。
Outcome<StableId> task_shop_order_contact_new_result_source(const TaskShopOrderContactState* before,const TaskShopOrderContactState* after,const StableId& actor,const StableId& kind,const ReceiptDetails&,Integer tick);
Status bind_task_shop_order_contact_result(CommonStore&,TaskShopOrderContactState&,const StableId& fact_id,const StableId& actor,Integer request_sequence,const CommitResult&);
Status validate_task_shop_order_contact_results(const CommonStore&,const TaskShopOrderContactState&);
}
