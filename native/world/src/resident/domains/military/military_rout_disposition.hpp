// 溃退处置只在原同步移动与同轮伤亡完成后生成候选；原MilitaryCasualtyFact负责唯一真实回写。
#pragma once
#include "military_rout_retreat.hpp"
namespace hanmo::resident {
struct MilitaryRoutDisposition {
 MilitaryBattleRecord battle;
 std::vector<MilitaryRoutProgress> progress;
 std::vector<MilitaryBattleEvent> events;
};
Outcome<MilitaryRoutDisposition> resolve_military_rout_disposition(
 const MilitaryBattleRecord& before,const MilitaryBattleRecord& after_movement_and_damage,
 const MilitaryBattleCatalog&,const MilitaryRoutRetreatRule&,const std::vector<MilitaryRoutProgress>&);
}
