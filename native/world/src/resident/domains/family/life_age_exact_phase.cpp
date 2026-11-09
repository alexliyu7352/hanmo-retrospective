#include "life_age_exact_phase.hpp"
#include "care_life_condition.hpp"
#include "resident/domains/actor_foundation/character_condition_world.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/kernel/native_control_continuation.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
template <class F> Status checked(F f) {
  try {
    f();
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
void same(const WorldMoment &a, const WorldMoment &b) {
  if (take(compare_world_moments(a, b)))
    fail("LifeAgeExactClockMismatch");
}
struct Written {
  WorldBundle &world;
  ~Written() { world.note_write_boundary(); }
};
void binding(const WorldBundle &w, const LifeAgeExactReadBinding &b,
             const WorldMoment &at) {
  if (!w.life_age_checks || !w.life_age_checks_seed_complete || !w.catalog ||
      !w.catalog->life_age_checks || &b.state != &*w.life_age_checks ||
      &b.catalog != w.catalog->life_age_checks.get() ||
      &b.deaths != &w.family_life || !w.actor_foundation ||
      &b.sources.common != &w.common ||
      &b.sources.foundation != &*w.actor_foundation ||
      &b.sources.family != &w.family || &b.sources.births != &w.family_birth ||
      &b.sources.control.common != &w.common ||
      &b.sources.control.family != &w.family ||
      b.sources.control.foundation != &*w.actor_foundation ||
      b.sources.control.foundation_catalog != &b.sources.foundation_catalog ||
      !b.sources.control.actual_clock || !b.life_sources.age)
    fail("LifeAgeSameWorldReadRequired");
  same(at, take(w.actual_phase_moment()));
  same(at, take(b.clock.read_current_moment()));
  same(at, take(b.sources.control.actual_clock->read_current_moment()));
  take(validate_life_age_catalog(
      b.catalog, b.sources.foundation_catalog.identity, b.sources.calendar));
  const auto &rule = b.sources.foundation_catalog.natural_age_death_rule;
  if (!rule || !actor_foundation_rule_equal(*rule, b.catalog.policy_ref))
    fail("LifeAgeOriginalConditionRuleRequired", {},
         ErrorKind::unsupported_capability);
}
std::shared_ptr<const LifeAgeExactReadLease>
read(LifeAgeExactWorldFactory f, const WorldBundle &w, const WorldMoment &at) {
  take(validate_native_control_continuation_layout(w));
  if (!f.read || !f.write || !f.after_actual_life_write)
    fail("LifeAgeExactWorldFactoryRequired", {},
         ErrorKind::unsupported_capability);
  if (w.control_continuation && !f.after_actual_death)
    fail("LifeAgeContinuationDeathAdopterRequired", {},
         ErrorKind::unsupported_capability);
  auto lease = take(f.read(w, at));
  if (!lease)
    fail("LifeAgeExactReadLeaseRequired");
  binding(w, lease->bindings(), at);
  return lease;
}
std::shared_ptr<LifeAgeExactWriteLease>
write(LifeAgeExactWorldFactory f, WorldBundle &w, const WorldMoment &at) {
  take(validate_native_control_continuation_layout(w));
  if (!f.read || !f.write || !f.after_actual_life_write)
    fail("LifeAgeExactWorldFactoryRequired", {},
         ErrorKind::unsupported_capability);
  if (w.control_continuation && !f.after_actual_death)
    fail("LifeAgeContinuationDeathAdopterRequired", {},
         ErrorKind::unsupported_capability);
  auto lease = take(f.write(w, at));
  if (!lease)
    fail("LifeAgeExactWriteLeaseRequired");
  auto b = lease->bindings();
  auto r = b.original.bindings();
  binding(w, r, at);
  if (&b.common != &w.common || &b.foundation != &r.sources.foundation ||
      &b.state != &r.state || &b.deaths != &r.deaths ||
      &b.stamps != w.read_stamps.get())
    fail("LifeAgeSameWorldWriteRequired");
  same(at, take(b.current.read_current_moment()));
  return lease;
}
} // namespace
// 包装值可不同，真实来源根必须同候选；不因缺 reader 回退到无 episode。
Status validate_life_age_death_condition_sources(
    const LifeAgeExactReadBinding &original,
    const CharacterConditionSources *body, const WorldMoment &actual_at) {
  return checked([&] {
    if (!body)
      fail("LifeAgeActualConditionSourcesRequired", {},
           ErrorKind::unsupported_capability);
    if (!body->life_sources || !original.life_sources.age)
      fail("LifeAgeActualLifeSourcesRequired", {},
           ErrorKind::unsupported_capability);
    if (&body->common != &original.sources.common ||
        &body->state != &original.sources.foundation ||
        &body->catalog != &original.sources.foundation_catalog ||
        body->deaths != &original.deaths)
      fail("LifeAgeConditionCandidateMismatch");
    const auto &actual = *body->life_sources;
    const auto &expected = original.life_sources;
    if (&actual.military != &expected.military ||
        actual.military_exact != expected.military_exact ||
        actual.catalog_selection != expected.catalog_selection ||
        actual.age != expected.age || actual.sea != expected.sea ||
        actual.military_formations != expected.military_formations ||
        actual.military_returns != expected.military_returns ||
        actual.field_battles != expected.field_battles ||
        actual.field_application_clock != expected.field_application_clock ||
        body->military != &expected.military ||
        body->military_exact != expected.military_exact ||
        body->military_formations != expected.military_formations ||
        body->military_returns != expected.military_returns ||
        body->born_sources != original.sources.born_sources)
      fail("LifeAgeConditionOriginalSourcesMismatch");
    same(actual_at, body->actual_now);
    same(actual_at, take(original.clock.read_current_moment()));
  });
}
NativeLifeAgeExactRead::NativeLifeAgeExactRead(
    LifeAgeSources s, const LifeAgeCatalog &c, const LifeAgeState &state,
    const FamilyDeathState &deaths, const CurrentMomentRead &clock,
    const MilitaryState &military, const MilitaryExactState &exact,
    const EnvironmentalSeaDeathSourceRead *sea,
    const MilitaryFormationSourceRead *formations,
    const MilitaryReturnSourceRead *returns)
    : sources_(s), catalog_(c), state_(state), deaths_(deaths), clock_(clock),
      life_sources_{military, exact, this, sea, formations, returns} {}
// 可空旧军根只随同一实际目录进入；显式 Field 来源借同外层 owner，
// 不新增拥有根或伪造 clock，完整 validator 仍核选择、绑定与原事实。
NativeLifeAgeExactRead::NativeLifeAgeExactRead(
    LifeAgeSources s, const LifeAgeCatalog &c, const LifeAgeState &state,
    const FamilyDeathState &deaths, const CurrentMomentRead &clock,
    const CatalogIdentity &selected, const MilitaryState &military,
    const MilitaryExactState *exact, const EnvironmentalSeaDeathSourceRead *sea,
    const MilitaryFormationSourceRead *formations,
    const MilitaryReturnSourceRead *returns, const FieldBattleWorldState *field,
    const FieldSegmentApplicationRead *field_clock)
    : sources_(s), catalog_(c), state_(state), deaths_(deaths), clock_(clock),
      life_sources_{selected,   military, exact, this,       sea,
                    formations, returns,  field, field_clock} {}
LifeAgeExactReadBinding NativeLifeAgeExactRead::bindings() const {
  return {sources_, catalog_, state_, deaths_, life_sources_, clock_};
}
Outcome<LifeCauseView4> NativeLifeAgeExactRead::read_age_death_source(
    const NaturalAgeDeathCause4 &ref) const {
  LifeAgeDeathSourceRead source(sources_, catalog_, state_);
  return source.read_age_death_source(ref);
}
Outcome<LifeAgeCatalog>
decode_life_age_catalog(const BoundaryValue &config,
                        const CatalogIdentity &identity,
                        const OriginalCalendarRead &calendar) {
  try {
    using namespace boundary;
    fields(config, {"profile", "controlled_natural_death_enabled"}, {},
           "life_age_checks");
    if (get_string(config, "profile", "life_age_checks") !=
        "original_natural_age_70_v1")
      unsupported("UnsupportedLifeAgeProfile", "life_age_checks/profile");
    const bool enabled =
        get_bool(config, "controlled_natural_death_enabled", "life_age_checks");
    LifeAgeCatalog out;
    out.policy_ref = {"original_natural_age_70", 1, identity.content_digest};
    out.calendar_profile_ref = calendar.profile_ref();
    for (bool value : {false, true}) {
      LifeAgeSettingsDefinition d;
      d.ref = {value ? "original_controlled_natural_death_enabled"
                     : "original_controlled_natural_death_disabled",
               1, identity.content_digest};
      d.controlled_natural_death_enabled = value;
      if (value == enabled)
        out.initial_settings_ref = d.ref;
      const auto key = d.ref.id;
      if (!out.settings.insert(key, std::move(d)))
        return {
            false,
            {},
            common_failure("DuplicateLifeAgeSettingsDefinition", key).error};
    }
    auto valid = validate_life_age_catalog(out, identity, calendar);
    return {valid.ok, std::move(out), valid.error};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status initialize_life_age_state_from_original_seed(
    const LifeAgeSources &src, const LifeAgeCatalog &catalog,
    LifeAgeState &state, const FamilyDeathState &deaths,
    const WorldMoment &at) {
  return checked([&] {
    same(at, WorldMoment{});
    if (src.common.tick() != 0 || src.common.state().identity.revision != 0 ||
        !state.checks.empty() || !state.last_age_check_year.empty() ||
        !state.settings_changes.empty())
      fail("LifeAgeOriginalFreshSeedRequired");
    for (const auto &row : src.common.state().actors.rows())
      take(read_life_age_basis(src, row.first, at));
    take(validate_life_age_state(src, catalog, state, deaths, at));
  });
}
Status apply_original_life_age_calendar_in_candidate(
    WorldBundle &w, const ExactPeriodContext &p,
    ExactPhaseWriteContext &context, LifeAgeExactWorldFactory factory) {
  return checked([&] {
    if (p.period_id.empty() || p.opening_at.half_day_tick < 0 ||
        p.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
        p.through_tick != p.opening_at.half_day_tick + 1 ||
        take(compare_fractions(p.opening_at.sub_half_day, CanonicalFraction{})))
      fail("LifeAgeActualCalendarPeriodRequired", p.period_id);
    const WorldMoment at{p.through_tick, {}};
    same(at, take(w.actual_phase_moment()));
    if (w.active_exact_write_context() != &context ||
        !w.common.actor_condition_capture_identity())
      fail("LifeAgeOriginalLiveCaptureRequired");
    {
      auto lease = read(factory, w, at);
      const auto b = lease->bindings();
      if (!take(b.sources.calendar.year_boundary(p.opening_at.half_day_tick,
                                                 p.through_tick)))
        return;
    }
    // Stable original actor order is also the original RNG application order.
    // No actor is created by this stage; birth_due follows it in the same
    // graph.
    for (const auto &actor_id : w.common.state().actors.order()) {
      AgeCheckApplication result;
      {
        auto lease = write(factory, w, at);
        auto b = lease->bindings();
        const auto r = b.original.bindings();
        auto stamp = take(b.current.capture(
            {ResourceKey{ResourceKey::Kind::actor, actor_id}},
            {{"actor_foundation", "condition_head", actor_id}}, {}));
        if (!stamp)
          fail("LifeAgeActualReadStampRequired", actor_id);
        Written boundary{w};
        result = take(
            apply_life_age_check(b.common, b.state, r.catalog, r.sources,
                                 actor_id, p.opening_at.half_day_tick,
                                 p.through_tick, *stamp, b.stamps, b.versions));
      }
      if (!result.death_cause)
        continue;
      DeathRef actual_death;
      {
        auto lease = write(factory, w, at);
        auto b = lease->bindings();
        const auto r = b.original.bindings();
        auto stamp = take(b.current.capture(
            {ResourceKey{ResourceKey::Kind::actor, actor_id}},
            {{"actor_foundation", "condition_head", actor_id}},
            {result.death_cause->age_check_fact_id}));
        if (!stamp)
          fail("LifeAgeActualDeathReadStampRequired", actor_id);
        const bool care_selected =
            care_life_condition_consumer_selected(r.sources.foundation_catalog);
        if (care_selected &&
            (!b.care_life ||
             !b.care_life->bound_to(b.common, b.foundation, b.deaths)))
          fail("LifeAgeActualCareLifeWriterRequired", actor_id,
               ErrorKind::unsupported_capability);
        std::optional<CharacterConditionSources> body;
        if (b.character_conditions)
          body.emplace(take(b.character_conditions->sources()));
        take(validate_life_age_death_condition_sources(
            r, body ? &*body : nullptr, at));
        Written boundary{w};
        auto applied = take(apply_verified_life_termination(
            b.common, b.foundation, b.deaths, r.sources.foundation_catalog,
            *body->life_sources, *result.death_cause, *stamp, b.stamps,
            b.versions, &*body));
        bool capability_chain = false;
        for (const auto &module :
             r.sources.foundation_catalog.identity.module_versions)
          if (module.first == "growth_permanent_contributions" &&
              module.second == "3")
            capability_chain = true;
        if (capability_chain && !b.capability_changes)
          fail("LifeAgeActualCapabilityWriterRequired", actor_id,
               ErrorKind::unsupported_capability);
        for (auto &item : applied.actual_writes) {
          if (capability_chain)
            take(b.capability_changes->record_actual_condition_change(item));
          if (care_selected)
            take(b.care_life->consume_actual_life_condition(item));
          context.condition_writes.push_back(std::move(item));
        }
        actual_death = std::move(applied.death_ref);
      }
      // 先结束原写边界，再借真实 DeathRef；不能用当前 dead 布尔造待接续。
      if (w.control_continuation)
        take(factory.after_actual_death(w, actual_death));
      take(factory.after_actual_life_write(w, context));
    }
    auto lease = read(factory, w, at);
    auto b = lease->bindings();
    take(validate_life_age_state(b.sources, b.catalog, b.state, b.deaths, at));
  });
}
Status
validate_original_life_age_author_selection(const WorldBundle &w,
                                            LifeAgeExactWorldFactory factory) {
  return checked([&] {
    auto at = take(w.actual_phase_moment());
    auto lease = read(factory, w, at);
  });
}
Status
validate_original_life_age_world_sources(const WorldBundle &w,
                                         const WorldMoment &at,
                                         LifeAgeExactWorldFactory factory) {
  return checked([&] {
    auto lease = read(factory, w, at);
    const auto b = lease->bindings();
    take(validate_life_age_state(b.sources, b.catalog, b.state, b.deaths, at));
  });
}
ExactPhaseRegistration
make_life_age_exact_phase_registration(LifeAgeExactWorldFactory factory) {
  ExactPhaseRegistration r;
  r.provider = ExactProviderId::family;
  r.profile_id = "original_life_age_calendar";
  r.version = 1;
  r.schedule_kind = ExactScheduleKind::boundary_only;
  r.stages = {ExactStage::life_age};
  r.calendar_stages = r.stages;
  r.effect_writer_keys = {"family/original_life_age_check"};
  r.validate_author_selection = [factory](const WorldBundle &w) {
    return validate_original_life_age_author_selection(w, factory);
  };
  r.validate_world_sources = [factory](const WorldBundle &w,
                                       const WorldMoment &at) {
    return validate_original_life_age_world_sources(w, at, factory);
  };
  r.apply_calendar_stage = [factory](WorldBundle &w, ExactStage stage,
                                     const ExactPeriodContext &p,
                                     ExactPhaseWriteContext &context) {
    if (stage != ExactStage::life_age)
      return common_failure("LifeAgeCalendarStageMismatch", {},
                            ErrorKind::internal);
    return apply_original_life_age_calendar_in_candidate(w, p, context,
                                                         factory);
  };
  return r;
}
} // namespace hanmo::resident
