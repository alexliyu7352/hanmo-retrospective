// 海陆共用原Travel轨迹/相交排序；舟战只加入真实已绑定舰队，不从boat字符串推断有船。
#pragma once
#include "battle_world_model.hpp"
#include "naval_world.hpp"
#include "military_contact.hpp"
namespace hanmo::resident {
struct NavalState;struct NavalCatalog;
struct NavalContactHit { StableId first_fleet_id,second_fleet_id;MilitaryContactHit contact; };
struct NavalContactHistory {
 StableId id,first_fleet_id,second_fleet_id,battle_id;Integer tick=0;
 MilitaryMotionSource first_motion,second_motion;
};
struct NavalContactProbe { std::vector<NavalContactHit> ordered_hits;Integer eligible_fleets=0,candidate_pairs=0,exact_pairs=0; };
// occupied_fleets由原NavalBattleState给出，history只防同一未结束行程反复撞入同一静止船。
Outcome<NavalContactProbe> probe_naval_contacts(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalAssetState&,const NavalAssetCatalog&,const TravelState&,const TravelCatalog&,const TravelServices&,const std::vector<NavalContactHistory>&,const std::vector<StableId>& occupied_fleets,const std::vector<StableId>& held_actors);
// 舟战profile使用同一相交器和原航行规则，并核接管后的真实剩余名册。
Outcome<NavalContactProbe> probe_naval_contacts(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,const NavalState&,const NavalCatalog&,const TravelState&,const TravelCatalog&,const TravelServices&,const std::vector<NavalContactHistory>&,const std::vector<StableId>& occupied_fleets,const std::vector<StableId>& held_actors);
}
