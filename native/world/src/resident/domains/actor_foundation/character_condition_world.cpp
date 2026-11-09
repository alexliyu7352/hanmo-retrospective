#include "character_condition_world.hpp"
#include "born_origin.hpp"
#include "condition_application_fact.hpp"
#include "effective_attributes_v1.hpp"
#include "injury_sources_v1_state.hpp"
#include "injury_service_coverage_v1.hpp"
#include "named_custody_availability_internal.hpp"
#include "resident/domains/craft/craft_job.hpp"
#include "resident/domains/estate/estate_rest_history.hpp"
#include "resident/domains/family/profile_control_eligibility.hpp"
#include "resident/domains/growth/growth_action_time_read.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/growth/growth_quiet_source.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
namespace {
// Only the original selected World root may feed Condition8. This is not a
// fresh initializer or a substitute for full injury/episode validation.
Status
validate_injury_world_bindings(const WorldBundle &w,
                               const CharacterConditionWorldDependencies &d,
                               bool selected, const WorldMoment &actual_now) {
  const auto *root =
      w.character_injury_sources ? &*w.character_injury_sources : nullptr;
  auto layout =
      validate_character_injury_sources_v1_layout(w.catalog->identity, root);
  if (!layout.ok)
    return layout;
  if (d.injury_sources != root)
    return common_failure("CharacterInjuryWorldRootMismatch", {},
                          ErrorKind::internal);
  if (!selected) {
    if (d.attribute_absence)
      return common_failure("CharacterInjuryUnselectedAbsenceProvider", {},
                            ErrorKind::internal);
    return common_success();
  }
  if (!d.attribute_absence)
    return common_failure("CharacterAttributeAbsenceWorldSourceRequired", {},
                          ErrorKind::unsupported_capability);
  if (!d.attribute_absence->bound_to(w.common, *w.actor_foundation,
                                     *w.catalog->actor_foundation))
    return common_failure("CharacterAttributeAbsenceWorldBindingMismatch", {},
                          ErrorKind::internal);
  return d.attribute_absence->validate_finite_attribute_source_absence(
      actual_now);
}
class WorldRestTimeRead final : public AuthoredRestTimeRead {
  const WorldBundle &world_;
  const CompleteExactTimeRuntime &runtime_;

public:
  WorldRestTimeRead(const WorldBundle &w, const CompleteExactTimeRuntime &r)
      : world_(w), runtime_(r) {}
  Outcome<WorldMoment>
  completed_rest_at(const StableId &action) const override {
    const auto *activity = world_.growth.life_activities.find(action);
    if (!activity || activity->view.kind != LifeActivityKind::service ||
        activity->view.activity != "rest" ||
        activity->view.phase != LifeActivityPhase::completed)
      return {false,
              {},
              common_failure("OriginalRestCompletedSourceRequired", action,
                             ErrorKind::internal)
                  .error};
    const ActionTimeRegistration *selected = nullptr;
    for (const auto &r : runtime_.action_times)
      if (r.owner_domain == "growth" && r.action_kind == "service") {
        if (selected)
          return {false,
                  {},
                  common_failure("DuplicateGrowthServiceRegistration", action,
                                 ErrorKind::internal)
                      .error};
        selected = &r;
      }
    if (!selected)
      return {false,
              {},
              common_failure("OriginalRestTimeRegistrationRequired", action,
                             ErrorKind::unsupported_capability)
                  .error};
    auto at = world_.actual_phase_moment();
    if (!at.ok)
      return {false, {}, at.error};
    GrowthActionTimeRead original(
        world_.common, world_.growth, *world_.catalog->growth,
        *world_.catalog->common, *world_.action_elapsed,
        world_.world_timeline->scale_ref, world_.catalog->identity, at.value,
        {}, &world_.interaction, world_.catalog->interaction.get());
    auto actual = read_growth_original_time(original, *selected, action);
    if (!actual.ok)
      return {false, {}, actual.error};
    if (!actual.value.terminal || !actual.value.ended_at)
      return {false,
              {},
              common_failure("OriginalRestExactTerminalRequired", action,
                             ErrorKind::internal)
                  .error};
    return {true, *actual.value.ended_at, {}};
  }
};
} // namespace
struct CharacterConditionWorldRead::Impl final : CurrentMomentRead {
  const WorldBundle &world;
  const CompleteExactTimeRuntime &runtime;
  CharacterConditionWorldDependencies dependencies;
  WorldRestTimeRead rest;
  std::optional<FamilyLifeCauseSources4> life;
  std::optional<EstateRestHistoryContext> estates;
  ProfileControlEligibilitySources control;
  // 原Field clock借用由complete owner保有；历史包装先于quiet构造、后于quiet析构。
  std::optional<MilitaryFieldHistoryRead> quiet_field_history;
  std::optional<NativeGrowthQuietRead> quiet;
  // 只有完整拥有者发布前可补入一次；不缓存可用性结果或原 M。
  const NativeNamedCustodyAvailabilityRead *named_custody = nullptr;
  Outcome<WorldMoment> read_current_moment() const override {
    return world.actual_phase_moment();
  }
  Impl(const WorldBundle &w, const CompleteExactTimeRuntime &r,
       CharacterConditionWorldDependencies d, bool injury_selected)
      : world(w), runtime(r), dependencies(d), rest(w, r),
        control{w.common,
                w.family,
                *w.catalog->family,
                *w.catalog->common,
                w.family_legacy_enabled() ? &w.family_legacy : nullptr,
                &*w.actor_foundation,
                w.catalog->actor_foundation.get(),
                this} {
    if (w.catalog->family_life && w.catalog->family_life->enabled &&
        w.catalog->family_life->selected_version == 4)
      life.emplace(
          w.catalog->identity, w.military, d.military_exact, d.natural_age,
          d.environmental_sea, d.military_formations, d.military_returns,
          injury_selected && w.field_battle ? &*w.field_battle : nullptr,
          injury_selected ? d.field_application_clock : nullptr);
    if (w.catalog->estate)
      estates.emplace(EstateRestHistoryContext{
          w.common, *w.catalog->estate, w.family, *w.actor_foundation,
          *w.catalog->actor_foundation, *this,
          w.estate_employment_enabled() ? &w.estate_employment : nullptr});
    if (d.social_occupancy) {
      if (w.field_battle && d.field_application_clock)
        quiet_field_history.emplace(MilitaryFieldHistoryRead{
            *w.field_battle, *d.field_application_clock});
      quiet.emplace(w.common, w.military,
                    MilitarySelectedHistoryRead{w.catalog->identity,
                      d.military_exact,
                      quiet_field_history ? &*quiet_field_history : nullptr},
                    w.naval_enabled() ? &w.naval : nullptr, *d.social_occupancy,
                    *this, d.military_formations, d.military_returns);
    }
  }
};
CharacterConditionWorldRead::CharacterConditionWorldRead(
    CheckpointMemoryCharge memory, std::unique_ptr<Impl> impl)
    : object_memory_(std::move(memory)), impl_(std::move(impl)) {}
CharacterConditionWorldRead::~CharacterConditionWorldRead() = default;
Status CharacterConditionWorldRead::bind_original_named_custody(
    const NativeNamedCustodyAvailabilityRead &source) {
  const auto &w = impl_->world;
  if (impl_->named_custody ||
      !named_custody_availability_selected(*w.catalog->actor_foundation) ||
      !source.bound_to(w.common, w.military, *w.actor_foundation,
                       *w.catalog->actor_foundation))
    return common_failure("CompleteNamedCustodyBodyBindingMismatch");
  impl_->named_custody = &source;
  return common_success();
}
Outcome<std::unique_ptr<CharacterConditionWorldRead>>
CharacterConditionWorldRead::create(
    const WorldBundle &w, const CompleteExactTimeRuntime &runtime,
    CharacterConditionWorldDependencies dependencies) {
  if (!w.catalog || !w.catalog->actor_foundation || !w.catalog->growth ||
      !w.catalog->common || !w.catalog->family || !w.actor_foundation ||
      !w.action_elapsed || !w.world_timeline)
    return {false,
            {},
            common_failure("CompleteCharacterConditionWorldSourcesRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  // Life4按本身的实际选择构造，不能因旧军根未选而丢掉真实自然年龄源。
  const bool life4 = w.catalog->family_life &&
                     w.catalog->family_life->enabled &&
                     w.catalog->family_life->selected_version == 4;
  if ((dependencies.natural_age || dependencies.environmental_sea ||
       !w.family_life.deaths.empty()) &&
      !life4)
    return {false,
            {},
            common_failure("CompleteLife4SelectionRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  if (dependencies.military_exact !=
      (w.military_exact ? &*w.military_exact : nullptr))
    return {false,
            {},
            common_failure("CompleteLifeMilitaryCandidateMismatch", {},
                           ErrorKind::internal)
                .error};
  auto injury_selected = condition_application_facts_selected(
      w.catalog->identity, ConditionApplicationHistoryOwner::injury_sources);
  if (!injury_selected.ok)
    return {false, {}, injury_selected.error};
  if (life4) {
    auto selected = validate_family_death_policy_selection(
        *w.catalog->family_life, w.catalog->identity);
    if (!selected.ok)
      return {false, {}, selected.error};
    FamilyLifeCauseSources4 original(
        w.catalog->identity, w.military, dependencies.military_exact,
        dependencies.natural_age, dependencies.environmental_sea,
        dependencies.military_formations, dependencies.military_returns,
        injury_selected.value && w.field_battle ? &*w.field_battle : nullptr,
        injury_selected.value ? dependencies.field_application_clock : nullptr);
    selected =
        validate_family_life4_original_military_selection(w.common, original);
    if (!selected.ok)
      return {false, {}, selected.error};
  }
  if (dependencies.field_application_clock &&
      (!w.field_battle || !dependencies.field_application_clock->bound_to(
                              w.common, w.military, *w.field_battle)))
    return {false,
            {},
            common_failure("CompleteCharacterFieldClockBindingRequired").error};
  if (dependencies.social_occupancy &&
      !dependencies.social_occupancy->bound_to(w.common))
    return {false,
            {},
            common_failure("GrowthQuietSameCandidateRequired", {},
                           ErrorKind::internal)
                .error};
  // 显式选型闭合Quiet的真实军根，未选dynamic时不造空旧根；Field缺clock仍拒绝。
  if (dependencies.social_occupancy) {
    auto selected = validate_selected_military_roster_root(
        w.common, w.catalog->identity, dependencies.military_exact);
    if (!selected.ok) return {false, {}, selected.error};
    std::optional<MilitaryFieldHistoryRead> field;
    if (w.field_battle && dependencies.field_application_clock)
      field.emplace(MilitaryFieldHistoryRead{
          *w.field_battle, *dependencies.field_application_clock});
    selected = validate_selected_military_field_history(
        w.common, w.military, w.catalog->identity, field ? &*field : nullptr);
    if (!selected.ok) return {false, {}, selected.error};
  }
  if (w.catalog->growth->complete_capability_application_chain) {
    if (!w.growth_permanent || !w.catalog->growth_permanent ||
        !dependencies.work_practice)
      return {false,
              {},
              common_failure("GrowthPermanentWorldSourcesRequired", {},
                             ErrorKind::unsupported_capability)
                  .error};
    if (!dependencies.work_practice->bound_to(w.common))
      return {false,
              {},
              common_failure("GrowthPermanentSameWorldSourcesRequired", {},
                             ErrorKind::internal)
                  .error};
  }
  if (dependencies.born_sources &&
      !dependencies.born_sources->bound_to(w.common))
    return {
        false,
        {},
        common_failure("BornOriginCandidateMismatch", {}, ErrorKind::internal)
            .error};
  if (dependencies.craft && &dependencies.craft->materials.common != &w.common)
    return {false,
            {},
            common_failure("CraftConditionCandidateMismatch", {},
                           ErrorKind::internal)
                .error};
  auto at = w.actual_phase_moment();
  if (!at.ok)
    return {false, {}, at.error};
  auto injury_bindings = validate_injury_world_bindings(
      w, dependencies, injury_selected.value, at.value);
  if (!injury_bindings.ok)
    return {false, {}, injury_bindings.error};
  CheckpointMemoryCharge memory(0);
  auto impl =
      std::make_unique<Impl>(w, runtime, dependencies, injury_selected.value);
  return {
      true,
      std::unique_ptr<CharacterConditionWorldRead>(
          new CharacterConditionWorldRead(std::move(memory), std::move(impl))),
      {}};
}
Outcome<CharacterConditionSources>
CharacterConditionWorldRead::sources() const {
  const auto &w = impl_->world;
  auto at = w.actual_phase_moment();
  // Outcome of a reference aggregate has no default error value. Preserve the
  // bound same-world references even on failure, and require the caller's ok
  // gate.
  CharacterConditionSources result{
      w.common, *w.actor_foundation, *w.catalog->actor_foundation, {}};
  if (!at.ok)
    return {false, std::move(result), at.error};
  result.actual_now = at.value;
  result.interaction = &w.interaction;
  result.interaction_catalog = w.catalog->interaction.get();
  result.growth = &w.growth;
  result.growth_catalog = w.catalog->growth.get();
  result.shared = w.catalog->common.get();
  result.elapsed = &*w.action_elapsed;
  result.scale = &w.world_timeline->scale_ref;
  result.action_registrations = &impl_->runtime.action_times;
  result.rest_time = &impl_->rest;
  result.movement = w.movement_exact ? &*w.movement_exact : nullptr;
  result.deaths = w.family_life_enabled() ? &w.family_life : nullptr;
  result.life_sources = impl_->life ? &*impl_->life : nullptr;
  result.military = &w.military;
  result.military_exact = impl_->dependencies.military_exact;
  result.named_custody_availability = impl_->named_custody;
  result.personal_catalog = w.catalog->personal.get();
  result.family_catalog = w.catalog->family.get();
  result.rest_estates = impl_->estates ? &*impl_->estates : nullptr;
  result.fixed_spatial = impl_->dependencies.fixed_spatial;
  result.control = &impl_->control;
  result.director = &w.director;
  result.director_catalog = w.catalog->director.get();
  result.fixed_quiet = impl_->quiet ? &*impl_->quiet : nullptr;
  result.rest_estate_state = w.catalog->estate ? &w.estate : nullptr;
  result.permanent_growth = w.growth_permanent ? &*w.growth_permanent : nullptr;
  result.permanent_growth_catalog = w.catalog->growth_permanent.get();
  result.work_practice = impl_->dependencies.work_practice;
  result.born_sources = impl_->dependencies.born_sources;
  result.craft = impl_->dependencies.craft;
  result.military_supply_periods = impl_->dependencies.military_supply_periods;
  result.military_formations = impl_->dependencies.military_formations;
  result.military_returns = impl_->dependencies.military_returns;
  auto injury_selected = condition_application_facts_selected(
      w.catalog->identity, ConditionApplicationHistoryOwner::injury_sources);
  if (!injury_selected.ok)
    return {false, std::move(result), injury_selected.error};
  auto injury_bindings = validate_injury_world_bindings(
      w, impl_->dependencies, injury_selected.value, result.actual_now);
  if (!injury_bindings.ok)
    return {false, std::move(result), injury_bindings.error};
  if (injury_selected.value) {
    result.injury_sources = impl_->dependencies.injury_sources;
    result.attribute_absence = impl_->dependencies.attribute_absence;
    result.injury_personal = &w.personal;
    result.injury_field = w.field_battle ? &*w.field_battle : nullptr;
    result.injury_field_clock = impl_->dependencies.field_application_clock;
    result.injury_recovery_catalog = w.catalog->personal_recovery.get();
    auto recovery = validate_injury_recovery_selection_v1(result);
    if (!recovery.ok)
      return {false, std::move(result), recovery.error};
    // The old seed marker is installation metadata, not recovery progress.
    // source1 uses the real catalog but never the old period-owned ledger.
    if ((!result.injury_recovery_catalog &&
         w.personal_recovery_seed_complete) ||
        !w.personal_recovery.services.empty() ||
        !w.personal_recovery.credits.empty() ||
        !w.personal_recovery.closures.empty()) {
      auto failure = common_failure(result.injury_recovery_catalog
                                        ? "CharacterInjuryMixedRecoveryOwners"
                                        : "PersonalInjuryRecoveryUnavailable");
      return {false, std::move(result), std::move(failure.error)};
    }
  }
  if (w.catalog->growth->complete_capability_application_chain) {
    if (!result.permanent_growth || !result.permanent_growth_catalog ||
        !result.work_practice)
      return {false, std::move(result),
              common_failure("GrowthPermanentWorldSourcesRequired", {},
                             ErrorKind::unsupported_capability)
                  .error};
    if (!result.work_practice->bound_to(w.common))
      return {false, std::move(result),
              common_failure("GrowthPermanentSameWorldSourcesRequired", {},
                             ErrorKind::internal)
                  .error};
  }
  return {true, std::move(result), {}};
}
Status CharacterConditionWorldRead::validate() const {
  auto read = sources();
  if (!read.ok)
    return {false, {}, read.error};
  return validate_complete_character_conditions(read.value);
}
} // namespace hanmo::resident
