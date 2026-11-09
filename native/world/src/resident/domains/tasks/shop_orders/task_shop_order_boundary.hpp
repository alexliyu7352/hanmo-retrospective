// 三个正式任务命令只收原单/Task身份；价格、钱袋与批次永远取真实冻结来源。
#pragma once
#include "task_shop_order_execution.hpp"
namespace hanmo::resident {
struct TaskShopOrderCommand {enum class Kind {accept,deliver,cancel};Kind kind=Kind::accept;StableId id;};
StableId task_shop_order_command_kind(const TaskShopOrderCommand&);
BoundaryValue encode_task_shop_order_command(const TaskShopOrderCommand&);
Outcome<TaskShopOrderCommand> decode_task_shop_order_command(const StableId&,const BoundaryValue&);
// 激活必须显式选择店单profile与真实S5店铺域；没有订单政策不凭标记造来源。
Outcome<bool> decode_task_shop_order_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
Status validate_task_shop_order_layout(const std::vector<std::pair<StableId,StableId>>&,bool enabled,bool shop_enabled,bool has_order_policies,const TaskShopOrderState*);
// 未启用独立段时拒其具名孤头和Action；不能只凭目录缺省丢弃新责任。
Status validate_task_shop_order_layout(const CommonStore&,const std::vector<std::pair<StableId,StableId>>&,bool enabled,bool shop_enabled,bool has_order_policies,const TaskShopOrderState*);
// 取消纯预览和实际采用共用同一门，不通过临时执行和回滚模拟查询。
Status preview_task_shop_order_cancellation(const CommonStore&,const TaskState&,const TaskShopOrderState&,const PrivateShopOrderRead&,const StableId& actor_id,const StableId& task_id);
}
