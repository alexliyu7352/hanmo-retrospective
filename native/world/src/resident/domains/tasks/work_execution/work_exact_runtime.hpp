#pragma once
#include "work_supervision_original.hpp"
#include "work_patrol_sources.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include <memory>
namespace hanmo::resident {
struct GovernanceSupervisionApprovalValidationContext;
// 每次实际phase新建借读lease；持者保原来源寿命，不复制世界或缓存旧Moment。
struct TaskWorkReadBinding {
 const SupervisionSourceContext& execution;const SupervisionNativeContext& native;
 const SupervisionEconomicSources& economic;const PatrolWorkSources* patrol=nullptr;
 // Whole cold validation only; never called while constructing source adapters.
 const GovernanceSupervisionApprovalValidationContext* governance=nullptr;
};
struct TaskWorkWriteBinding {
 const SupervisionSourceContext& execution;const SupervisionNativeContext& native;
 const SupervisionEconomicSources& economic;EconomicProductionContext& production;
 WorkExpenseConsumer& expenses;const PatrolWorkSources* patrol=nullptr;
 WorkCompletedResultConsumer* completed_results=nullptr;
 GovernanceProgramState* lifecycle_programs=nullptr;
};
class TaskWorkReadLease {public:virtual ~TaskWorkReadLease()=default;virtual TaskWorkReadBinding bindings()const=0;};
class TaskWorkWriteLease {public:virtual ~TaskWorkWriteLease()=default;virtual TaskWorkWriteBinding bindings()=0;};
struct TaskWorkWorldFactory {
 Outcome<std::shared_ptr<const TaskWorkReadLease>> (*read)(const WorldBundle&,const WorldMoment&)=nullptr;
 Outcome<std::shared_ptr<TaskWorkWriteLease>> (*write)(WorldBundle&,const WorldMoment&)=nullptr;
};
TaskWorkWorldFactory native_task_work_world_factory() noexcept;
Status validate_supervision_world_sources(const WorldBundle&,const WorldMoment&,TaskWorkWorldFactory);
Outcome<std::vector<ExactEventPrediction>> predict_supervision_work(const WorldBundle&,const WorldMoment& from,const WorldMoment& through,TaskWorkWorldFactory);
Status provide_supervision_action_interval(WorldBundle&,const MomentSpan&,TaskWorkWorldFactory);
Outcome<ExactEventApplied> apply_supervision_work_due(WorldBundle&,const ExactEventPrediction&,TaskWorkWorldFactory,ExactPhaseWriteContext&);
// Called after a real original body/permission interruption is applied. It
// consumes only this Action's actual remaining service slice before releasing
// occupancy, keeps the Work obligation paused, and creates no Work result.
Outcome<std::optional<SourceRef>> consume_supervision_interruption_in_candidate(WorldBundle&,const StableId& actor_id,const MomentOwnerSource& actual_cause,TaskWorkWorldFactory,ExactPhaseWriteContext&);
Status validate_supervision_applied_source(const WorldBundle&,const ExactEventApplied&,TaskWorkWorldFactory);
}
