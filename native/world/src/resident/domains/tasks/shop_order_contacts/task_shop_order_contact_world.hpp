#pragma once
#include "task_shop_order_contact_profile.hpp"
#include "task_shop_order_contacts.hpp"
#include "task_shop_order_contact_receipts.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 同调用原纯订单read，避免Task完整源反入Shop全验；所有phase引用仅在本栈保活。
Status validate_task_shop_order_contact_sources_world(const WorldBundle&,const PrivateShopOrderRead&,const DynamicStepInput* phase=nullptr);
Status validate_task_shop_order_contact_world(const WorldBundle&);
Status record_task_shop_order_contact_world(WorldBundle&,const PrivateShopOrderRead&,const StableId& action_id,const TaskShopOrderDeliveryQuote&);
Outcome<StableId> task_shop_order_contact_new_result_world(const WorldBundle& before,const WorldBundle& after,const StableId& actor,const StableId& kind,const ReceiptDetails&);
Status bind_task_shop_order_contact_result_world(WorldBundle&,const StableId& source,const StableId& actor,Integer request,const CommitResult&);
}
