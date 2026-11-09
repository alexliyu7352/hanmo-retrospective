// 原缓存回执只引用本次真实写入，不以旧业务事实换挂新请求；失败Action保原中断来源。
#pragma once
#include "task_shop_order_business.hpp"
namespace hanmo::resident {
bool is_task_shop_order_command_kind(const StableId&);
// source为接受/取消的真实fact，或交付开始的原action；原Common先完成幂等key/result完整冷验证。
Status validate_task_shop_order_cached_source(const CommonStore&,const TaskState&,const TaskShopOrderState&,
    const StableId& key,const StableId& kind,const StableId& result_id,Integer tick,const StableId& source);
}
