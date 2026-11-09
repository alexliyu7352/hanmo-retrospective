// 陆战具名军官损失只属于原战场与原人物；不加入兵卒名册或复制Common生命状态。
#pragma once
#include "battle_model.hpp"
#include <algorithm>
namespace hanmo::resident {
// 永久标记只表达此场会战已结算的伤亡，不把值解释成战法到期轮。
inline Integer military_commander_loss(const BattleUnit& unit){for(const auto& row:unit.statuses)if(row.first=="military_commander_loss_v1")return row.second;return 0;}
// 只单向推进本战军官损失；原有战法状态及兵员五分类保持。
inline void mark_military_commander_loss(BattleUnit& unit,Integer loss){for(auto& row:unit.statuses)if(row.first=="military_commander_loss_v1"){row.second=std::max(row.second,loss);return;}if(loss>0)unit.statuses.emplace_back("military_commander_loss_v1",loss);}
// 显式作者策略有限且无默认启用；旧战场完全不多抽随机。
Status validate_military_commander_rule(const MilitaryCommanderLossRule&);
// 调用者提供同一原轮次伤兵结算前后候选；只借原RNG，失败须丢弃外层候选。
Status resolve_military_commander_losses(const MilitaryBattleRecord& before,MilitaryBattleRecord& after,
 const MilitaryCommanderLossRule&,const MilitaryBattleRandom&);
}
