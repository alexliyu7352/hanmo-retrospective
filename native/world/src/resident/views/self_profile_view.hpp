// 本人档案只投影当前实际玩家，不接收目标人物，不推进世界或产生随机数。
#pragma once
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 观察者必须等于当前玩家；未知年龄为-1，缺失或越界来源明确失败且不返回部分档案。
Outcome<BoundaryValue> self_profile_view(const WorldBundle&,const StableId& observer);
}
