// 正规effective目录初态的库存投影；仅初货和实例分配下界，不建立第二个可花资源账。
#pragma once
#include "resident/model/common_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
struct NativeInventorySeed { OrderedTable<InventoryView> inventories;Integer next_instance_id=1; };
// 必须传原effective_catalog已准入的data/selected initial；不从当前Common或旧档推导初态。
Outcome<NativeInventorySeed> seed_native_inventory_assets(const BoundaryValue& data,const BoundaryValue& initial);
}
