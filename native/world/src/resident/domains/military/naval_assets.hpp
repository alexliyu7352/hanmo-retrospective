// S2唯一实船与编队登记；人员、钱货和世界航线仍引用原军旅/Common/Travel，不复制余额。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "naval_vessel_ref.hpp"
#include "sea_embark_care_ref.hpp"
namespace hanmo::resident {
struct NavalHullDefinition {
 StableId id;Integer max_integrity=0,minimum_crew=0,person_capacity=0,cargo_capacity=0,minimum_naval_level=0,movement=0,travel_budget=0;
 std::vector<StableId> waters;StableId name_key;
};
// 泊地只引用Travel现有place，明确内河能力，不另存世界坐标。
struct NavalBerthDefinition { StableId place_id,water_kind; };
// 作者实船定义保原始产权来源，运行时不从人数或钱款临时生成船。
struct NavalVesselDefinition { StableId id,hull_id,cargo_inventory_id,berth_id;Owner owner;Integer integrity=0;StableId name_key; };
struct NavalAssetCatalog { StableId profile="afloat_inland_v1",world_map_id;std::vector<NavalHullDefinition> hulls;std::vector<NavalBerthDefinition> berths;std::vector<NavalVesselDefinition> vessels; };
struct NavalVesselRecord {
 StableId id,hull_id,cargo_inventory_id,berth_id,fleet_id,origin_source;Owner owner;Integer integrity=0;
 // 未编队的失能船仅保存一次真实世界锚点；编队航行时位置始终取原统帅Travel。
 std::optional<Location> afloat_location;FieldOrder field_order;
};
struct NavalVesselAssignment { StableId vessel_id,unit_id;std::vector<StableId> passenger_ids,prisoner_ids,extra_inventory_ids;FieldOrder field_order; };
// active只表示编队占用仍有效；航行/战斗状态从原Travel/战斗事实派生，不另记移动时钟。
struct NavalFleetRecord { StableId id,army_id,commander_actor_id,source_id,office_id,appointment_source,formation_source,formation_berth_id,request_key,release_source_id,release_request_key,release_berth_id,release_actor_id,release_authority_kind,release_authority_id,release_authority_source,status="active";Integer created_tick=0,released_tick=-1;std::vector<NavalVesselAssignment> assignments;FieldOrder field_order; };
// 退编仍保实际请求人和原返营身份，不把代办回执写成死去统帅。
struct NavalReleaseObservation { StableId berth_id,authority_kind,authority_id,authority_source; };
struct NavalAssetState { std::vector<NavalVesselRecord> vessels;std::vector<NavalFleetRecord> fleets; };
// 以下观察只在一次报价内借用：世界适配器须从原合法军团/军职、人物和库存生成。
struct NavalObservedInventory { StableId id;Owner owner;Integer weight=0; };
struct NavalObservedPerson { StableId id,inventory_id;bool alive=false,available=false;Integer naval_level=0;StableId action_id; };
struct NavalObservedUnit { StableId id,army_id,commander_actor_id;std::vector<StableId> fit,wounded;StableId busy_source_id; };
struct NavalFormationObservation {
 StableId actor_id,army_id,organization_id,army_inventory_id,berth_id,office_id,appointment_source,formation_source;Integer tick=0;bool army_active=false;
 std::vector<StableId> army_unit_ids,army_member_ids,prisoner_ids;std::vector<NavalObservedUnit> units;std::vector<NavalObservedPerson> people;std::vector<NavalObservedInventory> inventories;
};
struct NavalFormationRequest { StableId army_id;std::vector<NavalVesselAssignment> assignments; };
struct NavalVesselLoad { StableId vessel_id,unit_id;Integer people=0,working_crew=0,cargo_weight=0;std::vector<StableId> inventory_ids; };
// 原Travel只借真实舰队证明；预算仍沿同一地形引擎和付款军库。
struct NavalTravelProof { StableId fleet_id,army_id,commander_actor_id,payer_wallet_id;Integer travel_budget=0;std::vector<NavalVesselLoad> loads; };
struct NavalFormationQuote { std::vector<NavalVesselLoad> loads;Integer movement=0,travel_budget=0; };
// 热命令、冷历史与公开回执共享同一业务键边界。
bool naval_request_key_valid(const StableId&);
// 纯域先拒非法形状和重叠；真正人物/产权/历史来源仍需同世界适配器进一步核证。
Status validate_naval_assets(const NavalAssetState&,const NavalAssetCatalog&);
// 舟战段2可出现末船接管终态；这里只核形状，完整来源仍由Naval2历史重放验证。
Status validate_naval_assets(const NavalAssetState&,const NavalAssetCatalog&,bool combat_enabled);
Outcome<NavalFormationQuote> preview_naval_formation(const NavalAssetState&,const NavalAssetCatalog&,const NavalFormationObservation&,const NavalFormationRequest&,bool floating_state=false);
// 显式Naval2沿同一载重算法接受有合法捕获历史的舰队形状，旧调用仍严格段1。
Outcome<NavalFormationQuote> preview_naval_formation(const NavalAssetState&,const NavalAssetCatalog&,const NavalFormationObservation&,const NavalFormationRequest&,bool floating_state,bool combat_enabled);
// ID/事实号必须由Common唯一分配；此处只返回整候选，失败不改原状态或任何外部资源。
Outcome<NavalAssetState> commit_naval_formation(const NavalAssetState&,const NavalAssetCatalog&,const NavalFormationObservation&,const NavalFormationRequest&,const StableId& fleet_id,const StableId& source_id);
Outcome<NavalAssetState> commit_naval_formation(const NavalAssetState&,const NavalAssetCatalog&,const NavalFormationObservation&,const NavalFormationRequest&,const StableId& fleet_id,const StableId& source_id,bool combat_enabled);
}
