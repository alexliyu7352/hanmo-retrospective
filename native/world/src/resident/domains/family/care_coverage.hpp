// P24.1 original binding and coverage roots. Exact occurrence lives only in
// CareTemporalEvent; the Calendar fields below are checked projections.
#pragma once
#include "care_handover_period.hpp"
namespace hanmo::resident {
enum class CareBindingPhase { active,interrupted,ended };
struct CareBinding {
 StableId binding_id,child_actor_id,carer_actor_id,terms_ref,accepted_ref;
 Integer started_tick=0;std::optional<Integer> until_tick;
 MomentOwnerSource care_activity_ref,placement_ref;
 CareBindingPhase phase=CareBindingPhase::active;
};
struct CareCoverageSatisfied {
 StableId fact_id,requirement_id;Integer source_episode=1;StableId binding_ref;
 SourceRef handover_result_ref;Integer effective_tick=0;
};
struct CareCoverageLost {
 StableId fact_id,requirement_id,old_binding_ref;SourceRef cause_ref;
 Integer effective_tick=0;
};
struct CareCoverageReadTables {
 const OrderedTable<CareBinding>& bindings;
 const OrderedTable<CareCoverageSatisfied>& satisfied;
 const OrderedTable<CareCoverageLost>& lost;
};
struct CareFamilyActivitySources;struct CareFamilyActivityWriteBinding;
struct CareCoverageWriteTables {
 OrderedTable<CareBinding>& bindings;OrderedTable<CareCoverageSatisfied>& satisfied;
 OrderedTable<CareCoverageLost>& lost;
 CareFamilyActivityWriteBinding* family_activities=nullptr;
 CareCoverageReadTables read()const{return {bindings,satisfied,lost};}
};
// The actual placement/activity owners verify their own immutable results and
// permissions. A position match, receipt ID, or today's carer is insufficient.
class CareCoverageOriginalRead {
public:
 virtual ~CareCoverageOriginalRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual const CareFamilyActivitySources* family_activity_sources()const noexcept{return nullptr;}
 virtual Status validate_original_completed_handover(const CareHandoverExecution&,
  const AcceptedCaregiver&,const CareBinding&,const CareTemporalEvent&)const=0;
 virtual Status validate_current_care_participation(const CareBinding&,
  const AcceptedCaregiver&,const WorldMoment&)const=0;
};
class NativeCareDependentPlacementRead;class NativeCareActualSeparationRead;
struct CareCoverageSources {
 const CareHandoverSourceContext& handovers;CareCoverageReadTables tables;
 const CareHandoverPeriodOriginalRead& periods;const CareCoverageOriginalRead& originals;
 const NativeCareDependentPlacementRead* placements=nullptr;
 const NativeCareActualSeparationRead* separations=nullptr;
};
SourceRef care_coverage_satisfied_source(const StableId&);
SourceRef care_coverage_lost_source(const StableId&);
Outcome<Integer> care_coverage_source_schema(const SourceRef&);
Status validate_care_binding_shape(const CareBinding&);
Status validate_care_coverage_satisfied_shape(const CareCoverageSatisfied&);
Status validate_care_coverage_lost_shape(const CareCoverageLost&);
// The actual S1 position/dependency result is read directly from its sole
// owner. This is independent of ongoing activity/fees and never self-proves
// placement from the new Care Completed or today's child location.
Status validate_care_handover_actual_placement(const CareCoverageSources&,
 const CareHandoverExecution&,const AcceptedCaregiver&,const CareBinding&,
 const CareTemporalEvent&);
// These source-local checks never call the current CareGoal/Need validator.
Status validate_original_care_handover_coverage(const CareCoverageSources&,
 const CareRequirement&,const CareRequirementTransition&,const CareTemporalEvent&);
Status validate_original_care_coverage_loss(const CareCoverageSources&,
 const CareRequirement&,const CareRequirementTransition&,const CareTemporalEvent&);
Status validate_current_care_coverage(const CareCoverageSources&,const CareRequirement&,
 const WorldMoment&);
Status validate_care_coverage_source_closure(const CareCoverageSources&);
Outcome<const CareCoverageSatisfied*> read_care_coverage_satisfaction(
 const CareCoverageSources&,const SourceRef&);
// v1c 3.3: borrowed original coverage, never a prediction from current age,
// location or a current caregiver name. The accepted Calendar until remains
// in accepted->terms.until_tick; ended_by is only an already actual event.
// An interrupted old binding is readable after later episodes/handoffs.
struct CareBindingCoverageView2 {
 const CareBinding* binding=nullptr;
 const AcceptedCaregiver* accepted=nullptr;
 const CareCoverageSatisfied* satisfaction=nullptr;
 const CareTemporalEvent* handover_completed=nullptr;
 const CareTemporalEvent* ended_by=nullptr;
};
Outcome<const WorldMoment*> read_care_transition_moment_v2(
 const CareGoalSourceContext&,const SourceRef& actual_transition);
Outcome<const WorldMoment*> read_care_satisfaction_moment_v2(
 const CareCoverageSources&,const SourceRef& actual_satisfaction);
Outcome<CareBindingCoverageView2> read_care_binding_coverage_v2(
 const CareCoverageSources&,const StableId& binding_id);
// The original PeriodOnly producer calls this for a completed [t,t+1).
// A coverage end at to is allowed; a start after from is not rounded back.
// Actual original service/participation/provision sources are separate gates.
Status validate_care_binding_full_calendar_period(const CareCoverageSources&,
 const StableId& binding_id,Integer from_tick,Integer to_tick);
// Original death source, not notification. Same binding/cause replay returns
// its prior loss, even if a later genuine handover has since created a binding.
Outcome<CareRequirementApplication> lose_care_coverage_after_holder_death_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,
 const CareGoalSourceContext&,const StableId& actual_binding,const DeathRef&);
// Actual ordinary spatial source, with its own M and original Common cut.
// New same-place heads do not imply a separation. Missing providers are technical.
Outcome<CareRequirementApplication> lose_care_coverage_after_actual_separation_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,const CareGoalSourceContext&,
 const StableId& actual_binding,const MomentOwnerSource& actual_spatial_transition);
Outcome<BirthCareNeedApplication> lose_care_coverage_and_need_after_actual_separation_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,const CareGoalSourceContext&,
 const StableId& actual_binding,const MomentOwnerSource& actual_spatial_transition,
 AffairsState&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&);
// Only the original embedded, sealed StopCare result. Never an arbitrary
// authority-ended ref or a caller-provided snapshot of the current binding.
Outcome<CareRequirementApplication> lose_care_coverage_after_personal_stop_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,const CareGoalSourceContext&,
 const StableId& actual_binding,const MomentOwnerSource& actual_stop);
// Original absolute Calendar window, not a guessed elapsed deadline. The due
// callback creates no loss if another genuine cause already ended this binding.
Outcome<std::optional<WorldMoment>> next_care_coverage_window_end(const CareCoverageSources&);
Outcome<std::optional<CareRequirementApplication>> apply_care_coverage_window_end_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,const CareGoalSourceContext&,
 const StableId& actual_binding,Integer accepted_until_tick);
Outcome<CareRequirementApplication> close_care_coverage_after_subject_death_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,
 const CareGoalSourceContext&,const StableId& requirement_id,const DeathRef&);
Outcome<BirthCareNeedApplication> lose_care_coverage_and_need_after_holder_death_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,const CareGoalSourceContext&,
 const StableId& actual_binding,const DeathRef&,AffairsState&,const StateNeedPolicyRead&,
 const StateGoalRead&,StateTaskConsumer&);
Outcome<std::optional<BirthCareNeedApplication>> apply_care_coverage_and_need_window_end_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,const CareGoalSourceContext&,
 const StableId& actual_binding,Integer accepted_until_tick,AffairsState&,
 const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&);
// 同一原 Calendar 门消费所有实际到期 binding，每笔写后重取原来源。
// 只覆盖 accepted until 分支；不代成年、合法撤需求或完整 guardianship。
// 缺真实 coverage/Need/source 仍拒绝，不产生第二 period 或替代目标。
Status apply_due_care_coverage_and_need_window_ends_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,const CareGoalSourceContext&,
 AffairsState&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&);
Outcome<BirthCareNeedApplication> close_care_coverage_and_need_after_subject_death_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,const CareGoalSourceContext&,
 const StableId& requirement_id,const DeathRef&,AffairsState&,const StateNeedPolicyRead&,
 const StateGoalRead&,StateTaskConsumer&);
}
