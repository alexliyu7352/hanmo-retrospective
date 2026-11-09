// 具名在途货批的原recipe触点检查；只返回需观察的操作，不授连续性、资金或资源写权限。
#pragma once
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {
// 由未来完整携运原账给出当前真实原lot；这里不能按品名或今日同量替换旧lot。
struct PrivateShopFreightCargoSelection {StableId inventory_id;std::vector<StableId> lot_ids;};
// 同一操作可能影响多个被选原批，顺序保recipe原序及selection原序。
struct PrivateShopFreightResourceTouch {
    std::size_t operation_index=0;
    std::vector<PrivateShopFreightCargoSelection> selections;
};
// actual_before只借本次原Common暂存前库存。结果是保守观察清单，不是“这些货仍完好”的证明。
// 随后的真实staging、原业务原因、原lot分裂/转移映射和完整反向coverage均不可省略。
// 不执行recipe、不拒绝旧业务，不检查钱包费用，也不替代Common自身的资源合法性校验。
Outcome<std::vector<PrivateShopFreightResourceTouch>> inspect_private_shop_freight_resource_scope(
    const OrderedTable<InventoryView>& actual_before,
    const std::vector<PrivateShopFreightCargoSelection>&,
    const ResourceRecipe&);
}
