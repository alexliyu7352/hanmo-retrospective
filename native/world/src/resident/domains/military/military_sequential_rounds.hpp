// 队列纯读只使用原已验证M14入战统率；今日人物属性不能改变旧战先后。
#pragma once
#include "battle_model.hpp"
#include "military_sequential_contract.hpp"
namespace hanmo::resident {
Status validate_military_sequential_rule(const MilitarySequentialRoundRule&);
bool military_sequential_round_in_use(const MilitaryBattleRecord&,const MilitaryBattleCatalog&);
// 每轮轮转全部原参与者稳定ID，再按原统率/10＋兵种速度排序；不抽随机。
Outcome<std::vector<MilitarySequentialTurn>> query_military_sequential_turns(const MilitaryBattleRecord&,const MilitaryBattleCatalog&);
}
