#pragma once
#include "task_shop_order_contact_state.hpp"
namespace hanmo::resident {
Outcome<bool> decode_task_shop_order_contact_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
// 真实独立段，三个旧来源版本必须已选择；标题和完整binary选择共用此口。
Outcome<Integer> task_shop_order_contact_selected_version(const std::vector<std::pair<StableId,StableId>>& modules);
Status validate_task_shop_order_contact_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>& modules,bool selected,const TaskShopOrderContactState*);
Outcome<BoundaryValue> task_shop_order_contact_fact_payloads(const CommonStore&,const TaskShopOrderContactState*);
Status validate_task_shop_order_contact_json_payloads(const CommonStore&,const TaskShopOrderContactState*,const BoundaryValue& state);
}
