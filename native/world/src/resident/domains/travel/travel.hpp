// 行旅领域公开接缝：规则纯报价，资源变更只经共同写端口，军事/交互只读服务由组合根绑定。
#pragma once
#include "../../checkpoint/codec_visitor.hpp"
#include "../../checkpoint/ordered_value.hpp"
#include "../../geometry/route_cache.hpp"
#include "../../geometry/terrain.hpp"
#include "../../indexes/contact_model.hpp"
#include "../../model/common_operations.hpp"
#include "cargo_handling_model.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "mobile_inventory_model.hpp"
#include "resident/domains/military/field_battle/field_spatial_view.hpp"
#include "travel_contact_capture.hpp"
#include "travel_model.hpp"
#include "travel_plan_capture.hpp"
#include "travel_rescue_capture.hpp"
#include "travel_step_expense.hpp"
namespace hanmo::resident {
// 可选作者地图归属只用于坐标域核对；不改变场景选择的普通旅行模式。
struct TravelSettlement {
  StableId id, entry_place;
  PointI map_xy;
  StableId world_map_id;
};
// 作者道路目录只保存原规则和有限地图投影所需字段，封路仍按共同tick读取。
struct TravelRoad {
  StableId id, origin, destination;
  Integer duration_ticks = 1, cost_per_tick = 8, fatigue_per_tick = 5,
          blocked_from_tick = 0, blocked_until_tick = 0;
  std::vector<PointI> shape;
  StableId world_map_id;
};
struct TravelPlace {
  StableId settlement_id;
  std::vector<StableId> passages;
};
struct TravelScheduleDefinition {
  std::vector<StableId> destinations;
  Integer dwell_ticks = 0, start_tick = 0;
};
// 静态目录只准备一次；各地图已在prepare时完整验证，热路由不重读JSON。
struct TravelCatalog {
  OrderedTable<CargoHandlingRule> cargo_handling_rules;
  OrderedTable<MobileInventoryDefinition> mobile_inventory_definitions;
  StableId world_map_id;
  bool road_encounters = false;
  OrderedTable<terrain::TerrainMap> maps;
  OrderedTable<TravelSettlement> settlements;
  OrderedTable<TravelPlace> places;
  OrderedTable<TravelRoad> roads;
  OrderedTable<TravelScheduleDefinition> authored_plans;
  std::shared_ptr<terrain::RouteCache> route_cache =
      checkpoint_make_shared<terrain::RouteCache>();
  // 目录复制生成新的纯缓存，旧地图地址不能跨副本生效；普通world
  // clone仍共享const目录。
  TravelCatalog() = default;
  TravelCatalog(const TravelCatalog &other)
      : cargo_handling_rules(other.cargo_handling_rules),
        mobile_inventory_definitions(other.mobile_inventory_definitions),
        world_map_id(other.world_map_id),
        road_encounters(other.road_encounters), maps(other.maps),
        settlements(other.settlements), places(other.places),
        roads(other.roads), authored_plans(other.authored_plans) {}
  TravelCatalog(TravelCatalog &&) = default;
  TravelCatalog &operator=(TravelCatalog &&) = default;
  TravelCatalog &operator=(const TravelCatalog &other) {
    if (this != &other) {
      TravelCatalog copied(other);
      *this = std::move(copied);
    }
    return *this;
  }
};
// 军事字段为同候选的只读引用，公私负载算法仍在B，不复制军队权威。
struct ArmyTravelView {
  StableId commander_actor_id, wallet_id, inventory_id;
  std::vector<StableId> member_actor_ids, fit_actor_ids;
};
// 仅已核实的实船、原军团和逐船载荷产生此证明；不能从mode字符串推定有船。
struct VesselFleetTravelView {
  StableId fleet_id, army_id, commander_actor_id, payer_wallet_id;
  Integer travel_budget = 0;
};
// 位置占用与出航能力分离；原事实具名，失能船不会被当成无船陆行。
// 原水上死亡地点只供冷几何准入，不授予航行或遗产处分权。
struct VesselRemainsLocationView {
  StableId actor_id, fleet_id, army_id, round_id, casualty_id;
  WildernessLocation anchor;
};
struct VesselFleetLocationView {
  StableId fleet_id, army_id, commander_actor_id, formation_source_id,
      world_map_id;
};
// 所有服务须真实绑定；空军队与未绑定不是同一状态，能力失败不能伪装成独行。
class TravelServices {
public:
  virtual ~TravelServices() = default;
  // 新 profile 必由 BundleServices 绑定；只有旧 profile 使用默认 absent。
  virtual Outcome<std::optional<FieldBattleSpatialView>>
  field_battle_position(const StableId &) const {
    return {true, std::nullopt, {}};
  }
  virtual Outcome<Location> resolved_location(const StableId &) const = 0;
  virtual StableId party_id(const StableId &) const = 0;
  virtual Integer party_extra_cost(const StableId &) const = 0;
  virtual Outcome<std::optional<ArmyTravelView>>
  active_army(const StableId &) const = 0;
  // 旧未启用实现没有实船；生产组合根显式绑定Naval唯一账，错误不能回退陆路。
  // 占用证明不要求船只此刻可航，防止失能船退化为陆军。
  // 新版舟行只在实际报价后追加远航船体门，冷占用与已有旅程不重跑此门。
  virtual Status naval_voyage_allowed(const StableId &,
                                      const TravelQuote &) const {
    return common_success();
  }
  virtual bool naval_berth(const StableId &) const { return false; }
  virtual bool army_embarked(const StableId &) const { return false; }
  virtual Outcome<std::optional<VesselFleetTravelView>>
  vessel_fleet(const StableId &) const {
    return {true, std::nullopt, {}};
  }
  virtual bool talking(const StableId &) const = 0;
  virtual Outcome<bool> admission_allowed(const StableId &actor_id,
                                          const StableId &place_id) const = 0;
  virtual const terrain::Flags &terrain_flags() const = 0;
  virtual const TravelScheduleDefinition *schedule(const StableId &) const = 0;
  virtual const StableId &player_actor_id() const = 0;
  virtual Outcome<std::optional<VesselFleetLocationView>>
  vessel_location(const StableId &) const {
    return {true, std::nullopt, {}};
  }
  // 原活动行旅冻结来源，不以当前可出航资格验证已存在的行动。
  virtual Outcome<std::optional<VesselRemainsLocationView>>
  vessel_remains(const StableId &) const {
    return {true, std::nullopt, {}};
  }
  virtual Outcome<std::optional<VesselFleetTravelView>>
  vessel_journey(const StableId &, const StableId &, const StableId &) const {
    return {true, std::nullopt, {}};
  }
};
struct TravelProfile {
  StableId mode, payer_wallet_id;
  Integer load_weight = 0, naval_budget = 0;
};
// 计划只核当前报价/能力，绝不强制未来整段世界预演。
Outcome<TravelProfile> travel_profile(const CommonRead &,
                                      const TravelServices &,
                                      const StableId &actor_id);
Outcome<TravelQuote>
plan_terrain_travel(const CommonRead &, const TravelState &,
                    const TravelCatalog &, const TravelServices &,
                    const StableId &actor_id, const TravelDestination &,
                    const std::vector<StableId> &via = {});
Status can_step_terrain_travel(const CommonRead &, const TravelCatalog &,
                               const TravelServices &, const ActionCoreView &,
                               const TerrainTravelParams &,
                               bool ignore_pause = false);
// 原预算/整数落点纯几何正文；只读地图及flags，不决定资格、不收费或写人物。
Outcome<TravelMotion>
terrain_geometry_motion(const TravelCatalog &, const TravelServices &,
                        const PointI &start, const TerrainTravelParams &,
                        bool moving, const StableId &actor_id,
                        const StableId &action_id);
Outcome<TravelMotion> terrain_motion(const CommonRead &, const TravelCatalog &,
                                     const TravelServices &,
                                     const ActionCoreView &,
                                     const TerrainTravelParams &,
                                     const std::vector<StableId> &hold = {});
// 当前事务调用begin前由中央安装行动头；B分配并登记一次journey事实，不分配第二行动ID。
Status begin_terrain_travel(
    CommonWrite &, TravelState &, StableId action_id, const TravelQuote &,
    std::optional<TravelPlanInstallationDraft> *installation = nullptr);
Outcome<TravelStepResult>
step_terrain_travel(CommonWrite &, TravelState &, const TravelCatalog &,
                    const TravelServices &, StableId action_id,
                    bool capture_execution = false,
                    const TravelMotion *snapshot = nullptr,
                    std::optional<TravelStepExpenseDraft> *expense = nullptr);
Outcome<PreparedTravelOp>
prepare_travel_control(const CommonRead &, const TravelState &,
                       const TravelCatalog &, const TravelServices &,
                       const StableId &actor_id, const TravelCommandPayload &);
Status
apply_travel_control(CommonWrite &, TravelState &, const TravelCatalog &,
                     const PreparedTravelOp &,
                     std::optional<TravelControlDraft> *control = nullptr);
Status preview_rescue(const CommonRead &, const TravelState &,
                      const TravelCatalog &, const TravelServices &,
                      const StableId &actor_id);
Status begin_rescue(
    CommonWrite &, TravelState &, const StableId &actor_id,
    std::optional<TravelRescueReplacementDraft> *replacement = nullptr);
Status
finish_rescue(CommonWrite &, TravelState &, const TravelCatalog &,
              const StableId &action_id,
              std::optional<TravelRescueCompletionDraft> *completion = nullptr);
Status preview_move_local(const CommonRead &, const TravelCatalog &,
                          const TravelServices &, const StableId &actor_id,
                          const StableId &place_id);
Status apply_move_local_location(CommonWrite &, const TravelCatalog &,
                                 const StableId &actor_id,
                                 const StableId &place_id);
Status erase_travel_action_payload(TravelState &, const StableId &action_id);
// 通用入口沿原有world_map_id/载荷分流；军事几何仍可直接使用上面的地形专用入口。
Outcome<TravelQuote> plan_travel(const CommonRead &, const TravelState &,
                                 const TravelCatalog &, const TravelServices &,
                                 const StableId &, const TravelDestination &,
                                 const std::vector<StableId> &via = {});
Status begin_travel(
    CommonWrite &, TravelState &, const TravelCatalog &, StableId,
    const TravelQuote &, const TravelCommand *source = nullptr,
    std::optional<TravelPlanInstallationDraft> *installation = nullptr);
Status can_step_travel(const CommonRead &, const TravelState &,
                       const TravelCatalog &, const TravelServices &,
                       const ActionCoreView &, bool ignore_pause = false);
Outcome<TravelStepResult>
step_travel(CommonWrite &, TravelState &, const TravelCatalog &,
            const TravelServices &, StableId, bool capture_execution = false,
            const TravelMotion *snapshot = nullptr,
            std::optional<TravelStepExpenseDraft> *expense = nullptr);
// 共有只读门保留paused/encounter来源，不能以没有地形载荷冒充没有旅行。
Outcome<std::pair<StableId, bool>> travel_encounter(const TravelState &,
                                                    const StableId &action_id);
Integer road_next_cost(const TravelCatalog &, const TravelServices &,
                       const ActionCoreView &, const RoadTravelParams &,
                       const OnRouteLocation &);
bool road_is_open(const TravelRoad &, Integer tick);
Outcome<TravelQuote> plan_road_route(const TravelCatalog &,
                                     const StableId &origin,
                                     const TravelDestination &, Integer tick,
                                     const std::vector<StableId> &via = {});
// 道路私有规则以typed局部值运行；改道和接触不会另造资源账本。
Outcome<TravelQuote> replan_road_travel(const CommonRead &,
                                        const TravelCatalog &,
                                        const TravelServices &,
                                        const StableId &,
                                        const TravelDestination &,
                                        const std::vector<StableId> &via = {});
Status replace_road_plan(CommonWrite &, TravelState &, const TravelCatalog &,
                         const StableId &, const TravelQuote &);
Status can_step_road_travel(const CommonRead &, const TravelCatalog &,
                            const TravelServices &, const ActionCoreView &,
                            const RoadTravelParams &,
                            bool ignore_pause = false);
Outcome<bool> finish_road_leg(const TravelCatalog &, Integer tick,
                              RoadTravelParams &, Location &);
std::optional<ContactHit> original_road_contact_intersection(
    const TravelCatalog &, const RoadTravelMotion &, const RoadTravelMotion &);
Outcome<std::vector<ContactHit>>
detect_road_contacts(const CommonRead &, const TravelState &,
                     const TravelCatalog &, const TravelServices &,
                     const std::vector<StableId> &hold = {});
Status apply_road_contact_truncation(CommonWrite &, TravelState &,
                                     const ContactHit &, const StableId &,
                                     bool);
// C唯一encounter安装入口调用本函数一次；step的钱粮疲劳不在接触截断时重复结算。
Status apply_contact_truncation(
    CommonWrite &, TravelState &, const ContactHit &,
    const StableId &encounter_id, bool interactive,
    std::optional<TravelContactTruncationDraft> *contact = nullptr);
Status resume_after_encounter(TravelState &, const StableId &action_id,
                              bool prior_paused);
// 选中捕获的复行另借真实Common行动/位置；旧三参签名和调用行为保留。
Status
resume_after_encounter(const CommonRead &, TravelState &,
                       const StableId &action_id, bool prior_paused,
                       std::optional<TravelEncounterResumeDraft> *resume);
Outcome<ContactBatch>
detect_travel_contacts(const CommonRead &, const TravelState &,
                       const TravelCatalog &, const TravelServices &,
                       Integer phase_token,
                       const std::vector<StableId> &hold = {});
// actions前如果资源版本已变，中央仍调用can_step；不得为缓存命中跳过这个守卫。
bool contact_versions_current(const CommonRead &, const ContactBatch &);
Outcome<std::vector<TravelScheduleProposal>>
travel_schedule_proposals(const CommonRead &, const TravelState &,
                          const TravelCatalog &, const TravelServices &);
Status finish_travel_arrivals(const CommonRead &, TravelState &,
                              const TravelServices &);
Status mark_travel_departure(TravelState &, const TravelScheduleProposal &);
Status validate_travel_domain(const CommonRead &, const TravelState &,
                              const TravelCatalog &, const TravelServices &);
Status visit_travel_checkpoint(CodecVisitor &, TravelState &);
Outcome<TravelCatalog> decode_travel_catalog(const BoundaryValue &data,
                                             const StableId &scenario_id);
Outcome<TravelState> decode_travel_seed(const BoundaryValue &state);
class ProjectionWriter;
Outcome<bool> emit_travel_action_params_record_projection(ProjectionWriter &,
                                                          std::string_view,
                                                          const TravelState &,
                                                          const StableId &);
Outcome<bool> emit_travel_fact_record_projection(ProjectionWriter &,
                                                 std::string_view,
                                                 const TravelState &,
                                                 const StableId &);
Outcome<bool> emit_travel_itinerary_record_projection(ProjectionWriter &,
                                                      std::string_view,
                                                      const TravelState &,
                                                      const StableId &);
Status emit_travel_action_params_projection(ProjectionWriter &,
                                            const TravelState &);
Status emit_travel_facts_projection(ProjectionWriter &, const TravelState &);
Status emit_travel_itineraries_projection(ProjectionWriter &,
                                          const TravelState &);
Status emit_travel_state_projection(ProjectionWriter &, std::string_view,
                                    const TravelState &);
Outcome<BoundaryValue> export_travel_state(const TravelState &);
} // namespace hanmo::resident
