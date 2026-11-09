// 接管先产严格资源recipe和载荷候选；外围唯一World事务再登记原兵员/事实和实际产权。
#pragma once
#include "naval_battle_state.hpp"
#include "resident/model/common_operations.hpp"
namespace hanmo::resident {
struct NavalCaptureQuote {
 StableId round_source_id,vessel_id,from_fleet_id,to_fleet_id,captor_unit_id,authority_source;
 Owner from_owner,to_owner;WildernessLocation anchor;
 std::vector<StableId> prisoner_ids,original_prisoner_ids,released_vessel_ids;
 // 接管整支败方行动的终止由外层原Travel入口执行，不直接清掉人物action引用。
 std::vector<StableId> interrupted_action_ids;
 std::vector<NavalVesselAssignment> assignments;std::vector<NavalVesselLoad> loads;
 std::vector<NavalCapturedCargo> cargo;std::vector<StableId> emptied_inventory_ids;
 ResourceRecipe resources;
};
// authority_actor沿同世界当前军职或实际整军接任判断；人货观察仍由原Common/军旅账生成。
// 该报价只消费已保存的真实round claim，拒伪造/重复接管与被预留的船货。
Outcome<NavalCaptureQuote> preview_naval_capture(const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const NavalAssetState&,const NavalAssetCatalog&,const NavalBattleState&,const StableId& authority_actor,const NavalCaptureCommand&);
}
