// S3 consumes the original S2 Duty period and S1 checkpoint result, never a
// projected Action finish, current unit count, or planned route duration.
#pragma once
#include "work_task_sources.hpp"
#include "resident/domains/military/duty/military_duty_execution_read.hpp"
#include "resident/domains/travel/patrol_checkpoint.hpp"
namespace hanmo::resident {
struct GovernanceWorkHistoryContext;
struct PatrolWorkSources {
 const CommonStore& common;const TaskState& tasks;const WorkTaskRead& definitions;
 const MilitaryDutyState& duties;const MilitaryDutyContext& duty_context;
 const MovementExactState& movement;const PatrolCheckpointOriginalRead& checkpoints;
 const CurrentMomentRead& clock;
 const GovernanceWorkHistoryContext* work_history=nullptr;
};
// Pure original route adapter. History stays readable after Duty/Work closes;
// this owns no route progress or movement state.
class NativePatrolCheckpointOriginalRead final:public PatrolCheckpointOriginalRead {
 const CommonStore& common_;const TaskState& tasks_;const WorkTaskRead& definitions_;
 const MilitaryDutyState& duties_;const MilitaryDutyContext& duty_context_;
 const MovementExactState& movement_;const CommonCatalog& common_catalog_;const TravelCatalog& travel_catalog_;const CurrentMomentRead& clock_;
 const GovernanceWorkHistoryContext* work_history_=nullptr;
 PatrolWorkSources sources()const{return {common_,tasks_,definitions_,duties_,duty_context_,movement_,*this,clock_,work_history_};}
public:
 NativePatrolCheckpointOriginalRead(const CommonStore&c,const TaskState&t,const WorkTaskRead&d,const MilitaryDutyState&m,const MilitaryDutyContext&context,const MovementExactState&movement,const CommonCatalog&cc,const TravelCatalog&tc,const CurrentMomentRead&clock,const GovernanceWorkHistoryContext* history=nullptr)
  :common_(c),tasks_(t),definitions_(d),duties_(m),duty_context_(context),movement_(movement),common_catalog_(cc),travel_catalog_(tc),clock_(clock),work_history_(history){}
 Outcome<std::vector<StableId>> active_patrol_journeys(const WorldMoment&)const override;
 Outcome<PatrolRouteExecutionRead> read_patrol_route_execution(const StableId&,const WorldMoment&)const override;
 Outcome<MovementAnchor> read_patrol_checkpoint_anchor(const PatrolRouteExecutionRead&,const SourceRef&)const override;
};
// Only the actual accepted source tasks/work_accepted/schema1 is directly
// consumable. A distinct S6 order requires its original explicit Work binding.
Outcome<WorkExecutionSourceRead> read_patrol_work_execution(const PatrolWorkSources&,const WorkEpisode&,const MomentOwnerSource& actual_duty_binding);
Outcome<WorkPeriodRead> read_patrol_work_period(const PatrolWorkSources&,const WorkEpisode&,const WorkMilitaryPeriodSource&);
Outcome<WorkCompletionRead> read_patrol_work_completion(const PatrolWorkSources&,const WorkEpisode&,const std::vector<StableId>& actual_receipts,const WorldMoment&);
// A real final S1 checkpoint may satisfy the remaining route condition after
// service credit is already complete. This reader emits no result or period.
Outcome<const PatrolCheckpointArrival2*> read_patrol_completion_checkpoint(const PatrolWorkSources&,const WorkEpisode&,const WorldMoment& through);
// Return the actual Work to which this original Duty source was assigned. No
// match is an unrelated Duty; a corrupt claimed S3 assignment is an error.
Outcome<const WorkEpisode*> read_work_assigned_to_patrol_duty(const PatrolWorkSources&,const StableId& duty_id);
class NativePatrolDutyExecutionConsumer final:public MilitaryDutyExecutionConsumer {
 CommonStore& common_;TaskState& tasks_;const PatrolWorkSources& sources_;const WorkOriginalRead& original_;
public:NativePatrolDutyExecutionConsumer(CommonStore&c,TaskState&t,const PatrolWorkSources&s,const WorkOriginalRead&o):common_(c),tasks_(t),sources_(s),original_(o){}
 Status begin(const StableId&,const MomentOwnerSource&,const WorldMoment&)override;
 Status end(const StableId&,const MomentOwnerSource&,const WorldMoment&)override;
};

}
