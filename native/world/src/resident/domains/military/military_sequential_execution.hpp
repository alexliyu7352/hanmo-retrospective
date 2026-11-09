// 顺序轮只借原纯报价与几何/目标函数；实际World仍由原round唯一提交和消费半日。
#pragma once
#include "military_sequential_rounds.hpp"
namespace hanmo::resident {
Outcome<MilitaryBattleRecord> resolve_military_sequential_round(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId&,const MilitaryOrders&,const MilitaryBattleRandom&,const MilitaryDamageSources&);
namespace military_sequential_detail {
Outcome<MilitaryAttackQuote> attack_quote(const MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId& unit_id,const MilitaryOrder&,Integer guard,Integer moved,Integer incoming,const MilitaryDamageSources&);
Outcome<Integer> move_committed_path(MilitaryBattleRecord&,const MilitaryBattleCatalog&,const StableId& unit_id,const std::vector<HexPoint>&,const StableId& tactic_kind);
std::optional<MilitaryBattleResult> objective(MilitaryBattleRecord&,const MilitaryBattleCatalog&,bool at_round_end);
}
}
