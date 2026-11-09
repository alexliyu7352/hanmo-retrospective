#include "native_character_family_world.hpp"
#include "native_actor_social_occupancy.hpp"
#include "native_control_continuation.hpp"
#include "native_growth_permanent_world.hpp"
#include "native_life_age_world.hpp"
#include "native_military_state.hpp"
#include "native_military_training_period.hpp"
#include "resident/domains/craft/craft_job.hpp"
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "resident/domains/family/born_character_source.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
#include "resident/domains/military/military_formation_source.hpp"
#include "resident/domains/military/military_return_source.hpp"
#include "resident/domains/travel/military_action_placement.hpp"
#include "world_bundle.hpp"
#include <new>
#include <stdexcept>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
[[noreturn]] void required(const char *code) {
  throw Failure{
      common_failure(code, {}, ErrorKind::unsupported_capability).error};
}
[[noreturn]] void mismatch(const char *code) {
  throw Failure{common_failure(code, {}, ErrorKind::internal).error};
}
template <class T, class F> Outcome<T> guard(F f) {
  try {
    return {true, f(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("CharacterFamilyOwningResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("CharacterFamilyOwningResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
const NativeActorSocialOccupancyRead *occupancy(const WorldBundle &w) {
  const auto *owner = w.services.actor_social_occupancy_world();
  return owner ? &owner->read() : nullptr;
}
// 校验同候选已选拥有根；未选能力仅沿原阴性门，不能据空表伪造来源。
void selected(const WorldBundle &w, const CompleteExactTimeRuntime &runtime,
              const NativeCharacterFamilyDependencies &d,
              bool original_birth = false, bool defer_relations = false) {
  if (!w.catalog || !w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time) || !w.catalog->common ||
      !w.catalog->family || !w.catalog->actor_foundation ||
      !w.catalog->growth || !w.catalog->family_life ||
      !w.catalog->family_life->enabled ||
      w.catalog->family_life->selected_version != 4 || !w.actor_foundation ||
      !w.world_timeline || !w.action_elapsed || !w.movement_exact ||
      !w.spatial_carriers || !w.transport_registrations)
    required("CharacterFamilyActualOwningRootsRequired");
  if (w.catalog->complete_exact_time_runtime.get() != &runtime)
    mismatch("CharacterFamilyRuntimeCandidateMismatch");
  take(validate_native_military_state_layout(w));
  take(validate_native_life_age_layout(w));
  take(validate_native_growth_permanent_layout(w));
  take(validate_native_control_continuation_layout(w));
  if (&d.spatial.movement != &*w.movement_exact ||
      &d.spatial.carriers != &*w.spatial_carriers ||
      &d.spatial.registrations != &*w.transport_registrations)
    mismatch("CharacterFamilySpatialCandidateMismatch");
  if (w.catalog->actor_foundation->natural_age_death_rule &&
      !w.catalog->life_age_checks)
    required("CharacterFamilyOriginalNaturalAgeProviderRequired");
  if (w.catalog->actor_foundation->sea_death_rule && !d.sea)
    required("CharacterFamilyOriginalSeaProviderRequired");
  if (original_birth) {
    if (!w.catalog->family_birth || !w.catalog->family_birth->enabled ||
        w.catalog->family_birth->selected_version != 4 ||
        !w.catalog->family_lineage || !w.catalog->family_lineage->enabled ||
        !w.catalog->interaction ||
        w.catalog->interaction->common.get() != w.catalog->common.get())
      required("CharacterFamilyOriginalBirth4RootsRequired");
  }
  if (d.born && !d.born->bound_to(w.common))
    mismatch("CharacterFamilyBornCandidateMismatch");
  if (w.catalog->family_birth &&
      w.catalog->family_birth->selected_version == 4 && !d.born &&
      !original_birth)
    required("CharacterFamilyOriginalBornProviderRequired");
  for (const auto &row :
       w.actor_foundation->character_origin_records.origins.rows())
    if (row.second.origin_kind == CharacterOriginRecordV2::Kind::born &&
        !d.born && !original_birth)
      required("CharacterFamilyOriginalBornProviderRequired");
  for (const auto &module : w.catalog->identity.module_versions) {
    if (module.first == "craft_jobs" && !d.craft)
      required("CharacterFamilyOriginalCraftProviderRequired");
  }
  if (w.control_continuation && !defer_relations) {
    if (w.catalog->family_disciple_legacy && !d.disciple)
      required("CharacterFamilyOriginalDiscipleContextRequired");
    if (w.catalog->family_biological_legacy && !d.biological)
      required("CharacterFamilyOriginalBiologicalContextRequired");
    if (w.catalog->family_collateral_legacy && !d.collateral)
      required("CharacterFamilyOriginalCollateralContextRequired");
  }
  if (d.craft && &d.craft->materials.common != &w.common)
    mismatch("CharacterFamilyCraftCandidateMismatch");
  if (d.military_supply_periods &&
      (!w.production_cycles ||
       !d.military_supply_periods->bound_to(w.common, *w.production_cycles)))
    mismatch("CharacterFamilyMilitaryPeriodCandidateMismatch");
  if ((w.catalog->actor_foundation->original_military_supply_shortage_rule ||
       (w.military_exact &&
        !w.military_exact->supply_effect_sources.empty())) &&
      !d.military_supply_periods)
    required("CharacterFamilyOriginalMilitarySupplyPeriodRequired");
  if (d.military_formations &&
      !d.military_formations->bound_to(w.common.state(), w.military))
    mismatch("CharacterFamilyFormationCandidateMismatch");
  if (d.military_returns && !d.military_returns->bound_to(w.common, w.military))
    mismatch("CharacterFamilyReturnCandidateMismatch");
  if (d.spatial.military_actions &&
      !d.spatial.military_actions->bound_to(w.common, *w.movement_exact))
    mismatch("CharacterFamilyMilitaryPlacementCandidateMismatch");
  for (const auto &row : w.common.state().facts.rows()) {
    if (row.second.domain == "military" &&
        row.second.kind == "military_action_completion_applied" &&
        (!d.military_formations || !d.spatial.military_actions))
      required("CharacterFamilyOriginalFormationHistoryRequired");
    if (row.second.domain == "military" &&
        row.second.kind == "military_logistics_completion_applied" &&
        (!d.military_returns || !d.spatial.military_actions))
      required("CharacterFamilyOriginalReturnHistoryRequired");
  }
  // Complete预置长休规则不代表作者选择了长休行动；只按原已采用行动登记判断。
  bool long_rest_selected = false;
  for (const auto &action : runtime.action_times)
    if (action.owner_domain == "growth" && action.action_kind == "long_rest") {
      long_rest_selected = true;
      break;
    }
  // 原未选合同先反查残留；显式占用或真正登记的长休仍必须有原读取器。
  const auto *social = occupancy(w);
  if (!w.catalog->actor_social_occupancy_selected)
    take(validate_native_actor_social_occupancy(w));
  if ((w.catalog->actor_social_occupancy_selected || long_rest_selected) &&
      !social)
    required("CharacterFamilyOriginalSocialOccupancyRequired");
  if (social && !social->bound_to(w.common))
    mismatch("CharacterFamilySocialCandidateMismatch");
  take(w.actual_phase_moment());
}
} // namespace
NativeCharacterFamilyWorldRead::NativeCharacterFamilyWorldRead(
    const WorldBundle &w, const CompleteExactTimeRuntime &runtime,
    NativeCharacterFamilyDependencies d, OriginalCalendarRead calendar,
    bool original_birth, bool defer_relations)
    : world_(w), runtime_(runtime), dependencies_(d), clock_(w),
      calendar_(std::move(calendar)),
      control_{w.common,
               w.family,
               *w.catalog->family,
               *w.catalog->common,
               w.family_legacy_enabled() ? &w.family_legacy : nullptr,
               &*w.actor_foundation,
               w.catalog->actor_foundation.get(),
               &clock_,
               w.control_continuation ? &*w.control_continuation : nullptr},
      birth_access_(
          original_birth
              ? std::optional<BirthArrangementAccess>{w.family_birth_access()}
              : std::nullopt),
      birth_context_(
          original_birth
              ? std::optional<
                    BornCharacterSourceContext>{BornCharacterSourceContext{
                    w.common, *w.catalog->common, *w.catalog->family,
                    *w.catalog->family_lineage, *w.catalog->interaction,
                    *w.catalog->family_birth, w.family_birth, *birth_access_}}
              : std::nullopt),
      born_(original_birth
                ? std::optional<NativeBornCharacterSourceRead>{std::in_place,
                                                               *birth_context_}
                : std::nullopt),
      ages_{w.common,  *w.actor_foundation,     *w.catalog->actor_foundation,
            w.family,  w.family_birth,          control_,
            calendar_, born_ ? &*born_ : d.born},
      work_(w.common, w.tasks) {
  if (born_)
    dependencies_.born = &*born_;
  // 年龄与身体读取同一显式 Condition8/Field 组合，不能丢掉早期 owner 的两条借用。
  const bool injury_selected = take(condition_application_facts_selected(
      w.catalog->identity, ConditionApplicationHistoryOwner::injury_sources));
  if (w.catalog->life_age_checks)
    age_.emplace(ages_, *w.catalog->life_age_checks, *w.life_age_checks,
                 w.family_life, clock_, w.catalog->identity, w.military,
                 w.military_exact ? &*w.military_exact : nullptr, d.sea,
                 d.military_formations, d.military_returns,
                 injury_selected && w.field_battle ? &*w.field_battle : nullptr,
                 injury_selected ? d.field_application_clock : nullptr);
  body_ = take(CharacterConditionWorldRead::create(
      w, runtime,
      {w.military_exact ? &*w.military_exact : nullptr, age_ ? &*age_ : nullptr,
       d.sea, &d.spatial, occupancy(w), &work_, dependencies_.born, d.craft,
       d.military_supply_periods, d.military_formations, d.military_returns,
       d.field_application_clock, d.injury_sources, d.attribute_absence}));
  const auto sources = take(body_->sources());
  if (!sources.life_sources)
    required("CharacterFamilyActualLifeCauseOwnerRequired");
  // Borrow the very same cause owner used by body/appointments, after its
  // actual age provider exists. No fake military origin and no recursive read.
  life_.emplace(w.common, w.family_life, *w.catalog->family_life,
                w.catalog->identity, *sources.life_sources, clock_);
  people_.emplace(ages_, *life_, clock_,
                  w.control_continuation ? &*w.control_continuation : nullptr);
  if (birth_access_) {
    // All consumers above only bind references. Do not run a Born/age/body
    // history read until this one-time same-owner closure is complete.
    birth_access_->people = &*people_;
    birth_access_->foundation = &*w.actor_foundation;
    birth_access_->foundation_catalog = w.catalog->actor_foundation.get();
    birth_access_->placement = d.birth_placement;
    birth_access_->born_sources = d.birth_results;
  }
  if (!defer_relations)
    bind_relation_sources({d.disciple, d.biological, d.collateral});
}
Status NativeCharacterFamilyWorldRead::bind_original_named_custody(
    const NativeNamedCustodyAvailabilityRead &source) {
  if (!body_ || !relations_bound_)
    return common_failure("CompleteNamedCustodyFamilyBindingOrder");
  return body_->bind_original_named_custody(source);
}
void NativeCharacterFamilyWorldRead::bind_relation_sources(
    NativeCharacterFamilyRelationSources sources) {
  if (relations_bound_ || !people_ || continuation_ || origins_)
    mismatch("CharacterFamilyRelationBindingOrderMismatch");
  const auto &w = world_;
  dependencies_.disciple = sources.disciple;
  dependencies_.biological = sources.biological;
  dependencies_.collateral = sources.collateral;
  // 晚源只补机械反向引用；原选择缺源拒绝在发布之前照常执行。
  selected(w, runtime_, dependencies_, bool(born_));
  if (w.control_continuation) {
    origins_.emplace(FamilyOriginV2Sources{
        *w.actor_foundation, *w.catalog->actor_foundation, clock_});
    continuation_.emplace(NativeContinuationSources{
        w.common, *w.control_continuation, w.family, *w.catalog->family,
        *w.catalog->common, *w.catalog->interaction, w.family_life,
        w.family_legacy, *w.catalog->family_legacy, *people_, *origins_,
        sources.disciple, sources.biological, sources.collateral});
  }
  relations_bound_ = true;
}
Outcome<std::unique_ptr<NativeCharacterFamilyWorldRead>>
NativeCharacterFamilyWorldRead::create_for_complete_assembly(
    const WorldBundle &w, const CompleteExactTimeRuntime &runtime,
    NativeCharacterFamilyDependencies dependencies, bool original_birth) {
  return create_impl(w, runtime, dependencies, original_birth, true);
}
Outcome<std::unique_ptr<NativeCharacterFamilyWorldRead>>
NativeCharacterFamilyWorldRead::create(
    const WorldBundle &w, const CompleteExactTimeRuntime &runtime,
    NativeCharacterFamilyDependencies dependencies) {
  return create_impl(w, runtime, dependencies, false);
}
Outcome<std::unique_ptr<NativeCharacterFamilyWorldRead>>
NativeCharacterFamilyWorldRead::create_with_original_birth(
    const WorldBundle &w, const CompleteExactTimeRuntime &runtime,
    NativeCharacterFamilyDependencies dependencies) {
  return create_impl(w, runtime, dependencies, true);
}
Outcome<std::unique_ptr<NativeCharacterFamilyWorldRead>>
NativeCharacterFamilyWorldRead::create_impl(
    const WorldBundle &w, const CompleteExactTimeRuntime &runtime,
    NativeCharacterFamilyDependencies dependencies, bool original_birth,
    bool defer_relations) {
  return guard<std::unique_ptr<NativeCharacterFamilyWorldRead>>([&] {
    if (original_birth && dependencies.born)
      mismatch("CharacterFamilyOriginalBornSourceConflict");
    selected(w, runtime, dependencies, original_birth, defer_relations);
    const CalendarRuleRef *rule =
        w.catalog->life_age_checks
            ? &w.catalog->life_age_checks->calendar_profile_ref
            : nullptr;
    auto calendar =
        rule ? take(OriginalCalendarRead::admit(w.catalog->identity, *rule))
             : take(OriginalCalendarRead::admit(
                   w.catalog->identity, {original_calendar_rule_id, 1,
                                         w.catalog->identity.content_digest}));
    auto owner = std::unique_ptr<NativeCharacterFamilyWorldRead>(
        new NativeCharacterFamilyWorldRead(w, runtime, dependencies,
                                           std::move(calendar), original_birth,
                                           defer_relations));
    if (!defer_relations)
      take(owner->validate_bindings());
    return owner;
  });
}
Status NativeCharacterFamilyWorldRead::validate_bindings() const {
  return guard<Done>([&] {
    if (!relations_bound_)
      mismatch("CharacterFamilyRelationSourcesNotBound");
    selected(world_, runtime_, dependencies_, bool(born_));
    if (bool(birth_access_) != bool(birth_context_) ||
        bool(birth_context_) != bool(born_))
      mismatch("CharacterFamilyOriginalBornOwningGraphMismatch");
    if (born_) {
      if (!people_ || dependencies_.born != &*born_ ||
          ages_.born_sources != &*born_ ||
          &birth_context_->common != &world_.common ||
          &birth_context_->common_catalog != world_.catalog->common.get() ||
          &birth_context_->family_catalog != world_.catalog->family.get() ||
          &birth_context_->lineage != world_.catalog->family_lineage.get() ||
          &birth_context_->interactions != world_.catalog->interaction.get() ||
          &birth_context_->catalog != world_.catalog->family_birth.get() ||
          &birth_context_->births != &world_.family_birth ||
          &birth_context_->access != &*birth_access_ ||
          birth_access_->lineage != world_.catalog->family_lineage.get() ||
          birth_access_->life != &world_.family_life ||
          birth_access_->people != &*people_ ||
          birth_access_->foundation != &*world_.actor_foundation ||
          birth_access_->foundation_catalog !=
              world_.catalog->actor_foundation.get() ||
          birth_access_->placement != dependencies_.birth_placement ||
          birth_access_->born_sources != dependencies_.birth_results)
        mismatch("CharacterFamilyOriginalBornCandidateMismatch");
    }
    if (!body_ || !life_ || !people_ ||
        bool(age_) != bool(world_.catalog->life_age_checks))
      mismatch("CharacterFamilyIncompleteOwningGraph");
    const auto body = take(body_->sources());
    const auto at = take(clock_.read_current_moment());
    if (&body.common != &world_.common ||
        &body.state != &*world_.actor_foundation ||
        body.deaths != &world_.family_life || !body.life_sources ||
        body.military_exact !=
            (world_.military_exact ? &*world_.military_exact : nullptr) ||
        body.life_sources->military_exact != body.military_exact ||
        body.life_sources->catalog_selection != &world_.catalog->identity ||
        body.life_sources->age != (age_ ? &*age_ : nullptr) ||
        body.life_sources->sea != dependencies_.sea ||
        body.born_sources != dependencies_.born ||
        body.fixed_spatial != &dependencies_.spatial ||
        body.work_practice != &work_ || body.craft != dependencies_.craft ||
        body.military_supply_periods != dependencies_.military_supply_periods ||
        body.military_formations != dependencies_.military_formations ||
        body.military_returns != dependencies_.military_returns ||
        body.life_sources->military_formations !=
            dependencies_.military_formations ||
        body.life_sources->military_returns != dependencies_.military_returns ||
        !people_->bound_to(world_.common, world_.family_life) ||
        people_->control_continuation() != control_.control_continuation ||
        take(compare_world_moments(body.actual_now, at)) ||
        take(compare_world_moments(take(people_->read_current_moment()), at)))
      mismatch("CharacterFamilyOwningCandidateMismatch");
    if (bool(continuation_) != bool(world_.control_continuation) ||
        (continuation_ &&
         !continuation_->bound_to(world_.common, world_.family,
                                  *world_.control_continuation)))
      mismatch("CharacterFamilyContinuationCandidateMismatch");
    if (age_) {
      const auto age = age_->bindings();
      if (&age.sources.common != &world_.common ||
          &age.sources.foundation != &*world_.actor_foundation ||
          &age.deaths != &world_.family_life ||
          &age.life_sources.military != &world_.military ||
          age.life_sources.military_exact !=
              (world_.military_exact ? &*world_.military_exact : nullptr) ||
          age.life_sources.catalog_selection != &world_.catalog->identity ||
          age.life_sources.military_formations !=
              dependencies_.military_formations ||
          age.life_sources.military_returns != dependencies_.military_returns ||
          age.life_sources.field_battles != body.life_sources->field_battles ||
          age.life_sources.field_application_clock !=
              body.life_sources->field_application_clock ||
          take(
              compare_world_moments(take(age.clock.read_current_moment()), at)))
        mismatch("CharacterFamilyLifeAgeOwningCandidateMismatch");
    }
    return Done{};
  });
}
Status NativeCharacterFamilyWorldRead::validate_original_sources() const {
  return guard<Done>([&] {
    take(validate_bindings());
    take(body_->validate());
    take(life_->validate());
    if (age_) {
      const auto b = age_->bindings();
      take(validate_life_age_state(b.sources, b.catalog, b.state, b.deaths,
                                   take(clock_.read_current_moment())));
    }
    return Done{};
  });
}
} // namespace hanmo::resident
