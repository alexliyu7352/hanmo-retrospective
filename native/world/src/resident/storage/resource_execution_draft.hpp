// 原资源执行的同栈临时证据；不保存进Common、不分配事实ID，也不授予库存历史或付款权。
#pragma once
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {

// 每个原语实际触及的值保原字段序和lot顺序；创建之前为空，零净变化也保该原语。
struct ResourceWalletExecutionDraft {
    StableId wallet_id;
    std::optional<WalletView> before;
    WalletView after;
};
struct ResourceInventoryExecutionDraft {
    StableId inventory_id;
    std::optional<InventoryView> before;
    InventoryView after;
};

// 映射在原FIFO循环内记录，不能靠最后数量差重造拆批、转移或保ID整编来源。
struct ResourceLotFlowDraft {
    enum class Kind { add, reserve_whole, reserve_split, transfer, consume, adjust, integration_move, selected_reserve_whole, selected_reserve_split, selected_transfer_whole, selected_transfer_split, selected_consume };
    Kind kind=Kind::add;
    StableId source_inventory_id,source_lot_id,destination_inventory_id,destination_lot_id,item_id;
    Integer quantity=0;
};
struct ResourceOperationExecutionDraft {
    std::size_t operation_index=0;
    std::vector<ResourceWalletExecutionDraft> wallets;
    std::vector<ResourceInventoryExecutionDraft> inventories;
    std::vector<ResourceLotFlowDraft> lot_flows;
    Integer next_instance_before=0,next_instance_after=0;
};

// 一整recipe成功才交付；业务永久cause、实际result和完整消费必须由原World提交栈另行绑定。
struct ResourceExecutionDraft {
    WorldIdentity identity;
    ResourceRecipe recipe;
    Integer next_instance_before=0,next_instance_after=0;
    std::vector<ResourceOperationExecutionDraft> operations;
};

} // 本片不覆盖Actor/Party/军团/舟船绑定，不冒任意资源before-fact全史。
