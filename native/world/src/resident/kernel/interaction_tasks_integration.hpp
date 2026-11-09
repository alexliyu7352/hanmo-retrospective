// 当面任务入口复用同一个WorldBundle和统一命令报价；不保存第二份会话或任务状态。
#pragma once
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
struct WorldBundle;
Outcome<BoundaryValue> query_interaction_tasks(const WorldBundle&,Integer cursor=0);
}
