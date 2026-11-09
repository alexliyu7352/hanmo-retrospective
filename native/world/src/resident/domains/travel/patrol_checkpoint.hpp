// S1 PATROL-ARRIVAL-1 / DutyD§5.2: actual motion only, after contact installation.
#pragma once
#include "movement_result_read.hpp"
#include "spatial_carrier.hpp"
#include "../../kernel/exact_phase_graph.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// S3 owns the accepted route and its checkpoint definition; S1 owns arrival.
// A checkpoint reference is an original typed source, never caller coordinates.
struct PatrolRouteExecutionRead {
    MomentOwnerSource original_binding_source;
    StableId journey_id;
    SpatialCarrierRef actor_or_group_ref;
    std::vector<SourceRef> ordered_checkpoint_refs;
};
class PatrolCheckpointOriginalRead {
public:
    virtual ~PatrolCheckpointOriginalRead()=default;
    virtual Outcome<std::vector<StableId>> active_patrol_journeys(const WorldMoment&)const=0;
    virtual Outcome<PatrolRouteExecutionRead> read_patrol_route_execution(const StableId& journey_id,const WorldMoment&)const=0;
    virtual Outcome<MovementAnchor> read_patrol_checkpoint_anchor(const PatrolRouteExecutionRead&,const SourceRef&)const=0;
};
// Arrival schema2 has the original six fields, with exact time and a real final
// motion source. The witness is retained only for a real checkpoint consumer.
Outcome<const PatrolCheckpointArrival2*> read_patrol_checkpoint(const CommonStore&,const MovementExactState&,const PatrolCheckpointOriginalRead&,const SourceRef&,Integer schema,ExactArithmeticBudget budget={});
struct OrderedPatrolCheckpointRead {
    MomentOwnerSource route_binding_source;
    std::vector<SourceRef> actual_arrival_sources;
    Integer passed_checkpoints=0;
    bool complete=false;
};
Outcome<OrderedPatrolCheckpointRead> read_ordered_patrol_checkpoints(const CommonStore&,const MovementExactState&,const PatrolCheckpointOriginalRead&,const StableId& journey_id,const WorldMoment& through,ExactArithmeticBudget budget={});
Outcome<std::vector<ExactEventPrediction>> predict_patrol_checkpoint_arrivals(const CommonStore&,const MovementExactState&,const PatrolCheckpointOriginalRead&,const WorldMoment&,const WorldMoment&,ExactArithmeticBudget budget={});
Outcome<ExactEventApplied> adopt_patrol_checkpoint_arrival_in_candidate(CommonStore&,MovementExactState&,const PatrolCheckpointOriginalRead&,const ExactEventPrediction&,ExactArithmeticBudget budget={});
Status validate_patrol_checkpoint_sources(const CommonStore&,const MovementExactState&,const PatrolCheckpointOriginalRead&,const WorldMoment&,ExactArithmeticBudget budget={});
Status visit_patrol_checkpoint_sources(CodecVisitor&,MovementExactState&);
Status decode_patrol_checkpoint_sources(const BoundaryValue&,MovementExactState&);
Status emit_patrol_checkpoint_sources(ProjectionWriter&,const MovementExactState&);
Outcome<BoundaryValue> export_patrol_checkpoint_sources(const MovementExactState&);
} // This source is not a Work result and never closes a Duty window itself.
