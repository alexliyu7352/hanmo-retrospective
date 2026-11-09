// 原BattleUnit状态只承载本场已验证进度；冷授权仍必须对上Military10永久回合证明。
#pragma once
#include "military_rout_model.hpp"
namespace hanmo::resident {
const MilitaryRoutRetreatRule* military_rout_rule_in_use(const MilitaryBattleRecord&,const MilitaryBattleCatalog&);
bool military_rout_status_key(const StableId&);
bool military_unit_routing(const BattleUnit&);
Outcome<std::vector<MilitaryRoutProgress>> read_military_rout_progress(const MilitaryBattleRecord&,const MilitaryBattleCatalog&);
// 只在完整候选上更新两个具名永久状态；保其他战法/军官伤亡标记和兵员真值。
Status write_military_rout_progress(MilitaryBattleRecord&,const std::vector<MilitaryRoutProgress>&);
}
