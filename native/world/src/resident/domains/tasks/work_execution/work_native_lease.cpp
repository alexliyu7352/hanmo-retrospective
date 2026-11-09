#include "work_native_lease.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
NativeTaskWorkSources::NativeTaskWorkSources(NativeTaskWorkDependencies d)
    : dependencies_(std::move(d)), definitions_(dependencies_.tasks),
      history_{dependencies_.tasks.governance, dependencies_.tasks.tasks,
               definitions_},
      skill_rules_(history_),
      skills_({dependencies_.tasks.common, dependencies_.tasks.tasks,
               dependencies_.body, dependencies_.growth, skill_rules_,
               dependencies_.tasks.clock}),
      continuity_context_{history_,
                          dependencies_.lifecycle_catalog,
                          dependencies_.lifecycle_sources,
                          dependencies_.office_sources,
                          dependencies_.deaths,
                          dependencies_.original_sources_stamp,
                          dependencies_.stamp_authority,
                          dependencies_.versions,
                          dependencies_.body},
      economic_{
          dependencies_.economy.dependencies().production.population.economy,
          dependencies_.economy.dependencies().production.catalog,
          dependencies_.economy.dependencies().production.work_catalog,
          dependencies_.economy.calendar().sources,
          dependencies_.economy.production().inputs(),
          dependencies_.economy.dependencies()
              .household.scale.units_per_half_day},
      checkpoints_(
          dependencies_.tasks.common, dependencies_.tasks.tasks, definitions_,
          dependencies_.duties, dependencies_.duty_context,
          dependencies_.spatial.movement, dependencies_.spatial.catalog,
          dependencies_.travel_catalog, dependencies_.tasks.clock,
          dependencies_.tasks.tasks.work_execution &&
                  task_work_lifecycle_sources_version(
                      dependencies_.tasks.tasks.work_execution->version)
              ? &history_
              : nullptr),
      patrol_{dependencies_.tasks.common,
              dependencies_.tasks.tasks,
              definitions_,
              dependencies_.duties,
              dependencies_.duty_context,
              dependencies_.spatial.movement,
              checkpoints_,
              dependencies_.tasks.clock,
              dependencies_.tasks.tasks.work_execution &&
                      task_work_lifecycle_sources_version(
                          dependencies_.tasks.tasks.work_execution->version)
                  ? &history_
                  : nullptr},
      lifecycle_context_{history_, continuity_context_, dependencies_.elapsed,
                         dependencies_.scale, patrol_},
      reassignment_context_{dependencies_.tasks, lifecycle_context_,
                            dependencies_.original_sources_stamp},
      continuity_(continuity_context_,
                  dependencies_.tasks.tasks.work_execution &&
                          task_work_lifecycle_sources_version(
                              dependencies_.tasks.tasks.work_execution->version)
                      ? &lifecycle_context_
                      : nullptr),
      scopes_({history_, dependencies_.targets, continuity_}),
      approvals_(history_, continuity_), native_{dependencies_.tasks.common,
                                                 dependencies_.tasks.tasks,
                                                 dependencies_.body,
                                                 dependencies_.spatial,
                                                 scopes_,
                                                 dependencies_.tasks.clock,
                                                 &skills_},
      lifecycle_(reassignment_context_, dependencies_.work_lifecycle),
      original_(dependencies_.tasks, definitions_, native_, economic_, patrol_,
                lifecycle_, dependencies_.original_sources, skills_, approvals_,
                dependencies_.elapsed, dependencies_.scale,
                dependencies_.tasks.tasks.work_execution &&
                        task_work_lifecycle_sources_version(
                            dependencies_.tasks.tasks.work_execution->version)
                    ? &reassignment_context_
                    : nullptr),
      actions_(original_.supervision_sources()),
      governance_validation_{history_, continuity_,
                             original_.supervision_sources()} {}
Status NativeTaskWorkSources::validate_bindings() const {
  const auto &d = dependencies_;
  const auto &t = d.tasks;
  const auto &e = d.economy;
  auto valid = e.validate_bindings();
  if (!valid.ok)
    return valid;
  const auto &production = e.dependencies().production;
  if (&t.common != &t.governance.common || &t.common != &d.body.common ||
      d.body.growth != &d.growth || d.body.elapsed != &d.elapsed ||
      d.body.scale != &d.scale || d.body.movement != &d.spatial.movement ||
      d.body.shared != &d.spatial.catalog ||
      &t.common != &e.calendar().common || &t.tasks != &production.tasks ||
      &t.clock != &e.calendar().clock ||
      &t.clock != &production.population.clock ||
      &d.spatial.movement != &production.movement ||
      &d.spatial.carriers != &production.carriers ||
      &d.spatial.originals != &production.spatial ||
      &d.scale != &d.duty_context.scale ||
      &d.scale != &e.dependencies().household.scale ||
      d.scale.units_per_half_day <= 0 ||
      economic_.units_per_half_day != d.scale.units_per_half_day ||
      !skills_.bound_to(t.common, t.tasks) ||
      !approvals_.bound_to(t.common, t.tasks))
    return common_failure("NativeTaskWorkSnapshotMismatch", {},
                          ErrorKind::internal);
  if (!t.tasks.work_execution ||
      !task_work_fixed_execution_terms_version(
          t.tasks.work_execution->version) ||
      t.governance.catalog.schema_version != 4 ||
      (t.governance.state.schema_version != 2 &&
       t.governance.state.schema_version != 3) ||
      (task_work_lifecycle_sources_version(t.tasks.work_execution->version) !=
       (t.governance.state.schema_version == 3)) ||
      !d.duty_context.movement_executions)
    return common_failure("NativeTaskWorkOriginalOwnerGraphRequired", {},
                          ErrorKind::unsupported_capability);
  // 空 fallback 只允许已选 /6 的真实有限 lifecycle；不能让 /5 空根冒可运行。
  if (!d.work_lifecycle &&
      !task_work_lifecycle_sources_version(t.tasks.work_execution->version))
    return common_failure("NativeTaskWorkLifecycleOriginalRequired", {},
                          ErrorKind::unsupported_capability);
  auto now = t.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto body = compare_world_moments(now.value, d.body.actual_now);
  if (!body.ok)
    return {false, {}, body.error};
  auto duty = d.duty_context.stamps.read_current_moment();
  if (!duty.ok)
    return {false, {}, duty.error};
  auto same = compare_world_moments(now.value, duty.value);
  if (!same.ok)
    return {false, {}, same.error};
  auto governance = t.governance.clock.read_organization_authority_now();
  if (!governance.ok)
    return {false, {}, governance.error};
  auto authority = compare_world_moments(now.value, governance.value);
  if (!authority.ok)
    return {false, {}, authority.error};
  if (body.value || same.value || authority.value)
    return common_failure("NativeTaskWorkPhaseMismatch", {},
                          ErrorKind::internal);
  const auto mode = d.original_sources_stamp.mode();
  if (mode != ReadMode::phase_current && mode != ReadMode::committed_current)
    return common_failure("NativeTaskWorkActualSourceStampRequired", {},
                          ErrorKind::internal);
  return d.stamp_authority.validate(d.original_sources_stamp,
                                    t.common.state().identity, mode, t.common,
                                    d.versions);
}
TaskWorkReadBinding NativeTaskWorkSources::read_binding() const {
  return {original_.supervision_sources(), native_, economic_, &patrol_,
          &governance_validation_};
}
Status NativeTaskWorkSources::validate_sources() const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return valid;
  valid = definitions_.validate_all();
  if (!valid.ok)
    return valid;
  valid =
      validate_governance_programs(history_.programs, &governance_validation_);
  if (!valid.ok)
    return valid;
  auto now = dependencies_.tasks.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  valid = validate_task_work_execution(dependencies_.tasks.common,
                                       dependencies_.tasks.tasks, original_,
                                       now.value);
  if (!valid.ok)
    return valid;
  return validate_supervision_sources(original_.supervision_sources());
}
NativeTaskWorkReadLease::NativeTaskWorkReadLease(
    std::unique_ptr<CompleteExactWorldBindingOwner> owner,
    NativeTaskWorkDependencies d)
    : owner_(std::move(owner)), sources_(std::move(d)) {}
NativeTaskWorkReadLease::~NativeTaskWorkReadLease() = default;
Status NativeTaskWorkReadLease::validate_bindings() const {
  if (!owner_)
    return common_failure("NativeTaskWorkDependencyOwnerRequired", {},
                          ErrorKind::internal);
  return sources_.validate_bindings();
}
Status NativeTaskWorkReadLease::validate_sources() const {
  auto valid = validate_bindings();
  return valid.ok ? sources_.validate_sources() : valid;
}
NativeTaskWorkWriteLease::NativeTaskWorkWriteLease(
    std::unique_ptr<CompleteExactWorldBindingOwner> owner,
    NativeTaskWorkWriteDependencies d)
    : owner_(std::move(owner)), dependencies_(std::move(d)),
      sources_(dependencies_.read),
      duty_execution_(dependencies_.common, dependencies_.tasks,
                      sources_.patrol(), sources_.original()) {}
NativeTaskWorkWriteLease::~NativeTaskWorkWriteLease() = default;
namespace {
Status validate_native_task_work_write_binding(
    const NativeTaskWorkWriteDependencies &d,
    const NativeTaskWorkSources &sources) {
  auto valid = d.economics.validate_bindings();
  if (!valid.ok)
    return valid;
  if (&d.common != &d.read.tasks.common || &d.tasks != &d.read.tasks.tasks ||
      &d.read.economy != &d.economics.sources() ||
      !d.completed_results.bound_to(d.common, d.tasks) ||
      (task_work_lifecycle_sources_version(d.tasks.work_execution->version) &&
       (!d.lifecycle_programs ||
        d.lifecycle_programs != &d.read.tasks.governance.state)))
    return common_failure("NativeTaskWorkWriteCandidateMismatch", {},
                          ErrorKind::internal);
  const auto &production = d.economics.production_context();
  const auto &read = sources.read_binding().economic;
  if (&production.catalog != &read.rules || &production.state != &read.state ||
      &production.inputs != &read.originals ||
      &production.clock != &d.read.tasks.clock ||
      production.units_per_half_day != read.units_per_half_day)
    return common_failure("NativeTaskWorkProductionOwnerMismatch", {},
                          ErrorKind::internal);
  return common_success();
}
} // namespace
Status NativeTaskWorkWriteLease::validate_bindings() const {
  if (!owner_)
    return common_failure("NativeTaskWorkDependencyOwnerRequired", {},
                          ErrorKind::internal);
  auto valid = sources_.validate_bindings();
  if (!valid.ok)
    return valid;
  return validate_native_task_work_write_binding(dependencies_, sources_);
}
TaskWorkWriteBinding NativeTaskWorkWriteLease::bindings() {
  const auto read = sources_.read_binding();
  return {read.execution,
          read.native,
          read.economic,
          dependencies_.economics.production_context(),
          dependencies_.economics.work_expenses(),
          read.patrol,
          &dependencies_.completed_results,
          dependencies_.lifecycle_programs};
}
namespace {
NativeTaskWorkDependencies
phase_dependencies(const NativeTaskWorkDependencies &d,
                   const GovernanceProgramReadContext &programs,
                   const CharacterConditionSources &body) {
  return {{d.tasks.common, d.tasks.tasks, programs, d.tasks.interaction,
           d.tasks.interaction_catalog, d.tasks.interaction_access,
           d.tasks.clock, d.tasks.social_periods, d.tasks.current_open_period},
          body,
          d.growth,
          d.spatial,
          d.economy,
          d.targets,
          d.lifecycle_catalog,
          d.lifecycle_sources,
          d.office_sources,
          d.deaths,
          d.original_sources_stamp,
          d.stamp_authority,
          d.versions,
          d.duties,
          d.duty_context,
          d.travel_catalog,
          d.work_lifecycle,
          d.original_sources,
          d.elapsed,
          d.scale};
}
// References below point either to the retained outer owner/world or to earlier
// members here. Neither the caller's at nor its aggregate context is retained.
class NativeTaskWorkPhaseSources final {
  const WorldBundle &world_;
  const CompleteExactWorldBindingOwner &owner_;
  WorldMoment at_;
  GovernanceProgramReadContext programs_;
  CharacterConditionSources body_;
  NativeTaskWorkSources sources_;

public:
  NativeTaskWorkPhaseSources(const WorldBundle &w,
                             const CompleteExactWorldBindingOwner &owner,
                             NativeTaskWorkDependencies d,
                             const WorldMoment &at)
      : world_(w), owner_(owner), at_(at), programs_(d.tasks.governance),
        body_(d.body), sources_(phase_dependencies(d, programs_, body_)) {}
  NativeTaskWorkPhaseSources(const NativeTaskWorkPhaseSources &) = delete;
  NativeTaskWorkPhaseSources &
  operator=(const NativeTaskWorkPhaseSources &) = delete;
  NativeTaskWorkPhaseSources(NativeTaskWorkPhaseSources &&) = delete;
  NativeTaskWorkPhaseSources &operator=(NativeTaskWorkPhaseSources &&) = delete;
  Status validate_bindings() const {
    const auto &d = sources_.dependencies();
    if (!world_.catalog || !world_.actor_foundation || !world_.action_elapsed ||
        !world_.movement_exact || !world_.spatial_carriers ||
        !world_.governance_programs ||
        !world_.catalog->governance_program_catalog ||
        !world_.organization_runtime || !world_.catalog->organization ||
        !world_.catalog->organization_lifecycle ||
        &d.tasks.common != &world_.common || &d.tasks.tasks != &world_.tasks ||
        &d.growth != &world_.growth ||
        &d.body.state != &*world_.actor_foundation ||
        &d.elapsed != &*world_.action_elapsed ||
        &d.spatial.movement != &*world_.movement_exact ||
        &d.spatial.carriers != &*world_.spatial_carriers ||
        &d.tasks.governance.organization != &world_.organization ||
        &d.tasks.governance.organization_runtime !=
            &*world_.organization_runtime ||
        &d.tasks.governance.organization_catalog !=
            world_.catalog->organization.get() ||
        &d.lifecycle_catalog != world_.catalog->organization_lifecycle.get() ||
        &d.tasks.governance.state != &*world_.governance_programs ||
        &d.tasks.governance.catalog !=
            world_.catalog->governance_program_catalog.get() ||
        &owner_.reads().movement != &*world_.movement_exact)
      return common_failure("NativeTaskWorkWorldRootMismatch", {},
                            ErrorKind::internal);
    auto valid = sources_.validate_bindings();
    if (!valid.ok)
      return valid;
    const auto actual = world_.actual_phase_moment();
    if (!actual.ok)
      return {false, {}, actual.error};
    const auto clock = d.tasks.clock.read_current_moment();
    if (!clock.ok)
      return {false, {}, clock.error};
    // The economic phase root is retained by the outer owner, not copied into a
    // second production writer. Its actual M must match this inner snapshot.
    for (const auto *moment : {&actual.value, &clock.value, &d.body.actual_now,
                               &d.economy.dependencies().household.now}) {
      auto same = compare_world_moments(at_, *moment);
      if (!same.ok)
        return {false, {}, same.error};
      if (same.value)
        return common_failure("NativeTaskWorkLeaseMomentMismatch", {},
                              ErrorKind::internal);
    }
    return common_success();
  }
  const NativeTaskWorkSources &sources() const { return sources_; }
};
class NativeTaskWorkInOwnerReadLease final : public TaskWorkReadLease {
  NativeTaskWorkPhaseSources phase_;

public:
  NativeTaskWorkInOwnerReadLease(const WorldBundle &w,
                                 const CompleteExactWorldBindingOwner &owner,
                                 NativeTaskWorkDependencies d,
                                 const WorldMoment &at)
      : phase_(w, owner, std::move(d), at) {}
  NativeTaskWorkInOwnerReadLease(const NativeTaskWorkInOwnerReadLease &) =
      delete;
  NativeTaskWorkInOwnerReadLease &
  operator=(const NativeTaskWorkInOwnerReadLease &) = delete;
  NativeTaskWorkInOwnerReadLease(NativeTaskWorkInOwnerReadLease &&) = delete;
  NativeTaskWorkInOwnerReadLease &
  operator=(NativeTaskWorkInOwnerReadLease &&) = delete;
  Status validate_bindings() const { return phase_.validate_bindings(); }
  TaskWorkReadBinding bindings() const override {
    return phase_.sources().read_binding();
  }
};
class NativeTaskWorkInOwnerWriteLease final : public TaskWorkWriteLease {
  NativeTaskWorkPhaseSources phase_;
  NativeTaskWorkWriteDependencies dependencies_;
  NativePatrolDutyExecutionConsumer duty_execution_;

public:
  NativeTaskWorkInOwnerWriteLease(WorldBundle &w,
                                  CompleteExactWorldBindingOwner &owner,
                                  NativeTaskWorkWriteDependencies d,
                                  const WorldMoment &at)
      : phase_(w, owner, d.read, at),
        dependencies_{phase_.sources().dependencies(),
                      d.common,
                      d.tasks,
                      d.economics,
                      d.completed_results,
                      d.lifecycle_programs},
        duty_execution_(dependencies_.common, dependencies_.tasks,
                        phase_.sources().patrol(),
                        phase_.sources().original()) {}
  NativeTaskWorkInOwnerWriteLease(const NativeTaskWorkInOwnerWriteLease &) =
      delete;
  NativeTaskWorkInOwnerWriteLease &
  operator=(const NativeTaskWorkInOwnerWriteLease &) = delete;
  NativeTaskWorkInOwnerWriteLease(NativeTaskWorkInOwnerWriteLease &&) = delete;
  NativeTaskWorkInOwnerWriteLease &
  operator=(NativeTaskWorkInOwnerWriteLease &&) = delete;
  Status validate_bindings() const {
    auto valid = phase_.validate_bindings();
    if (!valid.ok)
      return valid;
    return validate_native_task_work_write_binding(dependencies_,
                                                   phase_.sources());
  }
  TaskWorkWriteBinding bindings() override {
    const auto read = phase_.sources().read_binding();
    return {read.execution,
            read.native,
            read.economic,
            dependencies_.economics.production_context(),
            dependencies_.economics.work_expenses(),
            read.patrol,
            &dependencies_.completed_results,
            dependencies_.lifecycle_programs};
  }
};
} // namespace
Outcome<std::shared_ptr<const TaskWorkReadLease>>
make_native_task_work_read_lease_in_owner(
    const WorldBundle &world, const CompleteExactWorldBindingOwner &owner,
    NativeTaskWorkDependencies d, const WorldMoment &at) {
  auto lease = checkpoint_make_shared<NativeTaskWorkInOwnerReadLease>(
      world, owner, std::move(d), at);
  auto valid = lease->validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true, std::move(lease), {}};
}
Outcome<std::shared_ptr<TaskWorkWriteLease>>
make_native_task_work_write_lease_in_owner(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner,
    NativeTaskWorkWriteDependencies d, const WorldMoment &at) {
  auto lease = checkpoint_make_shared<NativeTaskWorkInOwnerWriteLease>(
      world, owner, std::move(d), at);
  auto valid = lease->validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true, std::move(lease), {}};
}
} // namespace hanmo::resident
