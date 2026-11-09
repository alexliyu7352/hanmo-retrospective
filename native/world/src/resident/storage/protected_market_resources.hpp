// 选定市场的同候选资源观察口；只检查原recipe和捕获真实staging，不提供第二资源写入口。
#pragma once
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {
class CommonStore;
// 由World同候选服务持有，原业务在栈scope内绑定精确原因；复制Common后须重新绑定。
class ProtectedMarketResourceObserver {
public:
    virtual ~ProtectedMarketResourceObserver()=default;
    // 必须逐完整recipe默认拒绝未声明写者，包含间接释放预约及不增revision的操作。
    virtual Status inspect_market_recipe(const CommonStore&,const ResourceRecipe&) const=0;
    // 原语全部staging成功后、发布前调用；失败不得留下任何已接受草稿或修改共同/领域状态。
    // 成功只保存当前scope的临时批次差，之后原业务事实和永久回执须由外层同候选一起提交。
    virtual Status capture_market_recipe(const CommonStore&,const ResourceRecipe&,
        const OrderedTable<InventoryView>& staged_inventories,Integer next_instance_after)=0;
};
}
