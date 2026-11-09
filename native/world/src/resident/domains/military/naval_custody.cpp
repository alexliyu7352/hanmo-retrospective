#include "resident/domains/military/military_model.hpp"
// 仅根据同一Naval事实链返回现存军团ID；权限由完整World验证链确认，UI不能反向写此读口。
#include "naval_custody.hpp"
#include "naval_commander.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
template<class T>const T* find(const std::vector<T>& rows,const StableId& id){for(const auto& r:rows)if(r.id==id)return &r;return nullptr;}
bool has(const std::vector<StableId>& rows,const StableId& id){return std::find(rows.begin(),rows.end(),id)!=rows.end();}
// 末船接管可发生在独立军官已阵亡之后；只读原回合与唯一收据，不把军官塞进 token.dead。
bool commander_killed_before_capture(const NavalState& naval,const MilitaryState& military,const ArmyRecord& army,const NavalCaptureFact& capture){Integer deaths=0;bool witnessed=false;for(const auto& receipt:military.casualty_facts)for(const auto& change:receipt.actor_changes)if(change.actor_id==army.commander_actor_id&&change.status=="dead"){++deaths;const auto* round=find(naval.battle.rounds,receipt.battle_id);if(!round||receipt.id!=round->casualty_source_id||receipt.tick!=round->tick||receipt.tick>capture.tick||round->battle_id!=capture.battle_id||change.army_id!=army.id)continue;bool prior=false,boundary=false;for(const auto& r:naval.battle.rounds){if(r.id==round->id)prior=true;if(r.id==capture.round_source_id){boundary=true;break;}}if(!prior||!boundary)continue;for(const auto& u:round->after.units)if(u.army_id==army.id&&u.commander_actor_id==army.commander_actor_id&&naval_commander_loss(u)==2)witnessed=true;}return deaths==1&&witnessed;}

}
// 后一次实际转交取代上一次看管方；原军团关闭返营后仍沿原garrison custody规则定位。
std::optional<StableId> naval_custodian(const NavalState& naval,const StableId& person){
 const NavalCaptureFact* latest=nullptr;for(const auto& fact:naval.battle.captures)if(has(fact.prisoner_ids,person)||has(fact.original_prisoner_ids,person))latest=&fact;
 const auto* fleet=latest?find(naval.fleets,latest->to_fleet_id):nullptr;return fleet?std::optional<StableId>(fleet->army_id):std::nullopt;
}
// 只有末船确已接管且无人仍留败军时才承认败军终态；原钱包/库存仍属于原Common账。
std::optional<StableId> naval_defeat_source(const NavalState& naval,const MilitaryState& military,const StableId& army_id){
 const auto* army=find(military.armies,army_id);if(!army||army->status!="defeated"||!army->member_actor_ids.empty()||!army->prisoner_ids.empty())return std::nullopt;
 for(const auto& id:army->unit_ids){const auto* unit=find(military.military_units,id);if(!unit||unit->army_id!=army_id||!unit->fit.empty()||!unit->wounded.empty())return std::nullopt;}
 const NavalFleetRecord* lost=nullptr;for(const auto& fleet:naval.fleets)if(fleet.army_id==army_id){if(fleet.status=="active")return std::nullopt;if(fleet.status=="captured")lost=&fleet;}
 const auto* fact=lost?find(naval.battle.captures,lost->release_source_id):nullptr;if(!fact||fact->from_fleet_id!=lost->id||fact->tick!=lost->released_tick||(!has(fact->prisoner_ids,army->commander_actor_id)&&!commander_killed_before_capture(naval,military,*army,*fact)&&std::none_of(military.military_units.begin(),military.military_units.end(),[&](const auto& unit){return unit.army_id==army_id&&has(unit.dead,army->commander_actor_id);})))return std::nullopt;
 return fact->id;
}
}
