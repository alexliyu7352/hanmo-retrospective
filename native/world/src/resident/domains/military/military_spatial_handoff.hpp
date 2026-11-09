#pragma once
#include "military_local_motion.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct MilitaryRouteCorrespondence2 {
    Integer old_segment_ordinal=0,new_segment_ordinal=0;
    CanonicalFraction old_parameter_from,old_parameter_to,new_parameter_from,new_parameter_to;
    MilitarySpatialPoint old_from,old_to,new_from,new_to;
    StableId chart_ref; SourceRef original_parameter_source;
};
struct MilitarySpatialHandoff2 {
    StableId id; SpatialCarrierRef carrier_ref; SpatialWriterBinding from_binding,to_binding;
    MovementExecutionRef old_execution_ref; std::optional<MovementExecutionRef> new_execution_ref;
    WorldMoment at; SourceRef entry_or_exit_source_ref; StableId chart_ref,portal_id,old_basis_ref;
    CanonicalFraction old_cursor; MilitarySpatialPoint new_anchor;
    std::vector<MilitaryRouteCorrespondence2> remaining_route_correspondence;
    std::vector<SourceRef> cost_and_work_disposition_refs; SourceRef closed_writer_source_ref;
};
struct MilitarySpatialHandoffState {
    OrderedTable<MilitarySpatialChart2> charts; OrderedTable<MilitarySpatialHandoff2> handoffs;
};
// 原SessionInitialized/真实退出源只读证明；不能由UI/spawn或winner字段合成。
struct ActualMilitaryEntryRead {
    SourceRef entry_source; StableId session_ref; Integer generation=0; SpatialCarrierRef carrier;
    SourceRef world_contact_source; StableId world_prefix_ref,portal_id,geometry_definition_ref;
    WorldMoment occurred_at; std::vector<MilitaryLocalBodyRef> actual_bodies;
    std::vector<StableId> actual_named_actor_ids;
};
struct ActualMilitaryExitRead {
    SourceRef exit_source; StableId session_ref; Integer generation=0; SpatialCarrierRef carrier;
    MilitaryLocalBodyRef local_body; StableId chart_ref,portal_id; WorldMoment occurred_at;
    SourceRef terminal_or_actual_exit_source;
};
class MilitarySpatialOriginalSourceRead {
public:
    virtual ~MilitarySpatialOriginalSourceRead()=default;
    virtual Outcome<ActualMilitaryEntryRead> read_actual_military_entry(const SourceRef&)const=0;
    virtual Outcome<ActualMilitaryExitRead> read_actual_military_exit(const SourceRef&)const=0;
    virtual Outcome<MilitarySpatialGeometryDefinition2> read_original_spatial_definition(const StableId&)const=0;
    virtual Status validate_original_world_continuation(const MilitarySpatialHandoff2&,const MovementPrefixState&)const=0;
};
// 世界接触已经精确截断、旧unit已真实关闭后才转移唯一binding，不二次收费/给工。
Outcome<StableId> enter_military_spatial_from_contact_in_candidate(CommonStore&,MovementExactState&,MilitarySpatialHandoffState&,const MilitarySpatialOriginalSourceRead&,const SourceRef& actual_entry,ExactArithmeticBudget budget={});
// 新世界prefix已由S1按真实余程prepare，仍无当前写权；同候选关闭本地并授回S1。
Outcome<StableId> leave_military_spatial_in_candidate(CommonStore&,MovementExactState&,MilitaryLocalMotionState&,MilitarySpatialHandoffState&,const MilitarySpatialOriginalSourceRead&,const SourceRef& actual_exit,const StableId& prepared_world_prefix,ExactArithmeticBudget budget={});
Status validate_military_spatial_handoffs(const CommonStore&,const MovementExactState&,const MilitaryLocalMotionState&,const MilitarySpatialHandoffState&,const MilitarySpatialOriginalSourceRead&,ExactArithmeticBudget budget={});
struct MilitaryBodyWorldPosition {
    MilitaryLocalBodyRef body; MilitarySpatialPoint world_position;
    StableId actual_basis_ref; WorldMoment settled_at;
    std::vector<SourceRef> original_sources;
};
struct MilitaryCarrierWorldFootprint {
    SpatialCarrierRef carrier; StableId session_ref,chart_ref;
    std::vector<MilitaryBodyWorldPosition> actual_bodies;
};
// Preserve the actual union of local bodies. A compound carrier is never reduced
// to its leader, centroid, or a requirement that every body share one point.
Outcome<MilitaryCarrierWorldFootprint> read_military_carrier_world_footprint(const MovementExactState&,const MilitaryLocalMotionState&,const MilitarySpatialHandoffState&,const SpatialCarrierRef&,ExactArithmeticBudget budget={});
// Compatibility scalar is only meaningful for an actual single-body carrier.
Outcome<MilitarySpatialPoint> read_military_carrier_world_position(const MovementExactState&,const MilitaryLocalMotionState&,const MilitarySpatialHandoffState&,const SpatialCarrierRef&,ExactArithmeticBudget budget={});
Status visit_military_spatial_handoff_payloads(CodecVisitor&,MilitarySpatialHandoffState&);
}
