// 舟战军官是原真实人物；局部状态不加入兵员 token，也不另存人物生命或资产。
#pragma once
#include "battle_model.hpp"
namespace hanmo::resident {
// 原 BattleUnit.statuses 的具名扩展只描述本战已结算损失；无字段时旧档仍逐字节不变。
inline Integer naval_commander_loss(const BattleUnit& unit){for(const auto& status:unit.statuses)if(status.first=="naval_commander_loss_v1")return status.second;return 0;}
// 限定唯一已知状态和值；其它战法状态不能借舟战入口夹带。
inline bool valid_naval_commander_loss(const BattleUnit& unit){return unit.statuses.empty()||(unit.statuses.size()==1&&unit.statuses.front().first=="naval_commander_loss_v1"&&(unit.statuses.front().second==1||unit.statuses.front().second==2));}
// 状态仅单向推进，真正死亡由同轮 MilitaryCasualtyFact 与 Common 人物一次提交。
inline void mark_naval_commander_loss(BattleUnit& unit,Integer loss){if(loss>naval_commander_loss(unit))unit.statuses={{"naval_commander_loss_v1",loss}};}
}
