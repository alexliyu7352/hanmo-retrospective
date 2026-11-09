#include "native_military_world_scope.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "military_battle_lifecycle.hpp"
#include "military_exact_validation.hpp"
#include "military_roster_history.hpp"
#include "naval_contact_history.hpp"
#include "resident/domains/travel/military_action_placement.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/native_character_family_world.hpp"
#include "resident/kernel/native_military_battle_lifecycle.hpp"
#include "resident/kernel/native_military_state.hpp"
#include "resident/kernel/native_military_training_period.hpp"
#include "resident/kernel/native_personal_battle_lifecycle.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/world_read_stamp.hpp"
#include <charconv>
#include <new>
#include <stdexcept>
#include <utility>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> x) {
  if (!x.ok)
    throw Failure{std::move(x.error)};
  return std::move(x.value);
}
void take(Status x) {
  if (!x.ok)
    throw Failure{std::move(x.error)};
}
[[noreturn]] void fail(const char *code) {
  throw Failure{
      common_failure(code, {}, ErrorKind::unsupported_capability).error};
}
template <class T, class F> Outcome<T> guarded(F &&f) {
  try {
    return {true, f(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("NativeMilitaryWorldScopeResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("NativeMilitaryWorldScopeResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
bool same(const WorldMoment &a, const WorldMoment &b,
          ExactArithmeticBudget budget) {
  return take(compare_world_moments(a, b, budget)) == 0;
}
Integer module(const CatalogIdentity &id, const char *name) {
  Integer out = 0;
  bool seen = false;
  for (const auto &row : id.module_versions)
    if (row.first == name) {
      if (seen || row.second.empty())
        fail("NativeMilitaryWorldModuleMismatch");
      seen = true;
      const auto parsed = std::from_chars(
          row.second.data(), row.second.data() + row.second.size(), out);
      if (parsed.ec != std::errc{} ||
          parsed.ptr != row.second.data() + row.second.size() || out <= 0 ||
          std::to_string(out) != row.second)
        fail("NativeMilitaryWorldModuleMismatch");
    }
  return out;
}
template <class T> void selected(bool on, const T *p, const char *code) {
  if (on != bool(p))
    fail(code);
}
struct Selection {
  MilitaryExactPhaseSelection phase;
  NativeMilitaryStateVersions versions;
  Integer personal = 0, personal_lifecycle = 0, naval = 0, land_lifecycle = 0;
};
Selection preflight(const WorldBundle &w,
                    const NativeMilitaryWorldScopeDependencies &d,
                    const WorldMoment &at) {
  if (!w.catalog || !w.catalog->common || !w.catalog->military ||
      !w.catalog->exact_time || !w.catalog->complete_exact_time_runtime ||
      !w.world_timeline || !w.action_elapsed || !w.actor_foundation ||
      !w.catalog->actor_foundation)
    fail("NativeMilitaryWorldCoreRootsRequired");
  if (!same(take(w.actual_phase_moment()), at, d.budget) ||
      at.half_day_tick < w.common.tick())
    fail("NativeMilitaryWorldActualMomentRequired");
  Selection s{
      take(read_military_exact_phase_selection(*w.catalog)),
      take(selected_native_military_state_versions(w.catalog->identity))};
  if (bool(w.military_exact) != bool(s.phase.dynamic_schema))
    fail("NativeMilitaryWorldDynamicSelectionMismatch");
  if (s.phase.field_original &&
      (!w.military.training_facts.empty() ||
       !w.military.training_actions.empty() ||
       !w.military.logistics_actions.empty() ||
       !w.military.logistics_facts.empty() ||
       !w.military.return_facts.empty() || !w.military.battles.empty() ||
       !w.military.recovery.facts.empty() ||
       !w.military.recovery.battle_releases.empty() ||
       !w.military.recovery.siege_releases.empty() ||
       !w.military.recovery.field_order.empty() ||
       !w.military.garrison_rest.facts.empty() ||
       !w.military.garrison_rest.field_order.empty() ||
       !w.military.supply_attrition.facts.empty() ||
       !w.military.supply_attrition.field_order.empty()))
    fail("UnselectedMilitaryFieldLegacyStatePresent");
  // Personal has the original fixed /1 physical section. Its authored catalog,
  // not a nonempty battle table, determines whether this source branch is used.
  s.personal = w.catalog->personal ? 1 : 0;
  s.personal_lifecycle =
      take(selected_native_personal_battle_lifecycle(w.catalog->identity));
  s.naval = module(w.catalog->identity, "military_naval");
  s.land_lifecycle =
      take(selected_native_military_battle_lifecycle(w.catalog->identity));
  if (bool(s.naval) != bool(w.catalog->naval) || s.naval < 0 || s.naval > 6)
    fail("NativeMilitaryWorldNavalSelectionMismatch");
  if (d.access.common_catalog != w.catalog->common.get() ||
      d.access.interaction != &w.interaction ||
      d.access.interaction_catalog != w.catalog->interaction.get() ||
      d.access.military_authority_time_schema !=
          s.versions.military_authority_time ||
      d.access.military_action_resources_schema !=
          s.versions.military_action_resources ||
      d.access.military_action_conditions_schema !=
          s.versions.military_action_conditions ||
      d.access.military_training_completion_time_schema !=
          s.versions.military_training_completion_time ||
      d.access.military_formation_reservations_schema !=
          s.versions.military_formation_reservations ||
      d.access.military_return_detachments_schema !=
          s.versions.military_return_detachments ||
      d.access.military_battle_lifecycle_schema != s.land_lifecycle)
    fail("NativeMilitaryWorldAccessSelectionMismatch");
  if (!d.formations.bound_to(w.common.state(), w.military) ||
      !d.returns.bound_to(w.common, w.military) || !w.production_cycles ||
      !d.periods.bound_to(w.common, *w.production_cycles) ||
      !d.social.bound_to(w.common, w.interaction))
    fail("NativeMilitaryWorldCoreSourcesMismatch");
  if (s.phase.actions.office_accept || s.phase.actions.army_form) {
    if (!w.military_action_time || !w.movement_exact || !w.spatial_carriers ||
        !w.transport_registrations || !w.catalog->interaction ||
        !w.catalog->family || !w.catalog->estate ||
        !d.placements.bound_to(w.common, *w.movement_exact))
      fail("NativeMilitaryWorldOriginalRootsRequired");
    if (s.versions.military_action_time != 1 ||
        s.versions.military_action_conditions != 2 ||
        s.versions.military_authority_time != 1 || s.land_lifecycle != 1 ||
        (s.personal ? s.personal_lifecycle != 1 : s.personal_lifecycle != 0))
      fail("NativeMilitaryWorldOriginalSelectionRequired");
    if (s.naval && (!d.naval_history || d.access.naval_state != &w.naval ||
                    d.access.travel_state != &w.travel))
      fail("NativeMilitaryWorldNavalHistoryRequired");
  }
  if (!d.remaining || !d.remaining->bound_to(w, d.owner))
    fail("NativeMilitaryWorldRemainingOwnerRequired");
  return s;
}
void body_roots(const WorldBundle &w,
                const NativeMilitaryWorldScopeDependencies &d,
                const CharacterConditionSources &body, const WorldMoment &at) {
  if (&body.common != &w.common || &body.state != &*w.actor_foundation ||
      &body.catalog != w.catalog->actor_foundation.get() ||
      body.shared != w.catalog->common.get() || body.growth != &w.growth ||
      body.growth_catalog != w.catalog->growth.get() ||
      body.elapsed != &*w.action_elapsed ||
      body.scale != &w.world_timeline->scale_ref ||
      body.action_registrations !=
          &w.catalog->complete_exact_time_runtime->action_times ||
      body.movement != (w.movement_exact ? &*w.movement_exact : nullptr) ||
      body.deaths != (w.family_life_enabled() ? &w.family_life : nullptr) ||
      body.personal_catalog != w.catalog->personal.get() ||
      body.family_catalog != w.catalog->family.get() ||
      body.director != &w.director ||
      body.director_catalog != w.catalog->director.get() ||
      body.rest_estate_state != (w.catalog->estate ? &w.estate : nullptr) ||
      body.permanent_growth !=
          (w.growth_permanent ? &*w.growth_permanent : nullptr) ||
      body.permanent_growth_catalog != w.catalog->growth_permanent.get() ||
      body.military != &w.military ||
      body.military_exact !=
          (w.military_exact ? &*w.military_exact : nullptr) ||
      body.military_formations != &d.formations ||
      body.military_returns != &d.returns ||
      body.military_supply_periods != &d.periods ||
      body.interaction != &w.interaction ||
      body.interaction_catalog != w.catalog->interaction.get() ||
      !same(body.actual_now, at, d.budget))
    fail("NativeMilitaryWorldBodyRootsMismatch");
  if (const auto *life = body.life_sources)
    if (&life->military != &w.military ||
        life->military_exact !=
            (w.military_exact ? &*w.military_exact : nullptr) ||
        life->military_formations != &d.formations ||
        life->military_returns != &d.returns)
      fail("NativeMilitaryWorldLifeRootsMismatch");
  if (d.family.military_formations() != &d.formations ||
      d.family.military_returns() != &d.returns ||
      d.family.military_supply_periods() != &d.periods)
    fail("NativeMilitaryWorldFamilySourcesMismatch");
}

class NativeMilitaryWorldScope final : public NativeCompleteWorldSourceScope {
  const WorldBundle &world_;
  NativeMilitaryWorldScopeDependencies dependencies_;
  WorldMoment at_;
  WorldCurrentMomentRead clock_;
  Selection selected_;
  // 只在已发布 Complete owner 的实际 Military acquisition 内保活当面服务借口。
  std::optional<InteractionAccess> field_interaction_;
  MilitaryAccess access_;
  CharacterConditionSources body_;
  // 与本 scope 的真实 Body/Field application 同源，只保存借用，不造旧 dynamic
  // 根。
  std::optional<MilitaryFieldHistoryRead> field_history_;
  std::optional<MilitaryActionConditionContext> conditions_;
  std::optional<ExactOfficeSourceReadContext> office_source_;
  std::optional<MilitaryAuthoritySourceContext> authority_source_;
  std::optional<NavalOriginalSourceContext> naval_original_;
  std::optional<NavalHistoryAccess> naval_history_;
  std::optional<ExactNavalAssetHistoryContext> naval_assets_;
  std::optional<NativeOfficeNavalSources> office_naval_;
  std::optional<NativeFormationNavalSources> formation_naval_;
  std::optional<NativeOfficeOriginalRead> office_original_;
  std::optional<ExactOfficeReadContext> office_;
  std::optional<MilitaryAuthorityTimeContext> authority_time_;
  std::optional<NativeFormationOriginalRead> formation_original_;
  std::optional<ExactFormationReadContext> formation_;
  std::optional<MilitaryBattleLifecycleWriteContext> battle_lifecycle_;
  std::optional<NativeMilitaryWorldConstructionContext> construction_;
  // Returned professional contexts can borrow any earlier member. They are
  // destroyed before those roots, and before the long-lived remaining owner.
  std::shared_ptr<const NativeMilitaryWorldRemainingScope> remaining_read_;
  std::shared_ptr<NativeMilitaryWorldRemainingScope> remaining_write_;
  std::optional<MilitaryExactReadBinding> read_;
  std::optional<ExactOfficeWriteContext> office_write_;
  std::optional<ExactFormationWriteContext> formation_write_;
  std::optional<MilitaryExactWriteBinding> write_;
  NativeCompleteWorldSourceExports exports_;

  void originals() {
    const auto &w = world_;
    const auto &d = dependencies_;
    if (w.military_action_time) {
      conditions_.emplace(
          MilitaryActionConditionContext{w.common, *w.military_action_time,
                                         *w.actor_foundation, body_, clock_});
      access_.action_conditions = selected_.versions.military_action_conditions
                                      ? &*conditions_
                                      : nullptr;
    } else if (selected_.versions.military_action_conditions)
      fail("NativeMilitaryWorldActionTimeRequired");
    if (selected_.land_lifecycle) {
      battle_lifecycle_.emplace(MilitaryBattleLifecycleWriteContext{
          w.common, w.military, clock_, d.budget});
      access_.battle_lifecycle = &*battle_lifecycle_;
    } else
      access_.battle_lifecycle = nullptr;
    // Never retain a different acquisition's authority context. A selected
    // remaining-only profile supplies its own typed context before publication.
    access_.authority_time = nullptr;
    if (!selected_.phase.actions.office_accept &&
        !selected_.phase.actions.army_form)
      return;
    // Body owns the World timeline root. The outer phase contexts below retain
    // their separately required catalog exact-time scale root.
    // Physical military_action_time/1 owns the original schema-2 action rows.
    office_source_.emplace(ExactOfficeSourceReadContext{
        w.common, w.military, *w.military_action_time, *w.action_elapsed,
        *w.catalog->military, access_, *body_.scale, clock_, *w.catalog,
        w.organization, w.family, body_, d.budget, 2});
    authority_source_.emplace(MilitaryAuthoritySourceContext{
        w.common, w.military, *w.military_action_time, *w.action_elapsed,
        *office_source_, selected_.versions.military_authority_time, nullptr});
    if (selected_.naval) {
      naval_original_.emplace(NavalOriginalSourceContext{
          w.common, w.military, *w.military_exact, *w.military_action_time,
          *w.action_elapsed, *authority_source_, d.formations, d.returns,
          &d.periods, nullptr});
      naval_history_.emplace(*d.naval_history);
      naval_history_->original_sources = &*naval_original_;
      naval_assets_.emplace(ExactNavalAssetHistoryContext{
          *w.military_action_time, *w.action_elapsed,
          ExactNavalRepairSourceContext{*w.catalog->military, access_,
                                        *w.catalog->naval, *body_.scale, clock_,
                                        d.budget},
          &*naval_original_});
      office_naval_.emplace(NativeOfficeNavalSources{w.naval, *w.catalog->naval,
                                                     w.travel, *naval_history_,
                                                     *naval_assets_});
      formation_naval_.emplace(
          NativeFormationNavalSources{w.naval, *w.catalog->naval, w.travel,
                                      *naval_history_, *naval_assets_});
    }
    ActorSpatialHeadSourceReadContext spatial{w.common,
                                              *w.catalog->common,
                                              *w.movement_exact,
                                              *w.spatial_carriers,
                                              *w.transport_registrations,
                                              *w.actor_foundation,
                                              *w.catalog->actor_foundation,
                                              d.spatial,
                                              w.catalog->family.get(),
                                              d.births,
                                              d.budget,
                                              &d.placements};
    office_original_.emplace(NativeOfficeOriginalContext{
        *authority_source_, *conditions_,
        w.military_exact ? &*w.military_exact : nullptr, w.catalog->identity,
        spatial, d.formations, d.returns,
        (w.military_exact ? &w.military_exact->spatial_handoffs : nullptr),
        selected_.personal ? &w.personal : nullptr,
        office_naval_ ? &*office_naval_ : nullptr, selected_.personal,
        selected_.personal_lifecycle, selected_.naval, selected_.land_lifecycle,
        d.budget, field_history_ ? &*field_history_ : nullptr});
    office_.emplace(ExactOfficeReadContext{
        *w.catalog->military, access_, w.catalog->exact_time->scale_ref,
        *office_original_, clock_, *w.catalog, w.organization, w.family, body_,
        d.budget});
    authority_time_.emplace(MilitaryAuthorityTimeContext{
        w.common, w.military, *w.military_action_time, *w.action_elapsed,
        *office_});
    access_.authority_time = &*authority_time_;
    if (selected_.phase.actions.army_form) {
      NativeFormationSpatialSources space{
          *w.catalog->common,
          *w.movement_exact,
          *w.spatial_carriers,
          *w.transport_registrations,
          *w.actor_foundation,
          *w.catalog->actor_foundation,
          d.spatial,
          d.placements,
          (w.military_exact ? &w.military_exact->spatial_handoffs : nullptr),
          w.catalog->family.get(),
          d.births};
      formation_original_.emplace(NativeFormationOriginalContext{
          w.common,
          w.military,
          *w.military_action_time,
          w.military_exact ? &*w.military_exact : nullptr,
          w.catalog->identity,
          *w.action_elapsed,
          *w.catalog->military,
          access_,
          *body_.scale,
          clock_,
          d.periods,
          d.formations,
          d.returns,
          *conditions_,
          *authority_source_,
          space,
          w.interaction,
          d.social,
          w.estate,
          *w.catalog->estate,
          &w.population_military,
          w.catalog->population_military.get(),
          selected_.personal ? &w.personal : nullptr,
          formation_naval_ ? &*formation_naval_ : nullptr,
          selected_.personal,
          selected_.personal_lifecycle,
          selected_.naval,
          selected_.land_lifecycle,
          selected_.versions.military_return_detachments,
          d.budget,
          field_history_ ? &*field_history_ : nullptr});
      formation_.emplace(ExactFormationReadContext{
          *w.catalog->military, access_, w.catalog->exact_time->scale_ref,
          *formation_original_, d.periods, clock_, d.budget});
      take(formation_original_->validate_bindings());
    }
  }
  void link_remaining(const NativeMilitaryWorldRemainingReadBindings &r) {
    if (authority_time_) {
      if (r.authority_time && r.authority_time != &*authority_time_)
        fail("NativeMilitaryDuplicateAuthorityOwner");
      return;
    }
    selected(selected_.versions.military_authority_time != 0, r.authority_time,
             "NativeMilitaryAuthorityConstructionRequired");
    if (r.authority_time) {
      const auto &x = *r.authority_time;
      const auto &w = world_;
      if (!w.military_action_time || &x.common != &w.common ||
          &x.military != &w.military ||
          &x.actions != &*w.military_action_time ||
          &x.elapsed != &*w.action_elapsed ||
          &x.office.catalog != w.catalog->military.get() ||
          &x.office.access != &access_ || &x.office.clock != &clock_ ||
          &x.office.scale != &w.catalog->exact_time->scale_ref ||
          &x.office.prepared != w.catalog.get() ||
          &x.office.organization != &w.organization ||
          &x.office.family != &w.family || &x.office.body != &body_ ||
          !x.office.original.bound_to(w.common, w.military,
                                      *w.military_action_time))
        fail("NativeMilitaryRemainingAuthorityRootsMismatch");
      access_.authority_time = r.authority_time;
    }
  }
  void validate_remaining(const NativeMilitaryWorldRemainingReadBindings &r) {
    const auto &w = world_;
    const auto &s = selected_.phase;
    selected(s.actions.military_train, r.training,
             "NativeMilitaryTrainingConstructionRequired");
    selected(s.actions.army_return, r.returning,
             "NativeMilitaryReturnConstructionRequired");
    selected(s.actions.army_resupply, r.resupply,
             "NativeMilitaryResupplyConstructionRequired");
    selected(s.naval_repair, r.naval_repair,
             "NativeMilitaryNavalRepairConstructionRequired");
    auto action = [&](const auto *c) {
      if (c &&
          (&c->catalog != w.catalog->military.get() || &c->access != &access_ ||
           &c->scale != &w.catalog->exact_time->scale_ref ||
           &c->clock != &clock_))
        fail("NativeMilitaryRemainingActionRootsMismatch");
    };
    action(r.training);
    action(r.returning);
    action(r.resupply);
    if (r.training && (&r.training->periods != &dependencies_.periods ||
                       !r.training->original.bound_to(w.common, w.military)))
      fail("NativeMilitaryTrainingOriginalRootsMismatch");
    if (r.returning && (r.returning->exact !=
                            (w.military_exact ? &*w.military_exact : nullptr) ||
                        (r.returning->identity &&
                         r.returning->identity != &w.catalog->identity) ||
                        (!r.returning->identity && !r.returning->exact) ||
                        r.returning->field_history !=
                            (field_history_ ? &*field_history_ : nullptr) ||
                        &r.returning->formations != &dependencies_.formations ||
                        &r.returning->conditions != &body_ ||
                        &r.returning->population != &w.population_military ||
                        r.returning->population_catalog !=
                            w.catalog->population_military.get() ||
                        &r.returning->periods != &dependencies_.periods ||
                        !w.military_action_time ||
                        !r.returning->original.bound_to(
                            w.common, w.military, *w.military_action_time)))
      fail("NativeMilitaryReturnOriginalRootsMismatch");
    if (r.resupply && (!w.military_action_time ||
                       !r.resupply->original.bound_to(w.common, w.military,
                                                      *w.military_action_time)))
      fail("NativeMilitaryResupplyOriginalRootsMismatch");
    if (r.naval_repair &&
        (!w.catalog->naval ||
         &r.naval_repair->military_catalog != w.catalog->military.get() ||
         &r.naval_repair->naval_catalog != w.catalog->naval.get() ||
         &r.naval_repair->access != &access_ ||
         &r.naval_repair->clock != &clock_ ||
         &r.naval_repair->scale != &w.catalog->exact_time->scale_ref))
      fail("NativeMilitaryNavalRepairOriginalRootsMismatch");
    if (r.naval_repair &&
        (!w.military_action_time ||
         !r.naval_repair->original.bound_to(w.common, w.military, w.naval,
                                            *w.military_action_time)))
      fail("NativeMilitaryNavalRepairOriginalBindingMismatch");
    selected(s.dynamic_schema != 0, r.dynamic_sources,
             "NativeMilitaryDynamicConstructionSelectionMismatch");
    selected(s.dynamic_schema != 0, r.duty,
             "NativeMilitaryDutyConstructionSelectionMismatch");
    if (s.dynamic_schema) {
      if (!r.dynamic_sources || !r.duty)
        fail("NativeMilitaryDynamicAndDutyConstructionRequired");
      if (!w.catalog->dynamic_military_actions ||
          !w.catalog->military_duty_catalog ||
          &r.dynamic_sources->actions.clock != &clock_ ||
          &r.dynamic_sources->land.clock != &clock_ ||
          &r.dynamic_sources->actions.catalog !=
              w.catalog->dynamic_military_actions.get() ||
          &r.dynamic_sources->battle_catalog !=
              &w.catalog->military->battle_catalog ||
          &r.duty->catalog != w.catalog->military_duty_catalog.get() ||
          &r.duty->scale != &w.catalog->exact_time->scale_ref ||
          r.duty->selected_dynamic_military != s.dynamic_schema ||
          !same(take(r.duty->stamps.read_current_moment()), at_,
                dependencies_.budget))
        fail("NativeMilitaryRemainingOriginalRootsMismatch");
      if ((s.dynamic_schema == 3 || s.dynamic_schema == 4) &&
          !r.dynamic_sources->tactical_ownership)
        fail("NativeMilitaryTacticalOwnershipConstructionRequired");
      if (s.dynamic_schema == 4 &&
          (!r.duty->assignments ||
           !r.duty->assignments->bound_to(
               w.common, w.military_exact->duty_sources, r.duty->catalog) ||
           r.duty->condition_sources != &body_))
        fail("NativeMilitaryDutyAssignmentConstructionRequired");
    }
  }
  void finish_read(const NativeMilitaryWorldRemainingReadBindings &r) {
    validate_remaining(r);
    selected(selected_.phase.field_original, r.field_supply,
             "NativeMilitaryFieldSupplySelectionMismatch");
    const auto &w = world_;
    if (r.field_supply &&
        (!r.field_supply->field_supply_source1 ||
         r.field_supply->dynamic_military ||
         &r.field_supply->action_time != &*w.military_action_time ||
         &r.field_supply->elapsed != &*w.action_elapsed ||
         &r.field_supply->clock != &clock_ ||
         &r.field_supply->formations != &dependencies_.formations ||
         &r.field_supply->periods != &dependencies_.periods))
      fail("NativeMilitaryFieldSupplyRootsMismatch");
    if (!same(take(clock_.read_current_moment()), at_, dependencies_.budget) ||
        !same(body_.actual_now, at_, dependencies_.budget))
      fail("NativeMilitaryWorldScopeMomentChangedDuringConstruction");
    read_.emplace(MilitaryExactReadBinding{
        w.common, w.military, *w.catalog->military, access_, *w.action_elapsed,
        clock_, w.military_action_time ? &*w.military_action_time : nullptr,
        w.military_exact ? &*w.military_exact : nullptr,
        selected_.phase.naval_repair ? &w.naval : nullptr,
        selected_.phase.actions.office_accept ? &*office_ : nullptr,
        formation_ ? &*formation_ : nullptr, r.training, r.returning,
        r.resupply, r.naval_repair, r.dynamic_sources, r.duty,
        dependencies_.budget, r.field_supply});
    exports_.military = &*read_;
  }

public:
  NativeMilitaryWorldScope(const WorldBundle &w,
                           NativeMilitaryWorldScopeDependencies d,
                           const WorldMoment &at, Selection s,
                           CharacterConditionSources body)
      : world_(w), dependencies_(std::move(d)), at_(at), clock_(w),
        selected_(std::move(s)), access_(dependencies_.access),
        body_(std::move(body)) {
    if (selected_.phase.field_original) {
      // Field Office/Order 原 preview/start
      // 仍验真实当面服务；不能把缺指针当无条件。 scope 在
      // read_military/write_military 的已发布 owner 之后创建，不重入构造。
      field_interaction_.emplace(
          w.services.interaction_access(&dependencies_.owner.reads()));
      access_.interaction_access = &*field_interaction_;
    }
    if (w.field_battle) {
      if (body_.injury_field != &*w.field_battle || !body_.injury_field_clock)
        fail("NativeMilitaryFieldHistoryRootsRequired");
      field_history_.emplace(
          MilitaryFieldHistoryRead{*w.field_battle, *body_.injury_field_clock});
    }
    take(validate_selected_military_field_history(
        w.common, w.military, w.catalog->identity,
        field_history_ ? &*field_history_ : nullptr));
    originals();
    construction_.emplace(NativeMilitaryWorldConstructionContext{
        w, dependencies_.owner, at_, clock_, access_, body_,
        dependencies_.family, selected_.phase,
        selected_.phase.actions.office_accept ? &*office_ : nullptr,
        formation_ ? &*formation_ : nullptr,
        authority_source_ ? &*authority_source_ : nullptr,
        dependencies_.formations, dependencies_.returns, dependencies_.periods,
        dependencies_.budget, field_history_ ? &*field_history_ : nullptr});
  }
  void acquire_read() {
    remaining_read_ =
        take(dependencies_.remaining->read_sources(*construction_));
    if (!remaining_read_)
      fail("NativeMilitaryRemainingReadScopeMismatch");
    link_remaining(remaining_read_->reads());
    if (!remaining_read_->bound_to(*construction_, nullptr))
      fail("NativeMilitaryRemainingReadScopeMismatch");
    finish_read(remaining_read_->reads());
  }
  void acquire_write(WorldBundle &w, ExactPhaseWriteContext &writes) {
    if (&w != &world_ || w.active_exact_write_context() != &writes)
      fail("NativeMilitaryActualWriteContextRequired");
    remaining_write_ =
        take(dependencies_.remaining->write_sources(w, *construction_, writes));
    if (!remaining_write_)
      fail("NativeMilitaryRemainingWriteScopeMismatch");
    link_remaining(remaining_write_->reads());
    if (!remaining_write_->bound_to(*construction_, &writes))
      fail("NativeMilitaryRemainingWriteScopeMismatch");
    finish_read(remaining_write_->reads());
    auto *r = remaining_write_->writes();
    if (!r || !r->calendar)
      fail("NativeMilitaryRemainingWritersRequired");
    const auto &s = selected_.phase;
    selected(s.actions.military_train, r->training,
             "NativeMilitaryTrainingWriterRequired");
    selected(s.actions.army_return, r->returning,
             "NativeMilitaryReturnWriterRequired");
    selected(s.actions.army_resupply, r->resupply,
             "NativeMilitaryResupplyWriterRequired");
    selected(s.naval_repair, r->naval_repair,
             "NativeMilitaryNavalRepairWriterRequired");
    auto action = [&](const auto *writer, const auto *reader) {
      if (writer &&
          (&writer->read.original != &reader->original ||
           &writer->read.access != &access_ || &writer->read.clock != &clock_ ||
           &writer->read.catalog != w.catalog->military.get() ||
           &writer->read.scale != &w.catalog->exact_time->scale_ref))
        fail("NativeMilitaryRemainingWriteRootsMismatch");
    };
    action(r->training, read_->training);
    action(r->returning, read_->returning);
    action(r->resupply, read_->resupply);
    if (r->training && &r->training->read.periods != &read_->training->periods)
      fail("NativeMilitaryTrainingWritePeriodsMismatch");
    if (r->returning &&
        (r->returning->read.exact != read_->returning->exact ||
         r->returning->read.identity != read_->returning->identity ||
         r->returning->read.field_history != read_->returning->field_history ||
         &r->returning->read.formations != &read_->returning->formations ||
         &r->returning->read.conditions != &read_->returning->conditions ||
         &r->returning->read.population != &read_->returning->population ||
         r->returning->read.population_catalog !=
             read_->returning->population_catalog ||
         &r->returning->read.periods != &read_->returning->periods))
      fail("NativeMilitaryReturnWriteRootsMismatch");
    if (r->naval_repair &&
        (&r->naval_repair->read.original != &read_->naval_repair->original ||
         &r->naval_repair->read.military_catalog != w.catalog->military.get() ||
         &r->naval_repair->read.naval_catalog != w.catalog->naval.get() ||
         &r->naval_repair->read.access != &access_ ||
         &r->naval_repair->read.scale != &w.catalog->exact_time->scale_ref ||
         &r->naval_repair->read.clock != &clock_))
      fail("NativeMilitaryNavalRepairWriteRootsMismatch");
    if (&r->calendar->common != &w.common ||
        &r->calendar->military != &w.military ||
        &r->calendar->catalog != w.catalog->military.get() ||
        &r->calendar->access != &access_ ||
        r->calendar->field_without_siege != s.field_original)
      fail("NativeMilitaryRemainingCalendarMismatch");
    selected(s.dynamic_schema != 0, r->dynamic_sources,
             "NativeMilitaryDynamicWriteSelectionMismatch");
    if (s.dynamic_schema &&
        (&r->dynamic_sources->actions != &read_->dynamic_sources->actions ||
         &r->dynamic_sources->originals != &read_->dynamic_sources->effects ||
         &r->dynamic_sources->land != &read_->dynamic_sources->land ||
         &r->dynamic_sources->battle_catalog !=
             &read_->dynamic_sources->battle_catalog ||
         &r->dynamic_sources->objectives != read_->dynamic_sources->objectives))
      fail("NativeMilitaryRemainingCalendarOrDynamicMismatch");
    if ((s.actions.office_accept || s.actions.army_form) && !r->release_action)
      fail("NativeMilitaryActualActionReleaseRequired");
    if (s.actions.office_accept)
      office_write_.emplace(
          ExactOfficeWriteContext{*office_, r->release_action});
    if (s.actions.army_form) {
      if (!r->adopt_formation_locations)
        fail("NativeMilitaryActualFormationPlacementRequired");
      formation_write_.emplace(ExactFormationWriteContext{
          *formation_, r->release_action, r->adopt_formation_locations});
    }
    write_.emplace(MilitaryExactWriteBinding{
        *read_, w.common, w.military, *w.action_elapsed, *r->calendar,
        w.military_action_time ? &*w.military_action_time : nullptr,
        w.military_exact ? &*w.military_exact : nullptr,
        s.naval_repair ? &w.naval : nullptr,
        office_write_ ? &*office_write_ : nullptr,
        formation_write_ ? &*formation_write_ : nullptr, r->training,
        r->returning, r->resupply, r->naval_repair, r->dynamic_sources});
    if (w.active_exact_write_context() != &writes ||
        !same(take(clock_.read_current_moment()), at_, dependencies_.budget))
      fail("NativeMilitaryActualWriteContextChangedDuringConstruction");
    exports_.military_write = &*write_;
  }
  bool bound_to(const WorldBundle &w) const noexcept override {
    return &w == &world_;
  }
  NativeCompleteWorldRoute route() const noexcept override {
    return NativeCompleteWorldRoute::military;
  }
  const WorldMoment &actual_at() const noexcept override { return at_; }
  const NativeCompleteWorldSourceExports &sources() const noexcept override {
    return exports_;
  }
};
} // namespace
Outcome<std::shared_ptr<const NativeCompleteWorldSourceScope>>
make_native_military_world_read_scope(const WorldBundle &w,
                                      NativeMilitaryWorldScopeDependencies d,
                                      const WorldMoment &at) {
  return guarded<std::shared_ptr<const NativeCompleteWorldSourceScope>>([&] {
    auto selected = preflight(w, d, at);
    auto body = take(d.family.body().sources());
    body_roots(w, d, body, at);
    auto scope =
        std::unique_ptr<NativeMilitaryWorldScope>(new NativeMilitaryWorldScope(
            w, std::move(d), at, std::move(selected), std::move(body)));
    scope->acquire_read();
    return std::shared_ptr<const NativeCompleteWorldSourceScope>(
        checkpoint_share(std::move(scope)));
  });
}
Outcome<std::shared_ptr<NativeCompleteWorldSourceScope>>
make_native_military_world_write_scope(WorldBundle &w,
                                       NativeMilitaryWorldScopeDependencies d,
                                       const WorldMoment &at,
                                       ExactPhaseWriteContext &writes) {
  return guarded<std::shared_ptr<NativeCompleteWorldSourceScope>>([&] {
    if (w.active_exact_write_context() != &writes)
      fail("NativeMilitaryActualWriteContextRequired");
    auto selected = preflight(w, d, at);
    auto body = take(d.family.body().sources());
    body_roots(w, d, body, at);
    auto scope =
        std::unique_ptr<NativeMilitaryWorldScope>(new NativeMilitaryWorldScope(
            w, std::move(d), at, std::move(selected), std::move(body)));
    scope->acquire_write(w, writes);
    return std::shared_ptr<NativeCompleteWorldSourceScope>(
        checkpoint_share(std::move(scope)));
  });
}
} // namespace hanmo::resident
