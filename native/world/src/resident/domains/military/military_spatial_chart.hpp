// S2 chart只作精确坐标转换；有理叶仍唯一WorldMoment数值实现。
#pragma once
#include "resident/domains/travel/spatial_carrier.hpp"
#include "battle_model.hpp"
namespace hanmo::resident {
struct SignedSpatialRational { Integer sign=0; CanonicalFraction magnitude; };
struct MilitarySpatialPoint { SignedSpatialRational x,y; };
struct MilitarySpatialTranslation { SignedSpatialRational b1,b2; };
struct MilitarySpatialMatrix { SignedSpatialRational a11,a12,a21,a22; };
struct MilitarySpatialSegment { MilitarySpatialPoint from,to; };
struct MilitarySpatialPortal { StableId portal_id,world_entry_exit_source_kind,world_boundary_ref; MilitarySpatialSegment local_boundary_segment; std::vector<StableId> allowed_carrier_kinds; MilitarySpatialMatrix orientation_mapping; StableId connector_path_ref; };
struct MilitarySpatialChart2 { StableId id,session_ref,geometry_definition_ref,world_geometry_ref,world_length_unit_ref,local_axis_unit_ref; MilitarySpatialMatrix matrix; MilitarySpatialTranslation translation; std::vector<MilitarySpatialPoint> domain_polygon; std::vector<MilitarySpatialPortal> portal_rows; SourceRef creation_source_ref; Integer rule_version=2; };
struct MilitaryEntryConnectorDefinition { StableId id,chart_ref,portal_id; std::vector<MilitarySpatialPoint> path; std::vector<Integer> entered_cell_costs; SourceRef original_path_source; };
struct MilitaryConnectorWork { StableId connector_ref; std::vector<CanonicalFraction> segment_work; CanonicalFraction total; };
// 作者有限选择表只读真实route tangent；没有匹配行时拒绝，不默认单位矩阵。
struct MilitaryOrientationHalfspace {MilitarySpatialPoint normal;SignedSpatialRational maximum;};
struct MilitaryEncounterOrientation {StableId id;std::vector<MilitaryOrientationHalfspace> tangent_region;MilitarySpatialMatrix matrix;MilitarySpatialPoint local_entry;};
struct LockedArenaChartDefinition {MilitarySpatialMatrix matrix;MilitarySpatialTranslation translation;};
struct EncounterChartDefinition {std::vector<MilitaryEncounterOrientation> orientation_choices;};
struct MilitarySpatialGeometryDefinition2 {StableId id,world_geometry_ref,world_length_unit_ref,local_axis_unit_ref;std::vector<MilitarySpatialPoint> domain_polygon;std::vector<MilitarySpatialPortal> portal_rows;std::variant<LockedArenaChartDefinition,EncounterChartDefinition> author_mode;SourceRef actual_definition_source;};
Outcome<MilitarySpatialChart2> make_military_spatial_chart(const MilitarySpatialGeometryDefinition2&,const StableId& chart_id,const StableId& session_id,const SourceRef& actual_entry_source,const MilitarySpatialPoint& actual_world_entry,const MilitarySpatialPoint& actual_route_tangent,const StableId& actual_portal_id,ExactArithmeticBudget budget={});
Outcome<MilitarySpatialPoint> military_spatial_interpolate(const MilitarySpatialPoint&,const MilitarySpatialPoint&,const CanonicalFraction&,ExactArithmeticBudget budget={});
Outcome<MilitarySpatialPoint> military_spatial_difference(const MilitarySpatialPoint&,const MilitarySpatialPoint&,ExactArithmeticBudget budget={});
Outcome<CanonicalFraction> military_axial_length(const MilitarySpatialPoint&,const MilitarySpatialPoint&,ExactArithmeticBudget budget={});
Status visit_military_spatial_definition2(CodecVisitor&,MilitarySpatialGeometryDefinition2&);
Status validate_military_spatial_geometry_definition(const MilitarySpatialGeometryDefinition2&,ExactArithmeticBudget budget={});
struct MilitaryReceiverHalfspace { MilitarySpatialPoint normal; SignedSpatialRational maximum; };
struct MilitaryContactParameterWindow { CanonicalFraction first,last; };
// The sole finite convex footprint check is shared by author admission and SAT.
// Vertices are authored offsets, without a writable body center.
Status validate_military_convex_footprint(const std::vector<MilitarySpatialPoint>&,
    ExactArithmeticBudget budget={});
// Original axial heading numbers 0..5: successive +1 is (q,r)->(-r,q+r).
// This exact change of axes preserves the authored offsets and uses no angle,
// trigonometry, normalization or floating-point footprint approximation.
Outcome<std::vector<MilitarySpatialPoint>> orient_military_body_footprint(
    const std::vector<MilitarySpatialPoint>&,Integer actual_heading,
    ExactArithmeticBudget budget={});
// Pure S2 3.2/3.4 geometry over one common, positive source-valid time piece.
// Each footprint is already in the same actual chart and frozen heading. Its
// vertices are offsets from its center; neither polygon rotates in this piece.
// The caller retains the real shape/chart/heading/motion sources and decides
// any authorized entry co-location exception. No collision fact is made here.
struct MilitaryConvexBodyContactRead {
    std::optional<MilitaryContactParameterWindow> closed_contact;
    bool initially_penetrating=false;
    // The infimum of a nonempty strict interior-overlap interval in [0,1].
    // Parallel boundary contact and a single-point grazing touch have none.
    // An endpoint-only closed contact is retained above, without claiming
    // intrusion beyond this piece's actual source-valid interval.
    std::optional<CanonicalFraction> first_inward_contact;
};
Outcome<MilitaryConvexBodyContactRead> intersect_military_convex_body_motion(
    const std::vector<MilitarySpatialPoint>& first_footprint,
    const MilitarySpatialSegment& first_centers,
    const std::vector<MilitarySpatialPoint>& second_footprint,
    const MilitarySpatialSegment& second_centers,ExactArithmeticBudget budget={});
// Exact containment of the full translating footprint in one original convex
// chart domain. This is geometry only: an actual portal/entry source decides
// whether crossing that boundary is authorized. No invented exterior cells.
struct MilitaryConvexDomainMotionRead {
    std::optional<MilitaryContactParameterWindow> closed_inside_window;
    bool initially_outside=false;
    std::optional<CanonicalFraction> first_outward_contact;
};
Outcome<MilitaryConvexDomainMotionRead> intersect_military_convex_domain_motion(
    const std::vector<MilitarySpatialPoint>& body_footprint,
    const MilitarySpatialSegment& body_centers,
    const std::vector<MilitarySpatialPoint>& actual_domain,ExactArithmeticBudget budget={});
// 相对两端和有限半空间的完整交集；不是末位置采样，也不冒充跨未知通路的NoContact。
Outcome<std::optional<MilitaryContactParameterWindow>> intersect_military_receiver_window(const MilitarySpatialPoint& relative_from,const MilitarySpatialPoint& relative_to,const std::vector<MilitaryReceiverHalfspace>&,ExactArithmeticBudget budget={});
// The original six direction numbers are selected by exact axial quadratic
// distance. A bisector retains the earlier original direction, independent of
// actor IDs and iteration over world records. Coincident bodies have no ray.
Outcome<Integer> military_spatial_direction(const MilitarySpatialPoint& from,
    const MilitarySpatialPoint& to,ExactArithmeticBudget budget={});
struct MilitaryHexSupercoverHit {HexPoint cell;MilitaryContactParameterWindow parameter_window;};
struct MilitaryHexSupercoverRead {
    std::vector<MilitaryHexSupercoverHit> intersections;
    bool complete_board_coverage=false;
};
// Every closed hex intersecting the actual rational segment is returned, even
// a single-point tangency. This is a geometric witness, not a permission/LOS
// result: the original battlefield owner still supplies blocking/height rules.
Outcome<MilitaryHexSupercoverRead> military_hex_supercover(
    const MilitarySpatialPoint& from,const MilitarySpatialPoint& to,
    const std::vector<HexPoint>& actual_board_cells,ExactArithmeticBudget budget={});
Outcome<bool> military_spatial_point_inside(const MilitarySpatialChart2&,const MilitarySpatialPoint&,ExactArithmeticBudget budget={});
Outcome<SignedSpatialRational> spatial_integer(Integer,ExactArithmeticBudget budget={});
Outcome<SignedSpatialRational> spatial_binary64(double,ExactArithmeticBudget budget={});
Outcome<MilitarySpatialPoint> military_chart_to_world(const MilitarySpatialChart2&,const MilitarySpatialPoint&,ExactArithmeticBudget budget={});
// Linear part only, for an actual offset/direction in this chart. Translation
// belongs to the center and must never be added a second time to a footprint.
Outcome<MilitarySpatialPoint> military_chart_vector_to_world(const MilitarySpatialChart2&,const MilitarySpatialPoint&,ExactArithmeticBudget budget={});
Outcome<MilitarySpatialPoint> military_chart_to_local(const MilitarySpatialChart2&,const MilitarySpatialPoint&,ExactArithmeticBudget budget={});
Outcome<MilitarySpatialPoint> military_world_point(const MovementAnchor&,ExactArithmeticBudget budget={});
Outcome<MilitaryConnectorWork> military_connector_work(const MilitarySpatialChart2&,const MilitaryEntryConnectorDefinition&,const MilitarySpatialPoint& actual_entry,ExactArithmeticBudget budget={});
Outcome<bool> military_spatial_points_equal(const MilitarySpatialPoint&,const MilitarySpatialPoint&,ExactArithmeticBudget budget={});
Outcome<bool> military_point_on_portal(const MilitarySpatialPortal&,const MilitarySpatialPoint&,ExactArithmeticBudget budget={});
Status validate_military_spatial_chart(const MilitarySpatialChart2&,ExactArithmeticBudget budget={});
}
