// 同一resident候选上的已注册有限读取；来源身份门由中央execute_live先核，不建立第二world或worker。
#pragma once
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
struct WorldBundle;
// 观察者固定metadata.player；未知查询明确UnsupportedLiveQuery，不用整态导出充当视图。
Outcome<BoundaryValue> query_live_player_view(const WorldBundle&,const StableId& query_id,const BoundaryValue& args);
}
