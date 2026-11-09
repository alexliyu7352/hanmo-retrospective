// One S3 finite original-source router. It owns no Work state or clock.
#pragma once
#include "work_supervision_original.hpp"
#include "work_patrol_sources.hpp"
#include "work_skill_inputs.hpp"
#include "resident/kernel/dynamic_result_time2.hpp"
namespace hanmo::resident {
// S6 supplies original termination/continuation authority, not a second progress
// or completion owner. These reads must preserve historical same-M source order.
class WorkLifecycleOriginalRead {
public:virtual ~WorkLifecycleOriginalRead()=default;
 virtual Outcome<WorkStopRead> read_actual_work_stop(const WorkEpisode&,const MomentOwnerSource&,const WorldMoment&)const=0;
 virtual Status validate_actual_work_leader_change(const WorkEpisode&,const WorkLeaderChange&)const=0;
};
class NativeWorkOriginalRead final:public WorkOriginalRead {
 const WorkTaskSourceContext& task_sources_;const WorkTaskRead& definitions_;
 const SupervisionNativeContext& native_;const SupervisionEconomicSources& economic_;
 const PatrolWorkSources& patrol_;const WorkLifecycleOriginalRead& lifecycle_;
 const ExactResultOwnerRead& original_sources_;const WorkSkillInputsRead& skills_;
 NativeSupervisionEligibilityRead eligibility_;SupervisionSourceContext supervision_;
public:
 NativeWorkOriginalRead(const WorkTaskSourceContext&,const WorkTaskRead&,const SupervisionNativeContext&,const SupervisionEconomicSources&,const PatrolWorkSources&,const WorkLifecycleOriginalRead&,const ExactResultOwnerRead&,const WorkSkillInputsRead&,const SupervisionExecutionApprovalRead&,const ActionElapsedStore&,const TimeScaleRef&,const WorkReassignmentSourceContext* reassignments=nullptr);
 NativeWorkOriginalRead(const NativeWorkOriginalRead&)=delete;
 NativeWorkOriginalRead& operator=(const NativeWorkOriginalRead&)=delete;
 NativeWorkOriginalRead(NativeWorkOriginalRead&&)=delete;
 NativeWorkOriginalRead& operator=(NativeWorkOriginalRead&&)=delete;
 const WorkSkillInputsRead* skill_inputs()const noexcept override{return &skills_;}
 const SupervisionSourceContext& supervision_sources()const noexcept{return supervision_;}
 Outcome<WorkAcceptedRead> read_accepted_work(const WorkAuthorizationRef&)const override;
 Status validate_current_start(const WorkAcceptedRead&,const WorldMoment&)const override;
 Outcome<WorkExecutionSourceRead> read_original_execution(const WorkEpisode&,const MomentOwnerSource&)const override;
 Outcome<WorkPeriodRead> read_original_period(const WorkEpisode&,const WorkPeriodSource&)const override;
 Outcome<WorkCompletionRead> read_completion(const WorkEpisode&,const std::vector<StableId>&,const WorldMoment&)const override;
 Outcome<WorkStopRead> read_actual_stop(const WorkEpisode&,const MomentOwnerSource&,const WorldMoment&)const override;
 Status validate_leader_change(const WorkEpisode&,const WorkLeaderChange&)const override;
 Status validate_original_source(const MomentOwnerSource&)const override;
};
}
