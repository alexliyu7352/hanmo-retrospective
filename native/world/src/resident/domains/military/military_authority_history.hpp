// 历史军权只读实际任职、辞职、占领与整编事实；当前任职不能反向授权旧命令。
#pragma once
#include "battle_world.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct NavalOriginalSourceContext;
Outcome<OfficeRecord> military_office_before(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& office_id,Integer tick,const StableId& before_id);
// 仅原单半日候选允许读到to；旧稳定签名保持Common.tick，不开放任意上限。
Outcome<OfficeRecord> military_office_before(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const StableId& office_id,Integer tick,const StableId& before_id,const DynamicStepInput&);
// caller先证明历史观察者身份及真实参战名册；本函数仅返回原军级/本人本部控制集合。
Outcome<MilitaryBattleControl> military_control_before(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const std::vector<BattleUnit>&,const StableId& actor,Integer tick,const StableId& before_id);
// Same control policy with explicit finite source selection, no exact tick/M
// conversion. The Naval event proves only its actual Common ordinal cut.
Outcome<MilitaryBattleControl> military_control_before(const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const std::vector<BattleUnit>&,const StableId& actor,Integer tick,const StableId& before_id,const NavalOriginalSourceContext&);
}
