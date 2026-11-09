#include "director_native_lease.hpp"
#include "resident/domains/commerce/private_shop_absence.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/native_private_shop_operations.hpp"
#include "resident/kernel/native_task_work_catalog.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
namespace {
Status validate_native_director_owner_snapshot(
    const WorldBundle &world, const CompleteExactWorldBindingOwner &owner,
    const NativeDirectorExactDependencies &d, const WorldMoment &at) {
  if (&d.world != &world || !world.catalog || !world.catalog->exact_time ||
      !exact_complete_selected(*world.catalog->exact_time) ||
      !world.catalog->complete_exact_time_runtime || !world.movement_exact ||
      &owner.reads().runtime !=
          world.catalog->complete_exact_time_runtime.get() ||
      &owner.reads().movement != &*world.movement_exact)
    return common_failure("NativeDirectorCompleteOwnerMismatch", {},
                          ErrorKind::internal);
  auto actual = world.actual_phase_moment();
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  for (const auto *moment : {static_cast<const WorldMoment *>(&actual.value),
                             &d.conditions.actual_now}) {
    auto same = compare_world_moments(at, *moment);
    if (!same.ok)
      return {false, {}, std::move(same.error)};
    if (same.value)
      return common_failure("NativeDirectorLeaseMomentMismatch", {},
                            ErrorKind::internal);
  }
  return common_success();
}
} // namespace

NativeDirectorExactSources::NativeDirectorExactSources(
    NativeDirectorExactDependencies d)
    : dependencies_(std::move(d)),
      work_(dependencies_.work
                ? std::optional<DirectorWorkWakeRead2>{DirectorWorkWakeRead2{
                      dependencies_.actors.common, dependencies_.actors.tasks,
                      dependencies_.work->definitions(),
                      dependencies_.work->original(),
                      dependencies_.work->original().supervision_sources(),
                      dependencies_.conditions.actual_now}}
                : std::nullopt),
      cohort_context_(
          dependencies_.household
              ? std::optional<
                    DirectorCohortSourceContext2>{DirectorCohortSourceContext2{
                    dependencies_.actors.state, dependencies_.actors.catalog,
                    *dependencies_.household, dependencies_.world.social,
                    dependencies_.results, dependencies_.result_stamp}}
              : std::nullopt),
      cohorts_(
          cohort_context_
              ? std::optional<NativeDirectorCohortSources2>{std::in_place,
                                                            *cohort_context_}
              : std::nullopt),
      cohort_purchase_(
          dependencies_.household && dependencies_.household_knowledge
              ? std::optional<
                    CohortPurchaseDirectorRoute2>{std::in_place,
                                                  dependencies_.actors.state,
                                                  *dependencies_.household,
                                                  *dependencies_
                                                       .household_knowledge}
              : std::nullopt),
      subject_context_{dependencies_.actors,
                       dependencies_.activity,
                       dependencies_.world.interaction,
                       dependencies_.world.social,
                       dependencies_.results,
                       dependencies_.result_stamp,
                       dependencies_.clock,
                       dependencies_.actor_effects,
                       dependencies_.authorities,
                       dependencies_.conditions,
                       work_ ? &*work_ : nullptr,
                       cohorts_ ? &*cohorts_ : nullptr,
                       cohort_purchase_ ? &*cohort_purchase_ : nullptr,
                       dependencies_.institutions,
                       std::move(dependencies_.conversation_response)},
      subjects_(subject_context_) {}
Status NativeDirectorExactSources::validate_bindings() const {
  const auto &d = dependencies_;
  const auto &w = d.world;
  const auto &a = d.actors;
  if (!w.catalog || !w.catalog->director || !w.catalog->director->subjects2 ||
      !w.director.subject2 || !w.catalog->actor_foundation ||
      &a.common != &w.common || &a.state != &w.director ||
      &a.catalog != w.catalog->director.get() || &a.tasks != &w.tasks ||
      &a.affairs != &w.affairs || &d.conditions.common != &w.common ||
      &d.conditions.catalog != w.catalog->actor_foundation.get() ||
      &d.authorities.common != &w.common ||
      !d.actor_effects.bound_to(w.common) ||
      !subjects_.bound_to(w.common, w.director) ||
      bool(d.household) != bool(d.household_knowledge) ||
      bool(d.household) != bool(d.household_owner))
    return common_failure("NativeDirectorExactSnapshotMismatch", {},
                          ErrorKind::internal);
  auto valid = validate_native_task_work_layout(*w.catalog, w.tasks);
  if (!valid.ok)
    return valid;
  auto selected = selected_native_task_work_version(w.catalog->identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (bool(selected.value) != bool(d.work))
    return common_failure("NativeDirectorWorkSelectionMismatch", {},
                          ErrorKind::unsupported_capability);
  if (d.work) {
    const auto &work = d.work->dependencies();
    if (&work.tasks.common != &w.common || &work.tasks.tasks != &w.tasks ||
        &work.tasks.clock != &d.clock || &work.body != &d.conditions)
      return common_failure("NativeDirectorExactSnapshotMismatch", {},
                            ErrorKind::internal);
    valid = d.work->validate_bindings();
    if (!valid.ok)
      return valid;
  } else {
    // 保留原反向覆盖：未选 Work 不容许游离的 Work
    // 事实、行动或生命周期命令历史。
    valid = validate_native_task_work_world(w);
    if (!valid.ok)
      return valid;
  }
  if (d.household) {
    const auto &h = *d.household;
    // 家户可独立于私铺选择；未选仍由原 owner 完整反验同世界残留。
    const auto shops =
        selected_native_private_shop_operation_versions(w.catalog->identity);
    if (!shops.ok)
      return {false, {}, shops.error};
    if (bool(shops.value.shops) != bool(w.catalog->private_shops))
      return common_failure("NativeDirectorHouseholdOwnerMismatch", {},
                            ErrorKind::internal);
    if (!shops.value.shops) {
      valid = validate_unselected_private_shop_sources(w);
      if (!valid.ok)
        return valid;
    }
    if (!w.population_transport || !w.household_purchases ||
        !w.facility_claims || !w.action_elapsed ||
        !w.household_retail_settlements || !w.transport_registrations ||
        !w.economic_transport_expenses || !w.catalog->common ||
        !w.catalog->growth || !w.catalog->household_purchases ||
        !w.catalog->household_retail_settlements || &h.common != &w.common ||
        &h.shared != w.catalog->common.get() ||
        &h.economy != &w.local_economy ||
        &h.population != &*w.population_transport ||
        &h.catalog != w.catalog->household_purchases.get() ||
        &h.state != &*w.household_purchases ||
        &h.claims != &*w.facility_claims || &h.elapsed != &*w.action_elapsed ||
        h.transport_registrations != &*w.transport_registrations ||
        &h.transport_expenses != &*w.economic_transport_expenses ||
        h.selected_household_version != w.catalog->selected_household_version ||
        &h.retail.common != &w.common ||
        &h.retail.common_catalog != w.catalog->common.get() ||
        &h.retail.shops != &w.private_shops ||
        h.retail.shops_catalog != w.catalog->private_shops.get() ||
        &h.retail.markets != w.catalog->growth.get() ||
        &h.retail.catalog != w.catalog->household_retail_settlements.get() ||
        &h.retail.state != &*w.household_retail_settlements ||
        d.household != &d.household_owner->household() ||
        (d.work &&
         d.household_owner != &d.work->dependencies().economy.household()))
      return common_failure("NativeDirectorHouseholdOwnerMismatch", {},
                            ErrorKind::internal);
    valid = d.household_owner->validate_bindings();
    if (!valid.ok)
      return valid;
  }
  auto actual = w.actual_phase_moment();
  if (!actual.ok)
    return {false, {}, actual.error};
  auto now = d.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(actual.value, now.value);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value)
    return common_failure("NativeDirectorActualPhaseMismatch", {},
                          ErrorKind::internal);
  if (d.household) {
    same = compare_world_moments(d.household->now, now.value);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value)
      return common_failure("NativeDirectorActualPhaseMismatch", {},
                            ErrorKind::internal);
  }
  if (d.propagation_selected != bool(d.propagation))
    return common_failure("NativeDirectorPropagationSelectionMismatch", {},
                          ErrorKind::unsupported_capability);
  if (d.propagation) {
    const auto &p = *d.propagation;
    if (&p.common != &w.common || &p.social != &w.social ||
        &p.director != &w.director || &p.director_catalog != &a.catalog ||
        &p.exact != &d.results || &p.exact_stamp != &d.result_stamp)
      return common_failure("NativeDirectorPropagationOwnerMismatch", {},
                            ErrorKind::internal);
    valid = validate_propagation_composition(p.policy, p.limits, p.channels);
    if (!valid.ok)
      return valid;
  }
  // The original owner alone checks actual selected admissions, appointment
  // sources, cohort routes and institutional ports. No parallel policy check.
  return subjects_.validate_bindings();
}
DirectorExactReadBinding NativeDirectorExactSources::read_binding() const {
  return {subjects_, dependencies_.propagation_selected,
          dependencies_.propagation};
}
Status NativeDirectorExactSources::validate_sources() const {
  auto valid = validate_bindings();
  if (!valid.ok)
    return valid;
  const auto &d = dependencies_;
  const auto &w = d.world;
  auto actual = w.actual_phase_moment();
  if (!actual.ok)
    return {false, {}, actual.error};
  return validate_complete_director_read_binding(w, actual.value,
                                                 read_binding());
}
NativeDirectorExactReadLease::NativeDirectorExactReadLease(
    std::unique_ptr<CompleteExactWorldBindingOwner> owner,
    NativeDirectorExactDependencies d)
    : owner_(std::move(owner)), actual_owner_(owner_.get()),
      sources_(std::move(d)) {}
NativeDirectorExactReadLease::NativeDirectorExactReadLease(
    const CompleteExactWorldBindingOwner &owner,
    NativeDirectorExactDependencies d)
    : actual_owner_(&owner), sources_(std::move(d)) {}
NativeDirectorExactReadLease::~NativeDirectorExactReadLease() = default;
Status NativeDirectorExactReadLease::validate_bindings() const {
  if (!actual_owner_)
    return common_failure("NativeDirectorWorldDependencyOwnerRequired", {},
                          ErrorKind::internal);
  if (!owner_) {
    const auto &d = sources_.dependencies();
    auto valid = validate_native_director_owner_snapshot(
        d.world, *actual_owner_, d, d.conditions.actual_now);
    if (!valid.ok)
      return valid;
  }
  return sources_.validate_bindings();
}
Status NativeDirectorExactReadLease::validate_sources() const {
  auto valid = validate_bindings();
  return valid.ok ? sources_.validate_sources() : valid;
}
NativeDirectorExactWriteLease::NativeDirectorExactWriteLease(
    std::unique_ptr<CompleteExactWorldBindingOwner> owner,
    NativeDirectorExactWriteDependencies d)
    : owner_(std::move(owner)), actual_owner_(owner_.get()),
      dependencies_(std::move(d)), sources_(std::move(dependencies_.read)),
      executor_(dependencies_.execution) {}
NativeDirectorExactWriteLease::NativeDirectorExactWriteLease(
    CompleteExactWorldBindingOwner &owner,
    NativeDirectorExactWriteDependencies d)
    : actual_owner_(&owner), dependencies_(std::move(d)),
      sources_(std::move(dependencies_.read)),
      executor_(dependencies_.execution) {}
NativeDirectorExactWriteLease::~NativeDirectorExactWriteLease() = default;
Status NativeDirectorExactWriteLease::validate_bindings() const {
  if (!actual_owner_)
    return common_failure("NativeDirectorWorldDependencyOwnerRequired", {},
                          ErrorKind::internal);
  if (!owner_) {
    const auto &d = sources_.dependencies();
    auto valid = validate_native_director_owner_snapshot(
        d.world, *actual_owner_, d, d.conditions.actual_now);
    if (!valid.ok)
      return valid;
  }
  auto valid = sources_.validate_bindings();
  if (!valid.ok)
    return valid;
  const auto &r = sources_.dependencies();
  const auto &x = dependencies_.execution;
  if (&x.common != &r.world.common || &x.director != &r.world.director ||
      &x.clock != &r.clock ||
      !executor_.bound_to(r.world.common, r.world.director) ||
      r.propagation_selected != bool(dependencies_.propagation_writer))
    return common_failure("NativeDirectorWriteCandidateMismatch", {},
                          ErrorKind::internal);
  if (r.household) {
    const auto *p = x.cohort_purchase;
    if (!p || &p->original != r.household ||
        &p->knowledge != r.household_knowledge ||
        &p->economy != &r.household->economy ||
        &p->purchases != &r.household->state ||
        &p->claims != &r.household->claims ||
        &p->elapsed != &r.household->elapsed ||
        &p->retail != &r.household->retail.state)
      return common_failure("NativeDirectorActualCohortWriterRequired", {},
                            ErrorKind::internal);
  }
  if (r.institutions &&
      (!x.institutions ||
       !x.institutions->bound_to(r.world.common, r.world.director)))
    return common_failure("NativeDirectorActualInstitutionalWriterRequired", {},
                          ErrorKind::unsupported_capability);
  if (x.household_routine &&
      !x.household_routine->bound_to(r.world.common, r.world.director))
    return common_failure("NativeDirectorHouseholdRoutineCandidateMismatch", {},
                          ErrorKind::internal);
  return common_success();
}
DirectorExactWriteBinding NativeDirectorExactWriteLease::writes() {
  const auto read = sources_.read_binding();
  return {read.original_subjects, executor_, read.propagation_selected,
          read.propagation, dependencies_.propagation_writer};
}
Outcome<std::shared_ptr<const DirectorExactReadLease>>
make_native_director_read_lease_in_owner(
    const WorldBundle &world, const CompleteExactWorldBindingOwner &owner,
    NativeDirectorExactDependencies d, const WorldMoment &at) {
  auto valid = validate_native_director_owner_snapshot(world, owner, d, at);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  auto lease =
      checkpoint_make_shared<NativeDirectorExactReadLease>(owner, std::move(d));
  valid = lease->validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true, std::move(lease), {}};
}
Outcome<std::shared_ptr<DirectorExactWriteLease>>
make_native_director_write_lease_in_owner(
    WorldBundle &world, CompleteExactWorldBindingOwner &owner,
    NativeDirectorExactWriteDependencies d, const WorldMoment &at) {
  auto valid =
      validate_native_director_owner_snapshot(world, owner, d.read, at);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  auto lease = checkpoint_make_shared<NativeDirectorExactWriteLease>(
      owner, std::move(d));
  valid = lease->validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true, std::move(lease), {}};
}
} // namespace hanmo::resident
