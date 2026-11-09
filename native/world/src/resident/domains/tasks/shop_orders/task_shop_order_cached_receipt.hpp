// 原Common缓存明细只借具名店单永久来源；不新增回执tag、请求参数账或World持有者。
#pragma once
#include "task_shop_order_cached_source.hpp"
namespace hanmo::resident {
// 必须在原Common与完整Task/Shop冷验证之后调用；空state仍拒新命令历史和具名孤回执。
Status validate_task_shop_order_cached_results(const CommonStore&,const TaskState&,const TaskShopOrderState*);
}
