// 水战微步沿原六角/同步边界；实船船体唯一留在Naval账，部队仍为原真实军旅单位。
#pragma once
#include "naval_assets.hpp"
#include "naval_gunnery_model.hpp"
#include "naval_tactical_navigation.hpp"
#include "resident/domains/military/battle_model.hpp"
namespace hanmo::resident {
// 只联系原舰船和本场原军旅单位，不复制人口、军资或船的Owner/船体值。
struct NavalBattleBinding { StableId vessel_id,unit_id,fleet_id;FieldOrder field_order; };
struct NavalTacticalFrame {
 StableId id,source_id,battlefield_id,controller_actor_id,phase="planning";Integer round=1,controller_side=0;
 std::vector<BattleUnit> units;std::vector<NavalBattleBinding> bindings;
 std::vector<StableId> controlled_unit_ids;bool plan_submitted=false;FieldOrder field_order;
};
// 航行与接舷只给一个主动意图；接舷合法不等于胜利或已经俘获。
struct NavalTacticalOrder { StableId action="hold",target_unit_id;HexPoint destination;Integer heading=0;FieldOrder field_order; };
using NavalTacticalOrders=std::vector<std::pair<StableId,NavalTacticalOrder>>;
struct NavalTacticalQuote { std::vector<HexPoint> path;Integer cost=0;bool requests_boarding=false; };
struct NavalTacticalEvent { StableId kind,unit_id,target_unit_id,code;HexPoint position; };
struct NavalManeuverResult { NavalTacticalFrame frame;std::vector<NavalTacticalEvent> events;std::vector<std::pair<StableId,StableId>> boarding_requests,ram_requests,surrender_requests,gunnery_requests; };
// 来源由World提供；此只读规则不分配ID、不改军旅原人口、不消耗额外世界半日。
Status validate_naval_tactical_frame(const NavalAssetState&,const NavalAssetCatalog&,const BattlefieldDefinition&,const NavalTacticalFrame&);
Outcome<std::vector<HexPath>> naval_movement_options(const NavalAssetState&,const NavalAssetCatalog&,const BattlefieldDefinition&,const NavalTacticalFrame&,const StableId& unit_id,const NavalTacticalNavigationPolicy* =nullptr,std::optional<Integer> final_heading=std::nullopt);
Outcome<NavalTacticalQuote> preview_naval_tactical_order(const NavalAssetState&,const NavalAssetCatalog&,const BattlefieldDefinition&,const NavalTacticalFrame&,const StableId&,const NavalTacticalOrder&,const NavalTacticalNavigationPolicy* =nullptr,const NavalGunneryPolicy* =nullptr);
// full_orders必须由原同轮冻结的玩家/AI计划合并，不能从未交草案重建AI。
Outcome<NavalManeuverResult> resolve_naval_maneuvers(const NavalAssetState&,const NavalAssetCatalog&,const BattlefieldDefinition&,const NavalTacticalFrame&,const NavalTacticalOrders& full_orders,const NavalTacticalNavigationPolicy* =nullptr,const NavalGunneryPolicy* =nullptr);
}
