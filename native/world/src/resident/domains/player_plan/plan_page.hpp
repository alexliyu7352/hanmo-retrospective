// 玩家计划页面只借原世界已知来源，组合有限条款、真实承诺和条件预计。
#pragma once
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 旧profile只返回关闭状态；他人计划和隐藏世界状态不进入此查询。
Outcome<BoundaryValue> query_player_plan_page(const WorldBundle&,const StableId& actor_id);
} // 查询不准备新资源、不模拟未来人物位置，也不启动计划驱动。
