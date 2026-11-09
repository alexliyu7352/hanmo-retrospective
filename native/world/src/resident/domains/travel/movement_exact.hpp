// 精确运动只有一个cursor与原unit；各调用必须在同一完整WorldBundle候选中完成。
#pragma once
#include "movement_exact_model.hpp"
#include "spatial_location_capture.hpp"
#include "personal_movement_funding.hpp"
namespace hanmo::resident {
class CheckpointMemoryCharge;
// 持久浮点只用有限正零规范位模式；这些入口不授予任何移位权限。
Outcome<StableId> encode_movement_binary64(double);
Outcome<double> decode_movement_binary64(const StableId&);
Outcome<MovementAnchor> read_movement_position(const MovementPrefixState&,ExactArithmeticBudget budget={});
Outcome<Location> project_movement_location(const MovementAnchor&,ExactArithmeticBudget budget={});
Outcome<CanonicalFraction> movement_consumed_cost(const TerrainCostBasis&,const CanonicalFraction&,ExactArithmeticBudget budget={});
Outcome<TerrainCostBasis> make_original_terrain_cost_basis(const TravelMotion&,const StableId& basis_id,const StableId& rule_ref,const StableId& unit_ref,const SourceRef&,ExactArithmeticBudget budget={});
Outcome<RoadMovementBasis> make_original_road_movement_basis(const RoadTravelMotion&,const StableId& basis_id,const StableId& rule_ref,const StableId& unit_ref,const SourceRef&,ExactArithmeticBudget budget={});
// 只有真实已安装Action/旅程进入；不以报价预收款或分配影子Action。
Outcome<StableId> install_exact_personal_terrain_movement(CommonWrite&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const StableId& action_id,const WorldMoment&,ExactArithmeticBudget budget={});
Outcome<StableId> install_exact_personal_road_movement(CommonWrite&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const RoadTravelMotion&,const WorldMoment&,ExactArithmeticBudget budget={});
struct PreparedMovementPrefix { StableId prefix_id,unit_ref; MomentSpan span; CanonicalFraction cursor_after; HalfDayDuration active_elapsed_after; std::optional<TerrainUnitBudget> terrain_after; MovementAnchor anchor_after; bool first_payment=false,at_unit_boundary=false,at_basis_end=false,arrived=false; };
Outcome<PreparedMovementPrefix> prepare_movement_prefix(const MovementExactState&,const StableId&,const MomentSpan&,ExactArithmeticBudget budget={});
Outcome<TerrainCostBasis> make_continued_terrain_cost_basis(const TravelCatalog&,const TravelServices&,const MovementAnchor&,const TerrainTravelParams&,const CanonicalFraction& maximum,const StableId& basis_id,const StableId& unit_id,const SourceRef&,ExactArithmeticBudget budget={},CheckpointMemoryCharge* retained=nullptr);
Outcome<StableId> continue_exact_personal_terrain_unit(CommonWrite&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const StableId& prefix_id,const WorldMoment&,ExactArithmeticBudget budget={});
Outcome<StableId> continue_exact_personal_road_unit(CommonWrite&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const StableId& prefix_id,const WorldMoment&,ExactArithmeticBudget budget={});
Outcome<StableId> continue_exact_terrain_basis_in_unit(CommonWrite&,MovementExactState&,const TravelCatalog&,const TravelServices&,const StableId& prefix_id,const WorldMoment&,ExactArithmeticBudget budget={});
Outcome<WorldMoment> predict_movement_boundary(const MovementExactState&,const StableId& prefix_id,ExactArithmeticBudget budget={});
class CurrentMomentRead;
// S0 已选合法正片的 from 采纳槽：用真实 from 截面收费，to 只应用既已采纳的片。
Status adopt_exact_personal_positive_span_from_in_candidate(CommonWrite&,const TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const CurrentMomentRead&,const StableId& prefix_id,const MomentSpan&,ExactArithmeticBudget budget={},const PersonalMovementFundingRead* funding=nullptr);
Status validate_personal_movement_expense_sources(const CommonStore&,const MovementExactState&,ExactArithmeticBudget budget={},const PersonalMovementFundingRead* funding=nullptr,const TravelCatalog* catalog=nullptr,const PersonalMovementPartyCostHistoryRead* party_history=nullptr);
// 真执行重验原资格、当前writer；第一正prefix必须已在 from 真实付费，后续片只结累计。
Status apply_exact_personal_prefix(CommonWrite&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const PreparedMovementPrefix&,std::vector<BoundSpatialLocationWrite>* actual_writes=nullptr,ExactArithmeticBudget budget={});
// Hold和改率仅重接活动映射，保未消费basis；真实改道另走continuous首段。
Status hold_movement_prefix(MovementExactState&,const StableId&,const WorldMoment&,const SourceRef&,ExactArithmeticBudget budget={});
Status resume_movement_prefix(MovementExactState&,const StableId&,const WorldMoment&,const SourceRef&,ExactArithmeticBudget budget={});
Status rebind_movement_rate(MovementExactState&,const StableId&,const WorldMoment&,Integer new_budget,const SourceRef&,ExactArithmeticBudget budget={});
Outcome<std::optional<ExactMovementContact>> predict_exact_terrain_contact(const MovementExactState&,const StableId& first_prefix,const StableId& second_prefix,const WorldMoment& current,const ContactEntrances& entrances={},ExactArithmeticBudget budget={});
Outcome<std::optional<ExactMovementContact>> predict_exact_road_contact(const MovementExactState&,const TravelCatalog&,const StableId& first_prefix,const StableId& second_prefix,const WorldMoment& current,ExactArithmeticBudget budget={});
// 时间只映原完整ContactHit，绝不在其量化时间重新求锚或后缀。
Outcome<ExactMovementContact> map_movement_contact(const StableId&,const ContactHit&,const WorldMoment&,const HalfDayDuration&,const StableId&,const StableId&,ExactArithmeticBudget budget={});
Outcome<CanonicalFraction> read_exact_contact_fraction(const ExactMovementContact&,ExactArithmeticBudget budget={});
Outcome<WorldMoment> read_exact_contact_mapped_moment(const ExactMovementContact&,ExactArithmeticBudget budget={});
// 以真实执行匹配原hit端点；群体key仅几何身份，不伪Common Actor。
Outcome<MovementAnchor> read_exact_contact_endpoint_anchor(const MovementExactState&,const ExactMovementContact&,const StableId& prefix_id,ExactArithmeticBudget budget={});
Outcome<MovementAnchor> read_exact_contact_actor_anchor(const MovementExactState&,const ExactMovementContact&,const StableId& actor_id,ExactArithmeticBudget budget={});
struct GroupMovementExecutionState;
Status validate_exact_contact_geometry_source(const MovementExactState&,const TravelCatalog&,const ExactMovementContact&,ExactArithmeticBudget budget={},const GroupMovementExecutionState* groups=nullptr);
Status install_exact_contact_source(CommonWrite&,MovementExactState&,const ExactMovementContact&,ExactArithmeticBudget budget={});
Outcome<StableId> apply_exact_personal_contact_side(CommonWrite&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const StableId& contact_id,const StableId& prefix_id,const StableId& encounter_id,bool interactive,std::vector<BoundSpatialLocationWrite>&,ExactArithmeticBudget budget={},const PersonalMovementFundingRead* funding=nullptr,const CurrentMomentRead* clock=nullptr);
Status apply_exact_contact_truncation(CommonWrite&,TravelState&,MovementExactState&,const TravelCatalog&,const TravelServices&,const ExactMovementContact&,const StableId& encounter_id,bool interactive,std::vector<BoundSpatialLocationWrite>* actual_writes=nullptr,ExactArithmeticBudget budget={},const PersonalMovementFundingRead* funding=nullptr,const CurrentMomentRead* clock=nullptr);
// 原关闭决定唯一余预算处置；真实结果源由调用者实际业务producer给出。
Status close_movement_unit(MovementExactState&,const StableId&,MovementCloseKind,const SourceRef&,const WorldMoment&,ExactArithmeticBudget budget={});
// 实际unit关闭后生成一次用力源，返回S4须在同候选消费的source ID。
Outcome<StableId> finalize_exact_movement_unit_source(CommonWrite&,MovementExactState&,const StableId& prefix_id,MovementCloseKind,const SourceRef&,const WorldMoment&,ExactArithmeticBudget budget={});
Outcome<bool> read_unstarted_held_contact_close(const MovementExactState&,const MovementUnitState&,const SourceRef&,ExactArithmeticBudget budget={});
Outcome<StableId> finalize_exact_personal_unit(CommonWrite&,TravelState&,MovementExactState&,const TravelCatalog&,const StableId&,MovementCloseKind,const SourceRef&,const WorldMoment&,std::vector<BoundSpatialLocationWrite>* actual_writes=nullptr,ExactArithmeticBudget budget={});
Outcome<TravelExertionSource> read_verified_travel_exertion(const MovementExactState&,const StableId&,ExactArithmeticBudget budget={});
// Army 仍由原 S2 名册/Formation/真实采用源证明，S1 不从 ID 存在授载运。
class ArmySpatialBindingSourceRead {
public:
    virtual ~ArmySpatialBindingSourceRead()=default;
    virtual Status validate_actor_army_binding_source(const StableId& actor_id,const ArmyCarrierRef&,const SourceRef& actual_binding_source)const=0;
};
Status validate_movement_exact_values(const MovementExactState&,ExactArithmeticBudget budget={});
Status validate_movement_exact_world(const CommonRead&,const TravelState&,const MovementExactState&,ExactArithmeticBudget budget={},const ArmySpatialBindingSourceRead* army_sources=nullptr);
Status visit_movement_exact_checkpoint(CodecVisitor&,MovementExactState&,Integer selected_travel_version=4);
} // root负责外层rollback/publish和阶段登记，S1不推进Common.tick。
