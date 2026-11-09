// S2局部唯一连续位置。原名册/船体/成本由具名来源提供，几何不持第二人物位置。
#pragma once
#include <string_view>
#include "military_spatial_chart.hpp"
#include "battle_model.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
struct MilitaryLocalUnitRef { StableId unit_id; };
struct MilitaryLocalVesselRef { StableId vessel_id; };
using MilitaryLocalBodyRef=std::variant<MilitaryLocalUnitRef,MilitaryLocalVesselRef>;
struct LocalMotionSegmentRef { StableId basis_ref; Integer segment_ordinal=0; };
struct LocalHexNeighbor { LocalMotionSegmentRef root_edge_ref; HexPoint from_vertex,to_vertex; Integer directional_cost=0; };
struct ReverseLocalMotionAtPrefix { SourceRef accepted_intent_source_ref; };
struct RepriceLocalMotionSuffix { SourceRef cost_change_source_ref,continuing_intent_source_ref; };
using LocalMotionChange=std::variant<ReverseLocalMotionAtPrefix,RepriceLocalMotionSuffix>;
struct LocalPartialEdgeContinuation {
    LocalMotionSegmentRef root_edge_ref,parent_segment_ref; StableId parent_prefix_close_ref;
    LocalMotionChange change; CanonicalFraction root_u_start,root_u_end; Integer directional_cost=0;
    SourceRef topology_anchor_source_ref;
};
struct LocalEntryConnector {
    SourceRef admission_source_ref; StableId chart_ref,portal_id,connector_path_ref; Integer piece_ordinal=0;
    MilitarySpatialPoint from_qr,to_qr; SourceRef topology_anchor_source_ref;
};
struct LocalExitConnector {
    SourceRef exit_source_ref; StableId chart_ref,portal_id,connector_path_ref; Integer piece_ordinal=0;
    MilitarySpatialPoint from_qr,to_qr; SourceRef topology_anchor_source_ref;
};
using LocalMotionGeometry=std::variant<LocalHexNeighbor,LocalPartialEdgeContinuation,LocalEntryConnector,LocalExitConnector>;
struct LocalMotionSegment {
    Integer ordinal=0; LocalMotionGeometry geometry; WorkUnits required_edge_work;
    SourceRef terrain_cost_basis_ref; StableId original_cost_unit_ref;
};
struct LocalHexMotionBasis2 {
    StableId id,session_ref; SpatialCarrierRef carrier_ref; MilitaryLocalBodyRef local_body_ref;
    MovementExecutionRef execution_ref; SourceRef intent_source_ref;
    StableId geometry_definition_ref,map_version,terrain_rule_ref; SourceRef unit_or_vessel_capability_source_ref;
    StableId chart_ref; WorldMoment started_at; std::vector<LocalMotionSegment> segments;
    MilitarySpatialPoint route_terminal; SourceRef original_unit_cost_source_ref;
};
struct LocalMotionPrefixClose2 {
    StableId id,session_ref; MovementExecutionRef execution_ref; MilitaryLocalBodyRef local_body_ref;
    LocalMotionSegmentRef root_edge_ref; StableId parent_basis_ref; Integer parent_segment_ordinal=0;
    CanonicalFraction parent_u_start,parent_u_end; WorkUnits parent_required_work,credited_work_at_close;
    CanonicalFraction closed_root_u; WorldMoment closed_at; SourceRef topology_anchor_source_ref;
    std::vector<SourceRef> reservation_source_refs; StableId original_cost_unit_ref;
    std::vector<SourceRef> actual_work_cost_source_refs; SourceRef change_source_ref;
};
enum class LocalMotionPhase { moving,held,blocked,closed };
struct LocalMotionCursor2 {
    MovementExecutionRef execution_ref; MilitaryLocalBodyRef local_body_ref; StableId basis_ref;
    Integer segment_ordinal=0; WorkUnits credited_edge_work; WorldMoment last_settled_at;
    SourceRef rate_source_ref; CanonicalFraction rate_work_per_half_day;
    LocalMotionPhase phase=LocalMotionPhase::held; SourceRef phase_source_ref;
    std::vector<SourceRef> reserved_topology_refs; std::vector<StableId> closed_prefix_sources;
};
// Explicit, not-yet-admitted dynamic_military/3 successor of the SAME
// local_motion_prefix_closes owner. Variant 0 is the unchanged original /2 row.
struct BodyMotionBoundaryInput3 {
    MilitaryLocalBodyRef local_body_ref;MovementExecutionRef execution_ref;
    StableId basis_ref;Integer segment_ordinal=0;
    MomentOwnerSource prior_phase_source,prior_motion_source;
    WorkUnits credited_work_before;WorldMoment from_at;
    MomentOwnerSource rate_source_ref;CanonicalFraction rate_work_per_half_day;
    LocalMotionPhase phase_before=LocalMotionPhase::held;
    std::vector<MomentOwnerSource> reserved_topology_refs;
    Integer heading=0;MomentOwnerSource heading_source_ref,shape_source,spatial_binding_ref;
    std::vector<MomentOwnerSource> qualification_source_refs;
};
struct LocalMotionEdgeCompletion3 {
    StableId id,session_ref;Integer generation=0;
    CommonActualFactPrefix actual_before_common_prefix;BodyMotionBoundaryInput3 input;
    WorldMoment completed_at;WorkUnits credited_work_at_completion;
    std::vector<MomentOwnerSource> actual_work_cost_source_refs,reservation_release_sources;
    MomentOwnerSource arrived_topology_source;
};
struct LocalMotionBlockedCell3 {
    MomentOwnerSource battlefield_ref;StableId map_version;HexPoint cell;
    MomentOwnerSource terrain_cost_source,capability_source;
};
struct LocalMotionChartBoundary3 {StableId chart_ref;MomentOwnerSource geometry_definition_source;};
struct LocalMotionForbiddenRegion3 {MomentOwnerSource definition_source;Integer region_ordinal=0;MomentOwnerSource capability_source;};
using LocalMotionObstacleInput3=std::variant<LocalMotionBlockedCell3,LocalMotionChartBoundary3,LocalMotionForbiddenRegion3>;
struct LocalMotionContactBody3 {Integer body_ordinal=0;};
struct LocalMotionContactObstacle3 {Integer obstacle_ordinal=0;};
using LocalMotionContactOther3=std::variant<LocalMotionContactBody3,LocalMotionContactObstacle3>;
enum class LocalMotionContactConsequence3 {friendly_bodies_blocked,terrain_blocked,enemy_contact};
struct LocalMotionSimultaneousContact3 {
    Integer first_body_ordinal=0;LocalMotionContactOther3 other;
    MilitaryContactParameterWindow closed_parameter_window;CanonicalFraction first_inward_parameter;
    MomentOwnerSource original_relation_source;
    LocalMotionContactConsequence3 consequence_kind=LocalMotionContactConsequence3::friendly_bodies_blocked;
};
struct LocalMotionContact3 {
    StableId id,session_ref;Integer generation=0;CommonActualFactPrefix actual_before_common_prefix;
    MomentSpan prediction_window;WorldMoment occurred_at;
    std::vector<BodyMotionBoundaryInput3> body_inputs;
    std::vector<LocalMotionObstacleInput3> obstacle_inputs;
    std::vector<LocalMotionSimultaneousContact3> simultaneous_contacts;
    std::vector<MomentOwnerSource> actual_work_cost_source_refs;
};
using LocalMotionBoundarySource=std::variant<LocalMotionPrefixClose2,LocalMotionEdgeCompletion3,LocalMotionContact3>;
const StableId& local_motion_boundary_id(const LocalMotionBoundarySource&);
class MilitaryLocalBoundaryValidationRead {
public:
    virtual ~MilitaryLocalBoundaryValidationRead()=default;
    // True Common/payload provenance, original complete before-input set,
    // exact first gate, actual work/resource adoption and every consequence.
    virtual Status validate_original_local_boundary(const LocalMotionBoundarySource&)const=0;
};
struct LocalMotionEndpointRead {
    MilitarySpatialPoint from,to,current; CanonicalFraction segment_fraction,root_u;
    std::optional<LocalMotionSegmentRef> root_edge_ref; SourceRef topology_anchor_source_ref;
};
// 只有真正军事意图、能力、通行、占边来源可实现此口；无缺省成功或自由caller工量。
class MilitaryLocalMotionOriginalRead {
public:
    virtual ~MilitaryLocalMotionOriginalRead()=default;
    virtual Status validate_original_local_basis(const LocalHexMotionBasis2&)const=0;
    virtual Outcome<Integer> read_original_segment_cost(const LocalHexMotionBasis2&,const LocalMotionSegment&)const=0;
    virtual Status validate_original_local_cursor(const LocalHexMotionBasis2&,const LocalMotionCursor2&)const=0;
    virtual Status validate_original_prefix_change(const LocalHexMotionBasis2&,const LocalMotionPrefixClose2&)const=0;
};
struct MilitaryLocalMotionState {
    OrderedTable<LocalHexMotionBasis2> bases; OrderedTable<LocalMotionCursor2> cursors;
    OrderedTable<LocalMotionBoundarySource> prefix_closes;
};
const LocalMotionPrefixClose2* find_original_local_motion_prefix_close(const MilitaryLocalMotionState&,const StableId&);
StableId military_local_body_key(const MilitaryLocalBodyRef&);
bool military_local_body_key_matches(std::string_view,const MilitaryLocalBodyRef&);
Outcome<LocalMotionEndpointRead> read_local_motion_position(const MilitaryLocalMotionState&,const LocalMotionCursor2&,ExactArithmeticBudget budget={});
Status validate_local_motion_basis(const MilitaryLocalMotionState&,const LocalHexMotionBasis2&,const MilitaryLocalMotionOriginalRead&,ExactArithmeticBudget budget={});
Status validate_local_motion_prefix_close(const MilitaryLocalMotionState&,const LocalMotionPrefixClose2&,const MilitaryLocalMotionOriginalRead&,ExactArithmeticBudget budget={});
Status validate_military_local_motion(const MilitaryLocalMotionState&,const MilitaryLocalMotionOriginalRead&,const WorldMoment&,ExactArithmeticBudget budget={},const MilitaryLocalBoundaryValidationRead* actual_boundaries=nullptr,Integer selected_local_schema=2);
Outcome<std::optional<WorldMoment>> predict_local_motion_edge_due(const MilitaryLocalMotionState&,const LocalMotionCursor2&,ExactArithmeticBudget budget={});
// Borrowed current source plus a derived future linear piece. This is a
// prediction, never evidence that the future span has already been served or
// is free of collisions/qualification changes. The shared selector must also
// intersect all other original event bounds before adopting any prefix.
struct LocalMotionLinearPrediction {
    const LocalHexMotionBasis2* actual_basis=nullptr;
    const LocalMotionCursor2* actual_cursor=nullptr;
    MomentSpan window;
    MilitarySpatialSegment centers;
    std::optional<WorldMoment> actual_edge_due;
};
Outcome<LocalMotionLinearPrediction> predict_actual_local_motion_piece(
    const MilitaryLocalMotionState&,const StableId& body_key,
    const MilitaryLocalMotionOriginalRead&,const WorldMoment& actual_now,
    const WorldMoment& requested_upper,ExactArithmeticBudget budget={});
// Sole exact prefix quantity calculation. It does not prove service or grant
// movement; hot advance and immutable boundary verification share this leaf.
Outcome<WorkUnits> measure_local_motion_prefix(const WorkUnits& before,const WorkUnits& required,
    LocalMotionPhase,const CanonicalFraction& actual_rate,const MomentSpan&,ExactArithmeticBudget={});
// 只写已许可span的唯一cursor；调用者先裁到所有实际碰撞/事件的最早M。
// Closed retains the original completion cut and cannot receive another span.
Status advance_local_motion_prefix_in_candidate(MilitaryLocalMotionState&,const StableId& body_key,const MomentSpan&,const MilitaryLocalMotionOriginalRead&,ExactArithmeticBudget budget={});
// 真实半边改向/改价：旧credit只留close；新credit必0，同root且保原cost unit和拓扑占用。
Outcome<LocalMotionPrefixClose2> close_local_motion_prefix_for_change(const MilitaryLocalMotionState&,const LocalMotionCursor2&,const StableId& close_id,const SourceRef& actual_change,const SourceRef& topology_anchor,const std::vector<SourceRef>& actual_cost_sources,const MilitaryLocalMotionOriginalRead&,ExactArithmeticBudget budget={});
Outcome<LocalMotionSegment> make_partial_local_motion_segment(const MilitaryLocalMotionState&,const LocalMotionPrefixClose2&,const LocalMotionChange&,const CanonicalFraction& endpoint,Integer actual_directional_cost,const SourceRef& terrain_cost_source,ExactArithmeticBudget budget={});
// The original source owner prepares the successor route/rate and allocates its
// two IDs in this same private candidate. This finite adoption writes only the
// existing three owners. It derives the close from the actual old cursor and
// authenticates all new values through the mandatory original readers.
// Command replay belongs to the original outer command before this fresh write.
struct PreparedLocalMotionContinuation {
    StableId prefix_close_id;
    LocalHexMotionBasis2 successor;
    LocalMotionCursor2 successor_cursor;
    std::vector<SourceRef> actual_work_cost_sources;
};
Status adopt_local_motion_partial_continuation_in_candidate(MilitaryLocalMotionState&,
    const StableId& body_key,PreparedLocalMotionContinuation&&,
    const MilitaryLocalMotionOriginalRead&,const CurrentMomentRead&,
    ExactArithmeticBudget budget={});
// 相同M先读旧占位：离开者仍占旧顶点；同目标全阻挡，没有ID获胜者。
// Only a real adjacent HexNeighbor from the body's existing from reservation
// is proposed here. Half-edge reversals preserve their original reservations;
// connector admission uses its original source. The caller must authenticate
// the complete retained reservation/source set before this pure resolution.
struct LocalTopologyReservation { MilitaryLocalBodyRef body; std::vector<HexPoint> reserved_vertices; std::vector<SourceRef> actual_sources; };
struct LocalEdgeProposal { MilitaryLocalBodyRef body; HexPoint from,to; SourceRef actual_intent_source; };
struct LocalEdgeAdmission { MilitaryLocalBodyRef body; bool admitted=false; std::vector<MilitaryLocalBodyRef> blocking_bodies; };
Outcome<std::vector<LocalEdgeAdmission>> resolve_local_edge_admissions(const std::vector<LocalTopologyReservation>&,const std::vector<LocalEdgeProposal>&);
Status visit_military_local_motion_payloads(CodecVisitor&,MilitaryLocalMotionState&,Integer selected_local_schema=2);
} // 纯恢复只验证/重建派生P，不推进时间或开业务事件。
