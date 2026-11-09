// Actual S3 Task/Work source graph. It owns adapters, never Work or clock
// state.
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/local_economy/economic_native_lease.hpp"
#include "resident/domains/politics/governance_targets/governance_service_continuity.hpp"
#include "resident/domains/politics/governance_targets/governance_supervision_approvals.hpp"
#include "resident/domains/politics/governance_targets/governance_supervision_execution_approval.hpp"
#include "resident/domains/politics/governance_targets/governance_work_skill_rules.hpp"
#include "work_exact_runtime.hpp"
#include "work_lifecycle_original.hpp"
#include "work_original_read.hpp"
#include "work_reassignment_sources.hpp"
namespace hanmo::resident {
class CompleteExactWorldBindingOwner;
struct NativeTaskWorkDependencies {
  WorkTaskSourceContext tasks;
  const CharacterConditionSources &body;
  const GrowthState &growth;
  SupervisionSpatialSources spatial;
  const NativeEconomicExactSources &economy;
  const GovernanceSupervisionTargetOriginalRead &targets;
  const OrganizationLifecycleCatalog &lifecycle_catalog;
  const OrganizationLifecycleSourceRead &lifecycle_sources;
  const OfficeTransitionSourceRead &office_sources;
  const VerifiedDeathRead &deaths;
  const ReadStamp &original_sources_stamp;
  const ReadStampAuthority &stamp_authority;
  const DomainReadVersionSource &versions;
  const MilitaryDutyState &duties;
  const MilitaryDutyContext &duty_context;
  const TravelCatalog &travel_catalog;
  // /6 截止及替任由本 owning 图反读；旧 profile 必须显式提供其原 fallback。
  const WorkLifecycleOriginalRead *work_lifecycle = nullptr;
  const ExactResultOwnerRead &original_sources;
  const ActionElapsedStore &elapsed;
  const TimeScaleRef &scale;
};
// The enclosing native world lease retains all referenced original owners.
// Construction only binds their real contexts; it does not validate partially
// decoded source totals, create history, or infer missing lifecycle decisions.
class NativeTaskWorkSources final {
  NativeTaskWorkDependencies dependencies_;
  WorkTaskRead definitions_;
  GovernanceWorkHistoryContext history_;
  NativeGovernanceWorkSkillRuleRead skill_rules_;
  NativeWorkSkillInputsRead skills_;
  GovernanceServiceContinuityContext continuity_context_;
  SupervisionEconomicSources economic_;
  NativePatrolCheckpointOriginalRead checkpoints_;
  PatrolWorkSources patrol_;
  GovernanceWorkLifecycleSourceContext lifecycle_context_;
  WorkReassignmentSourceContext reassignment_context_;
  NativeGovernanceSupervisionContinuityRead continuity_;
  NativeGovernanceSupervisionScopeRead scopes_;
  NativeGovernanceSupervisionExecutionApprovalRead approvals_;
  SupervisionNativeContext native_;
  NativeGovernanceWorkLifecycleRead lifecycle_;
  NativeWorkOriginalRead original_;
  SupervisionActionTimeRead actions_;
  GovernanceSupervisionApprovalValidationContext governance_validation_;

public:
  explicit NativeTaskWorkSources(NativeTaskWorkDependencies);
  NativeTaskWorkSources(const NativeTaskWorkSources &) = delete;
  NativeTaskWorkSources &operator=(const NativeTaskWorkSources &) = delete;
  NativeTaskWorkSources(NativeTaskWorkSources &&) = delete;
  NativeTaskWorkSources &operator=(NativeTaskWorkSources &&) = delete;
  Status validate_bindings() const;
  Status validate_sources() const;
  TaskWorkReadBinding read_binding() const;
  const GovernanceWorkLifecycleSourceContext &lifecycle_sources() const {
    return lifecycle_context_;
  }
  const WorkReassignmentSourceContext &reassignment_sources() const {
    return reassignment_context_;
  }
  const NativeWorkOriginalRead &original() const { return original_; }
  const WorkTaskRead &definitions() const { return definitions_; }
  const GovernanceWorkHistoryContext &history() const { return history_; }
  const NativeGovernanceSupervisionContinuityRead &continuity() const {
    return continuity_;
  }
  const NativeGovernanceSupervisionScopeRead &scopes() const { return scopes_; }
  const NativeGovernanceSupervisionExecutionApprovalRead &approvals() const {
    return approvals_;
  }
  const NativeWorkSkillInputsRead &skills() const { return skills_; }
  const NativePatrolCheckpointOriginalRead &checkpoints() const {
    return checkpoints_;
  }
  const PatrolWorkSources &patrol() const { return patrol_; }
  const SupervisionActionTimeRead &actions() const { return actions_; }
  const NativeTaskWorkDependencies &dependencies() const {
    return dependencies_;
  }
};
class NativeTaskWorkReadLease final : public TaskWorkReadLease,
                                      public CheckpointAllocatedObject {
  // First owner outlives every borrowed root/provider in the original graph.
  std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
  NativeTaskWorkSources sources_;

public:
  NativeTaskWorkReadLease(std::unique_ptr<CompleteExactWorldBindingOwner>,
                          NativeTaskWorkDependencies);
  ~NativeTaskWorkReadLease();
  NativeTaskWorkReadLease(const NativeTaskWorkReadLease &) = delete;
  NativeTaskWorkReadLease &operator=(const NativeTaskWorkReadLease &) = delete;
  NativeTaskWorkReadLease(NativeTaskWorkReadLease &&) = delete;
  NativeTaskWorkReadLease &operator=(NativeTaskWorkReadLease &&) = delete;
  Status validate_bindings() const;
  Status validate_sources() const;
  TaskWorkReadBinding bindings() const override {
    return sources_.read_binding();
  }
  const NativeTaskWorkSources &sources() const { return sources_; }
};
struct NativeTaskWorkWriteDependencies {
  NativeTaskWorkDependencies read;
  CommonStore &common;
  TaskState &tasks;
  // These are the actual same-candidate original S5 writers, not an S3 expense
  // callback or a second production context assembled from current values.
  NativeEconomicExactWriteLease &economics;
  WorkCompletedResultConsumer &completed_results;
  // /6 写端必须是 read.tasks.governance.state 的同一真实可写根。
  GovernanceProgramState *lifecycle_programs = nullptr;
};
class NativeTaskWorkWriteLease final : public TaskWorkWriteLease,
                                       public CheckpointAllocatedObject {
  std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
  NativeTaskWorkWriteDependencies dependencies_;
  NativeTaskWorkSources sources_;
  NativePatrolDutyExecutionConsumer duty_execution_;

public:
  NativeTaskWorkWriteLease(std::unique_ptr<CompleteExactWorldBindingOwner>,
                           NativeTaskWorkWriteDependencies);
  ~NativeTaskWorkWriteLease();
  NativeTaskWorkWriteLease(const NativeTaskWorkWriteLease &) = delete;
  NativeTaskWorkWriteLease &
  operator=(const NativeTaskWorkWriteLease &) = delete;
  NativeTaskWorkWriteLease(NativeTaskWorkWriteLease &&) = delete;
  NativeTaskWorkWriteLease &operator=(NativeTaskWorkWriteLease &&) = delete;
  Status validate_bindings() const;
  TaskWorkWriteBinding bindings() override;
  const NativeTaskWorkSources &sources() const { return sources_; }
  NativePatrolDutyExecutionConsumer &duty_execution_consumer() {
    return duty_execution_;
  }
};
// Non-reacquiring inner leases for CompleteExactWorldBindingOwner ports. The
// outer native factory retains that owner until the returned inner lease dies.
// All external providers, Duty context, open-period pointer and economic leases
// must be retained members of that same owner, never acquisition-stack locals.
// The inner owns M, body/context snapshots and the complete local Work adapter
// graph. Construction checks binding identity only, not decoded source totals.
Outcome<std::shared_ptr<const TaskWorkReadLease>>
make_native_task_work_read_lease_in_owner(
    const WorldBundle &, const CompleteExactWorldBindingOwner &,
    NativeTaskWorkDependencies, const WorldMoment &);
Outcome<std::shared_ptr<TaskWorkWriteLease>>
make_native_task_work_write_lease_in_owner(WorldBundle &,
                                           CompleteExactWorldBindingOwner &,
                                           NativeTaskWorkWriteDependencies,
                                           const WorldMoment &);
} // namespace hanmo::resident
