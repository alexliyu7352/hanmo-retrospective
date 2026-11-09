#include "military_exact_phase.hpp"
#include "formation_exact_phase.hpp"
#include "military_battle_lifecycle.hpp"
#include "military_exact_validation.hpp"
#include "military_supply_exact.hpp"
#include "office_exact_phase.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/native_military_battle_lifecycle.hpp"
#include "resident/kernel/native_military_state.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/native_field_group_world.hpp"
#include "resident/domains/travel/group_movement.hpp"
#include "resident/storage/resource_execution_coverage.hpp"
#include "resupply_exact_phase.hpp"
#include "return_exact_phase.hpp"
#include "training_exact_phase.hpp"
#include <iterator>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 回调只持有紧凑值副本；每次调用在栈上还原完整选型，不借注册函数的局部对象。
struct CompactMilitaryExactPhaseSelection {
  Integer dynamic_schema;
  bool office_accept;
  bool army_form;
  bool military_train;
  bool army_return;
  bool army_resupply;
  bool naval_repair;
  bool station;
  bool field_original;
};
static_assert(sizeof(CompactMilitaryExactPhaseSelection) == 16);
static_assert(std::is_trivially_copyable_v<CompactMilitaryExactPhaseSelection>);
static_assert(std::is_trivially_destructible_v<CompactMilitaryExactPhaseSelection>);
CompactMilitaryExactPhaseSelection
compact_selection(const MilitaryExactPhaseSelection &s) {
  return {s.dynamic_schema, s.actions.office_accept, s.actions.army_form,
          s.actions.military_train, s.actions.army_return,
          s.actions.army_resupply, s.naval_repair, s.station, s.field_original};
}
MilitaryExactPhaseSelection
restore_selection(const CompactMilitaryExactPhaseSelection &s) {
  MilitaryExactPhaseSelection selected;
  selected.dynamic_schema = s.dynamic_schema;
  selected.actions.office_accept = s.office_accept;
  selected.actions.army_form = s.army_form;
  selected.actions.military_train = s.military_train;
  selected.actions.army_return = s.army_return;
  selected.actions.army_resupply = s.army_resupply;
  selected.naval_repair = s.naval_repair;
  selected.station = s.station;
  selected.field_original = s.field_original;
  return selected;
}
bool selected_action(const CompleteExactTimeSelection &selected,
                     const ActionTimeRegistration &original) {
  bool found = false;
  for (const auto &item : selected.action_time_profiles)
    if (item.action_kind == original.action_kind) {
      if (found || item.profile_id != original.profile_id ||
          item.version != original.profile_version)
        fail("MilitaryExactActionSelectionMismatch", item.action_kind);
      found = true;
    }
  return found;
}
bool any_action(const MilitaryActionTimeReadSelection &s) {
  return s.office_accept || s.army_form || s.military_train || s.army_return ||
         s.army_resupply;
}
bool same_selection(const MilitaryExactPhaseSelection &a,
                    const MilitaryExactPhaseSelection &b) {
  return a.actions.office_accept == b.actions.office_accept &&
         a.actions.army_form == b.actions.army_form &&
         a.actions.military_train == b.actions.military_train &&
         a.actions.army_return == b.actions.army_return &&
         a.actions.army_resupply == b.actions.army_resupply &&
         a.naval_repair == b.naval_repair && a.station == b.station &&
         a.dynamic_schema == b.dynamic_schema &&
         a.field_original == b.field_original;
}
// 未选军旅只承接完整图的固定阶段；所有判断借同一原目录/World，不能以空表代替选型。
bool absent_military_kind(std::string_view kind) {
  return kind.starts_with("military_") || kind.starts_with("naval_") ||
         kind.starts_with("army_") || kind.starts_with("campaign_") ||
         kind.starts_with("siege_") || kind == "office_accept" ||
         kind == "office_resign" || kind == "office_resigned" ||
         kind == "population_enroll" || kind == "population_release" ||
         kind.starts_with("population_military") ||
         kind.starts_with("political_military_") ||
         kind.starts_with("political_field_");
}
// 只识别原军旅拥有域，不把其他领域的相似业务当成军旅来源。
bool absent_military_domain(std::string_view domain) {
  return domain == "military" || domain.starts_with("military_") ||
         domain == "naval" || domain.starts_with("naval_") ||
         domain == "population_military";
}
// 阴性分支拒绝所有原军旅来源，包含被篡改domain后仍保留原kind的孤头。
void absent_military_source(const SourceRef &source) {
  if (absent_military_domain(source.domain) ||
      absent_military_kind(source.kind))
    fail("UnselectedMilitaryRetainedSource", source.id);
}
// 有限列出的原业务表必须全空；任何保留正文都不能走未选阶段。
template <class... Tables>
void absent_military_tables(const Tables &...tables) {
  if (!(tables.empty() && ...))
    fail("UnselectedMilitaryRetainedState");
}
// 只接受真正未选；原解码器的中性item/troop定义并不赋予军旅能力，不能误拒。
void absent_military_catalog(const PreparedCatalog &catalog) {
  take(validate_native_military_state_catalog(catalog));
  const auto p =
      take(selected_native_military_state_versions(catalog.identity));
  if (p.dynamic_military || p.military_duty_catalog || p.military_action_time ||
      p.military_authority_time || p.military_action_resources ||
      p.military_return_detachments || p.military_action_conditions ||
      p.military_training_completion_time ||
      p.military_formation_reservations || !catalog.military ||
      catalog.military->military || catalog.dynamic_military_actions ||
      catalog.military_duty_catalog || catalog.military_spatial_definitions ||
      catalog.military_session_definitions || catalog.field_battle ||
      catalog.naval || catalog.population_military ||
      catalog.political_military_appointments ||
      catalog.political_field_appointments ||
      catalog.selected_military_occupation_schema != 1)
    fail("UnselectedMilitaryCatalogRequired");
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "military" || module.first == "dynamic_military" ||
        module.first == "field_battle" ||
        module.first.starts_with("military_") ||
        module.first.starts_with("naval_") ||
        module.first.starts_with("population_military") ||
        module.first.starts_with("political_military_") ||
        module.first.starts_with("political_field_"))
      fail("UnselectedMilitaryModulePresent", module.first);
  const auto &m = *catalog.military;
  const auto &battle = m.battle_catalog;
  if (m.named_custody_policy || m.siege_engineering_policy ||
      m.recovery_policy || m.garrison_rest_policy ||
      m.supply_attrition_policy || m.rout_retreat_policy ||
      m.command_shock_policy || m.combat_condition_policy ||
      m.commanded_damage_policy || m.command_resources_policy ||
      m.sequential_round_policy || m.field_terrain_policy ||
      m.field_command_policy || m.officer_assignment_policy ||
      m.request_policy || m.march_condition_model ||
      m.siege_integration.enabled || battle.supply_attrition ||
      battle.rout_retreat_enabled || battle.field_terrain_enabled ||
      battle.command_shock_rule || battle.combat_condition_rule ||
      battle.commanded_damage_rule || battle.sequential_round_rule ||
      battle.command_resources_rule || battle.tactic_learning)
    fail("UnselectedMilitaryPolicyPresent");
  absent_military_tables(m.offices, m.garrisons, m.orders, m.strongholds,
                         m.plans, m.siege_integration.officer_policies);
  if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
      !catalog.exact_time->complete)
    fail("MilitaryExactCompleteProfileRequired");
  const auto &actions = *catalog.exact_time->complete;
  if (selected_action(actions, exact_office_action_registration()) ||
      selected_action(actions, exact_formation_action_registration()) ||
      selected_action(actions, exact_military_training_registration()) ||
      selected_action(actions, exact_return_action_registration()) ||
      selected_action(actions, exact_resupply_action_registration()) ||
      selected_action(actions, exact_naval_repair_action_registration()) ||
      selected_action(actions, military_station_action_time_registration()))
    fail("UnselectedMilitaryActionProfilePresent");
  for (const auto &action : catalog.exact_time->complete->action_time_profiles)
    if (absent_military_kind(action.action_kind))
      fail("UnselectedMilitaryActionProfilePresent", action.action_kind);
}
// 同World原拥有根及所有军旅保留表必须确实无业务；不借writer、scope或第二份空状态。
void absent_military_world(const WorldBundle &w) {
  if (!w.catalog || !w.world_timeline || !w.action_elapsed)
    fail("UnselectedMilitaryActualRootsRequired");
  if (!same_selection(take(read_military_exact_phase_selection(*w.catalog)),
                      MilitaryExactPhaseSelection{}))
    fail("MilitaryExactWorldSelectionMismatch");
  take(validate_native_military_state_layout(w));
  take(validate_native_military_battle_lifecycle_layout(w));
  if (w.military_exact || w.military_action_time || w.field_battle ||
      w.naval_seed_complete || w.population_seed_complete ||
      w.military_tactic_learning_seed_complete ||
      w.political_military_appointments_seed_complete ||
      w.political_field_appointments_seed_complete)
    fail("UnselectedMilitaryOwnerPresent");
  const auto &m = w.military;
  absent_military_tables(
      m.named_custody.terms, m.named_custody.closures,
      m.named_custody.field_order, m.recovery.facts, m.recovery.battle_releases,
      m.recovery.siege_releases, m.recovery.field_order, m.garrison_rest.facts,
      m.garrison_rest.field_order, m.supply_attrition.facts,
      m.supply_attrition.field_order, m.rout.entries, m.rout.rounds,
      m.rout.field_order, m.field_commands, m.officer_assignments,
      m.request_facts, m.battles, m.sieges, m.siege_engineering.preparations,
      m.siege_engineering.field_order, m.siege_lifecycle.facts,
      m.siege_lifecycle.field_order, m.training_facts, m.training_actions,
      m.march_facts, m.occupation_facts, m.relief_facts, m.siege_material_facts,
      m.logistics_actions, m.logistics_facts, m.return_facts, m.muster_facts,
      m.resignation_facts, m.delivery_facts, m.settlement_facts,
      m.contact_facts, m.battle_facts, m.casualty_facts, m.offices, m.garrisons,
      m.military_units, m.armies, m.campaigns, m.arrival_facts, m.strongholds,
      m.actions, m.action_facts, m.supply_facts, m.siege_integration.proposals,
      m.siege_integration.actor_actions, m.siege_integration.decisions,
      m.siege_integration.attempts, m.integration_facts);
  const auto &n = w.naval;
  absent_military_tables(
      n.vessels, n.fleets, n.battle.battles, n.battle.contacts, n.battle.plans,
      n.battle.rounds, n.battle.captures, n.battle.declines,
      n.battle.continuations, n.battle.field_order, n.repairs.starts,
      n.repairs.results, n.repairs.field_order, n.field_order,
      n.repair_charges.charges, n.repair_charges.field_order,
      n.navigation.voyages, n.navigation.steps, n.navigation.field_order,
      n.gunnery.preparations, n.gunnery.field_order);
  const auto &p = w.population_military;
  absent_military_tables(p.enrollments, p.actions, p.action_facts, p.facts,
                         p.integration_facts,
                         w.military_tactic_learning.acquisitions,
                         w.military_tactic_learning.practices,
                         w.military_tactic_learning.field_order,
                         w.political_military_appointments.authorizations,
                         w.political_military_appointments.answers,
                         w.political_military_appointments.withdrawals,
                         w.political_field_appointments.authorizations,
                         w.political_field_appointments.answers,
                         w.political_field_appointments.withdrawals);
  const auto &common = w.common.state();
  for (const auto &row : common.facts.rows())
    if (absent_military_domain(row.second.domain) ||
        absent_military_kind(row.second.kind))
      fail("UnselectedMilitaryRetainedFact", row.first);
  for (const auto &row : common.actions.rows())
    if (absent_military_kind(row.second.kind))
      fail("UnselectedMilitaryRetainedAction", row.first);
  for (const auto &row : common.results.rows()) {
    if (!row.second)
      continue;
    if (absent_military_kind(row.second->kind))
      fail("UnselectedMilitaryRetainedResult", row.first);
    if (!row.second->details)
      continue;
    // 原typed回执独立于外壳kind；改名不能把保留军旅成果变成未选。
    const auto &data = row.second->details->data;
    if (std::holds_alternative<MilitaryEffect>(data) ||
        std::holds_alternative<SiegeIntegrationEffect>(data) ||
        std::holds_alternative<NavalCommandReceipt>(data) ||
        std::holds_alternative<NavalBattleCommandReceipt>(data) ||
        std::holds_alternative<NavalRepairReceipt>(data))
      fail("UnselectedMilitaryRetainedResult", row.first);
    if (const auto *advance = std::get_if<AdvanceDetails>(&data))
      for (const auto &completed : advance->completed_actions.rows())
        if (completed.second.military)
          fail("UnselectedMilitaryRetainedResult", row.first);
  }
  for (const auto &row : common.history)
    if (absent_military_kind(row.kind))
      fail("UnselectedMilitaryRetainedHistory", row.result_id);
  for (const auto &row : common.actors.rows())
    if (std::holds_alternative<InArmyLocation>(row.second.location))
      fail("UnselectedMilitaryActorBinding", row.first);
  const auto reservation = [](const ReservationRef &r) {
    if (absent_military_domain(r.domain_id) ||
        absent_military_kind(r.activity_kind))
      fail("UnselectedMilitaryReservation");
  };
  for (const auto &row : common.wallets.rows())
    for (const auto &held : row.second.reserved)
      reservation(held.action_id);
  for (const auto &row : common.inventories.rows())
    for (const auto &lot : row.second.lots)
      reservation(lot.reserved_by);
  for (const auto &row : w.action_elapsed->terminal_bindings) {
    if (absent_military_kind(row.action_kind))
      fail("UnselectedMilitaryElapsedTerminal", row.action_ref);
    absent_military_source(row.original_execution_source_ref);
    std::visit([](const auto &v) { absent_military_source(v.source_ref); },
               row.started_source_ref);
    std::visit([](const auto &v) { absent_military_source(v.source_ref); },
               row.ended_source_ref);
  }
  for (const auto &row : w.action_elapsed->fixed_service_transitions) {
    absent_military_source(row.execution_source_ref);
    std::visit(
        [](const auto &v) {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, FixedOwnerTerminated>)
            absent_military_source(v.owner_result_ref);
          else if constexpr (std::is_same_v<T, FixedEligibilityChanged>)
            absent_military_source(v.original_cause_ref);
          else
            absent_military_source(v.owner_source_ref);
        },
        row.cause);
    for (const auto *inputs :
         {&row.input_basis_refs, &row.previous_input_basis_refs})
      for (const auto &input : *inputs)
        if (const auto *applied = std::get_if<FixedAppliedInput>(&input))
          absent_military_source(applied->source_ref);
  }
}
// 原期真实写上下文和期尾必须同时成立；未选分支也不接受脱离dispatcher的成功调用。
void absent_military_boundary(const WorldBundle &w, const ExactPeriodContext &p,
                              const ExactPhaseWriteContext &writes) {
  absent_military_world(w);
  if (w.world_timeline->boundary_state != PeriodBoundaryState::open ||
      w.world_timeline->period_id != p.period_id || p.period_id.empty() ||
      p.opening_at.half_day_tick < 0 ||
      p.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
      p.through_tick != p.opening_at.half_day_tick + 1 ||
      w.common.tick() != p.opening_at.half_day_tick ||
      w.active_exact_write_context() != &writes ||
      !w.common.actor_condition_capture_identity() ||
      compare(p.opening_at.sub_half_day, {}) != 0 ||
      !same_moment(take(w.actual_phase_moment()),
                   WorldMoment{p.through_tick, {}}))
    fail("UnselectedMilitaryActualCalendarBoundaryRequired", p.period_id);
}
// 热候选只记录同栈四腿已证无工作；七类真实输出全部反核为空，绝不制造来源或结果。
void absent_military_candidate(const MilitaryCalendarCandidate &c) {
  absent_military_tables(
      c.original_siege_step.events, c.original_siege_step.battle_requests,
      c.actual_completed_battle_sources, c.completed_receipt_ids,
      c.actual_journey_requests, c.actual_siege_entry_sources,
      c.actual_consequence_sources);
}
// 保原六阶段与排他键，仅在每次阴性证明后记录本期无工作，不安装业务能力。
ExactPhaseRegistration absent_military_registration() {
  ExactPhaseRegistration r;
  r.provider = ExactProviderId::military;
  r.profile_id = military_exact_phase_profile_id;
  r.version = 1;
  r.stages = {ExactStage::same_moment_hits,
              ExactStage::military_duty_period,
              ExactStage::military_siege_period,
              ExactStage::military_siege_battles_finished,
              ExactStage::siege_results,
              ExactStage::campaign_adoption};
  r.calendar_stages = r.stages;
  r.period_source_owner = false;
  r.interval_writer = false;
  // 保固定阶段排他键，不注册军旅Action、期间来源、due或业务writer。
  r.effect_writer_keys = {"military.dynamic_land_effects",
                          "military.original_duty_service",
                          "military.original_duty_period",
                          "military.original_siege_period",
                          "military.original_siege_battles_finished",
                          "military.original_siege_results",
                          "military.original_campaign_adoption"};
  r.validate_author_selection = [](const WorldBundle &w) {
    return status([&] { absent_military_world(w); });
  };
  r.validate_world_sources = [](const WorldBundle &w, const WorldMoment &at) {
    return status([&] {
      absent_military_world(w);
      if (!same_moment(take(w.actual_phase_moment()), at))
        fail("UnselectedMilitaryActualMomentMismatch");
    });
  };
  r.apply_calendar_stage = [](WorldBundle &w, ExactStage stage,
                              const ExactPeriodContext &p,
                              ExactPhaseWriteContext &writes) {
    return status([&] {
      absent_military_boundary(w, p, writes);
      if (stage != ExactStage::same_moment_hits &&
          stage != ExactStage::military_duty_period &&
          stage != ExactStage::military_siege_period &&
          stage != ExactStage::military_siege_battles_finished &&
          stage != ExactStage::siege_results &&
          stage != ExactStage::campaign_adoption)
        fail("UnselectedMilitaryCalendarStageMismatch");
      auto &c = *take(acquire_military_calendar_candidate(writes, p));
      absent_military_candidate(c);
      switch (stage) {
      case ExactStage::military_siege_period:
        if (c.siege_period_consumed || c.siege_battles_consumed ||
            c.siege_results_consumed || c.campaign_consumed)
          fail("MilitarySiegePeriodAlreadyConsumed", p.period_id);
        c.siege_period_consumed = true;
        break;
      case ExactStage::military_siege_battles_finished:
        if (!c.siege_period_consumed || c.siege_battles_consumed ||
            c.siege_results_consumed || c.campaign_consumed)
          fail("MilitarySiegeBattleFinishStageMismatch", p.period_id);
        c.siege_battles_consumed = true;
        break;
      case ExactStage::siege_results:
        if (!c.siege_period_consumed || !c.siege_battles_consumed ||
            c.siege_results_consumed || c.campaign_consumed)
          fail("MilitarySiegeResultsStageMismatch", p.period_id);
        c.siege_results_consumed = true;
        break;
      case ExactStage::campaign_adoption:
        if (!c.siege_period_consumed || !c.siege_battles_consumed ||
            !c.siege_results_consumed || c.campaign_consumed)
          fail("MilitaryCampaignStageMismatch", p.period_id);
        c.campaign_consumed = true;
        take(validate_military_calendar_candidate_consumed(c, p));
        break;
      default:
        break;
      }
      absent_military_candidate(c);
    });
  };
  return r;
}

template <class T> void selected_context(bool selected, const T *value) {
  if (selected != bool(value))
    fail("MilitaryExactSelectedContextRequired");
}
void read_binding(const WorldBundle &w, const MilitaryExactReadBinding &x,
                  const MilitaryExactPhaseSelection &selected,
                  const WorldMoment &at) {
  if (!w.catalog || !w.catalog->military || !w.catalog->exact_time ||
      !w.action_elapsed || &x.common != &w.common ||
      &x.military != &w.military || &x.catalog != w.catalog->military.get() ||
      &x.elapsed != &*w.action_elapsed ||
      x.dynamic != (w.military_exact ? &*w.military_exact : nullptr) ||
      bool(x.dynamic) != bool(selected.dynamic_schema))
    fail("CrossCandidateMilitaryExactRead");
  take(validate_native_military_state_layout(w));
  take(validate_native_military_battle_lifecycle_layout(w));
  const auto lifecycle =
      take(selected_native_military_battle_lifecycle(w.catalog->identity));
  if (x.access.military_battle_lifecycle_schema != lifecycle ||
      bool(x.access.battle_lifecycle) != bool(lifecycle))
    fail("MilitaryExactBattleLifecycleSelectionMismatch");
  if (lifecycle && (&x.access.battle_lifecycle->common != &w.common ||
                    &x.access.battle_lifecycle->state != &w.military ||
                    &x.access.battle_lifecycle->clock != &x.clock))
    fail("CrossCandidateMilitaryExactBattleLifecycle");
  if (!same_selection(selected,
                      take(read_military_exact_phase_selection(*w.catalog))))
    fail("MilitaryExactPhaseSelectionChanged");
  if (x.action_time !=
          (w.military_action_time ? &*w.military_action_time : nullptr) ||
      (x.naval && x.naval != &w.naval))
    fail("CrossCandidateMilitaryExactRead");
  if (!same_moment(take(w.actual_phase_moment()), at, x.budget) ||
      !same_moment(take(x.clock.read_current_moment()), at, x.budget))
    fail("MilitaryExactActualMomentRequired");
  selected_context(selected.field_original, x.field_supply);
  if (x.field_supply && (!x.field_supply->field_supply_source1 ||
                         x.field_supply->dynamic_military ||
                         &x.field_supply->action_time != x.action_time ||
                         &x.field_supply->elapsed != &x.elapsed ||
                         &x.field_supply->clock != &x.clock))
    fail("CrossCandidateFieldMilitarySupplySources");
  selected_context(selected.actions.office_accept, x.office);
  selected_context(selected.actions.army_form, x.formation);
  selected_context(selected.actions.military_train, x.training);
  selected_context(selected.actions.army_return, x.returning);
  selected_context(selected.actions.army_resupply, x.resupply);
  selected_context(selected.naval_repair, x.naval_repair);
  selected_context(selected.naval_repair, x.naval);
  // This explicitly selected phase owns Duty and dynamic land source closure.
  // Its original contexts remain mandatory even when their retained tables are
  // empty.
  selected_context(selected.dynamic_schema != 0, x.duty);
  selected_context(selected.dynamic_schema != 0, x.dynamic_sources);
  auto action = [&](const auto *context) {
    if (context &&
        (&context->catalog != &x.catalog || &context->access != &x.access ||
         &context->clock != &x.clock ||
         &context->scale != &w.catalog->exact_time->scale_ref))
      fail("CrossCandidateMilitaryExactContext");
  };
  action(x.office);
  action(x.formation);
  action(x.training);
  action(x.returning);
  action(x.resupply);
  if (x.office && (&x.office->prepared != w.catalog.get() ||
                   &x.office->organization != &w.organization ||
                   &x.office->family != &w.family))
    fail("CrossCandidateMilitaryExactOfficeRoots");
  if (x.returning &&
      (x.returning->exact != x.dynamic ||
       (x.returning->identity &&
        x.returning->identity != &w.catalog->identity) ||
       (!x.returning->identity && !x.returning->exact) ||
       &x.returning->population != &w.population_military ||
       x.returning->population_catalog != w.catalog->population_military.get()))
    fail("CrossCandidateMilitaryExactReturnRoots");
  if (x.naval_repair &&
      (!w.catalog->naval || &x.naval_repair->military_catalog != &x.catalog ||
       &x.naval_repair->naval_catalog != w.catalog->naval.get() ||
       &x.naval_repair->access != &x.access ||
       &x.naval_repair->clock != &x.clock ||
       &x.naval_repair->scale != &w.catalog->exact_time->scale_ref))
    fail("CrossCandidateMilitaryExactNavalRepair");
  if (selected.dynamic_schema) {
    if (!w.catalog->military_duty_catalog ||
        &x.duty->catalog != w.catalog->military_duty_catalog.get() ||
        &x.duty->scale != &w.catalog->exact_time->scale_ref ||
        !same_moment(take(x.duty->stamps.read_current_moment()), at,
                     x.budget) ||
        &x.dynamic_sources->actions.clock != &x.clock ||
        &x.dynamic_sources->land.clock != &x.clock ||
        !w.catalog->dynamic_military_actions ||
        &x.dynamic_sources->actions.catalog !=
            w.catalog->dynamic_military_actions.get() ||
        &x.dynamic_sources->battle_catalog != &x.catalog.battle_catalog)
      fail("CrossCandidateMilitaryExactOriginalSources");
    if (x.duty->selected_dynamic_military != selected.dynamic_schema)
      fail("MilitaryDutySelectedContextMismatch");
    if (selected.dynamic_schema == 4) {
      if (!x.duty->assignments ||
          !x.duty->assignments->bound_to(w.common, x.dynamic->duty_sources,
                                         x.duty->catalog))
        fail("MilitaryDutyAssignmentOriginalSourcesRequired");
      if (!x.duty->condition_sources || !w.actor_foundation ||
          !w.catalog->actor_foundation || !w.world_timeline ||
          !w.catalog->complete_exact_time_runtime)
        fail("MilitaryDutyConditionSourcesRequired");
      // sources() 返回值由真 lease 持有；這裡只核同一實際 World 根與 M，
      // 不重取完整 owner，不借 Motion 下一次 bindings() 會重建的 body。
      const auto &body = *x.duty->condition_sources;
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
          body.military != &w.military || body.military_exact != x.dynamic ||
          body.personal_catalog != w.catalog->personal.get() ||
          body.family_catalog != w.catalog->family.get() ||
          body.director != &w.director ||
          body.director_catalog != w.catalog->director.get() ||
          body.rest_estate_state != (w.catalog->estate ? &w.estate : nullptr) ||
          body.permanent_growth !=
              (w.growth_permanent ? &*w.growth_permanent : nullptr) ||
          body.permanent_growth_catalog != w.catalog->growth_permanent.get() ||
          body.interaction != &w.interaction ||
          body.interaction_catalog != w.catalog->interaction.get() ||
          !same_moment(body.actual_now, at, x.budget))
        fail("CrossCandidateMilitaryDutyConditionSources");
    }
    if ((selected.dynamic_schema == 3 || selected.dynamic_schema == 4) &&
        !x.dynamic_sources->tactical_ownership)
      fail("MilitaryExactTacticalOwnership3Required");
  }
}
void write_binding(WorldBundle &w, const MilitaryExactWriteBinding &x,
                   const MilitaryExactPhaseSelection &selected,
                   const WorldMoment &at) {
  read_binding(w, x.read, selected, at);
  if (&x.common != &w.common || &x.military != &w.military ||
      &x.elapsed != &*w.action_elapsed ||
      x.action_time !=
          (w.military_action_time ? &*w.military_action_time : nullptr) ||
      x.dynamic != (w.military_exact ? &*w.military_exact : nullptr) ||
      x.naval != (selected.naval_repair ? &w.naval : nullptr) ||
      &x.calendar.common != &w.common || &x.calendar.military != &w.military ||
      &x.calendar.catalog != &x.read.catalog ||
      &x.calendar.access != &x.read.access ||
      x.calendar.field_without_siege != selected.field_original)
    fail("CrossCandidateMilitaryExactWrite");
  selected_context(selected.actions.office_accept, x.office);
  selected_context(selected.actions.army_form, x.formation);
  selected_context(selected.actions.military_train, x.training);
  selected_context(selected.actions.army_return, x.returning);
  selected_context(selected.actions.army_resupply, x.resupply);
  selected_context(selected.naval_repair, x.naval_repair);
  auto action = [&](const auto *write, const auto *read) {
    if (write && (&write->read.original != &read->original ||
                  &write->read.catalog != &read->catalog ||
                  &write->read.access != &read->access ||
                  &write->read.clock != &read->clock ||
                  &write->read.scale != &read->scale))
      fail("CrossCandidateMilitaryExactWriteContext");
  };
  action(x.office, x.read.office);
  action(x.formation, x.read.formation);
  action(x.training, x.read.training);
  action(x.returning, x.read.returning);
  action(x.resupply, x.read.resupply);
  if (x.formation && &x.formation->read.periods != &x.read.formation->periods)
    fail("CrossCandidateMilitaryExactFormationPeriods");
  if (x.training && &x.training->read.periods != &x.read.training->periods)
    fail("CrossCandidateMilitaryExactTrainingPeriods");
  if (x.office &&
      (&x.office->read.prepared != &x.read.office->prepared ||
       &x.office->read.organization != &x.read.office->organization ||
       &x.office->read.family != &x.read.office->family ||
       &x.office->read.body != &x.read.office->body))
    fail("CrossCandidateMilitaryExactOfficeWriteRoots");
  if (x.returning &&
      (x.returning->read.exact != x.read.returning->exact ||
       x.returning->read.identity != x.read.returning->identity ||
       x.returning->read.field_history != x.read.returning->field_history ||
       &x.returning->read.formations != &x.read.returning->formations ||
       &x.returning->read.conditions != &x.read.returning->conditions ||
       &x.returning->read.population != &x.read.returning->population ||
       x.returning->read.population_catalog !=
           x.read.returning->population_catalog ||
       &x.returning->read.periods != &x.read.returning->periods))
    fail("CrossCandidateMilitaryExactReturnWriteRoots");
  if (x.naval_repair &&
      (&x.naval_repair->read.original != &x.read.naval_repair->original ||
       &x.naval_repair->read.military_catalog != &x.read.catalog ||
       &x.naval_repair->read.naval_catalog != w.catalog->naval.get() ||
       &x.naval_repair->read.access != &x.read.naval_repair->access ||
       &x.naval_repair->read.scale != &x.read.naval_repair->scale ||
       &x.naval_repair->read.clock != &x.read.clock))
    fail("CrossCandidateMilitaryExactNavalWrite");
  selected_context(selected.dynamic_schema != 0, x.dynamic_sources);
  if (selected.dynamic_schema) {
    if (!x.dynamic_sources ||
        &x.dynamic_sources->actions != &x.read.dynamic_sources->actions ||
        &x.dynamic_sources->originals != &x.read.dynamic_sources->effects ||
        &x.dynamic_sources->land != &x.read.dynamic_sources->land ||
        &x.dynamic_sources->battle_catalog !=
            &x.read.dynamic_sources->battle_catalog ||
        &x.dynamic_sources->objectives != x.read.dynamic_sources->objectives)
      fail("CrossCandidateMilitaryExactDynamicWrite");
  }
}
// Field 未选旧 Duty/Station 时只证明原工作不存在，不替代其来源或执行任何军令。
bool field_unselected_duty_kind(std::string_view kind) {
  return kind == "military_duty" || kind == "military_duty_assignment" ||
         kind == "military_duty_control" ||
         kind == "military_duty_execution_bound" ||
         kind == "military_duty_execution_stopped" ||
         kind == "military_duty_period" || kind == "military_duty_presence" ||
         kind == "military_duty_terms" || kind == "military_duty_window" ||
         kind == "military_station" || kind == "military_station_source" ||
         kind == "military_station_terminal";
}
void reject_field_unselected_duty_source(const SourceRef &source) {
  if (field_unselected_duty_kind(source.kind))
    fail("FieldUnselectedMilitaryDutySource", source.id);
}
// 只在同 World 的真实期尾写栈内承接阴性屏障；经济 Opening 仍是唯一期间原源。
void field_unselected_duty_barrier(
    const WorldBundle &w, const MilitaryExactWriteBinding &x,
    const MilitaryExactPhaseSelection &selected, const ExactPeriodContext &p,
    const ExactPhaseWriteContext &writes) {
  if (!selected.field_original || selected.dynamic_schema || selected.station ||
      w.military_exact || x.dynamic || x.read.dynamic || x.read.duty ||
      x.dynamic_sources || x.read.dynamic_sources ||
      !x.read.field_supply || !w.world_timeline || !w.action_elapsed ||
      w.catalog->military_duty_catalog)
    fail("FieldUnselectedMilitaryDutyRequired");
  if (w.world_timeline->boundary_state != PeriodBoundaryState::open ||
      w.world_timeline->period_id != p.period_id || p.period_id.empty() ||
      p.opening_at.half_day_tick < 0 ||
      p.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
      p.through_tick != p.opening_at.half_day_tick + 1 ||
      w.common.tick() != p.opening_at.half_day_tick ||
      w.active_exact_write_context() != &writes ||
      !w.common.actor_condition_capture_identity() ||
      compare(p.opening_at.sub_half_day, {}) != 0 ||
      !same_moment(take(w.actual_phase_moment()),
                   WorldMoment{p.through_tick, {}}, x.read.budget))
    fail("FieldMilitaryDutyActualCalendarBoundaryRequired", p.period_id);
  const auto original = take(x.read.field_supply->periods.read_training_period(
      p.opening_at.half_day_tick));
  if (original.period_ref != p.period_id ||
      original.from_tick != p.opening_at.half_day_tick ||
      original.to_tick != p.through_tick ||
      !same_moment(original.opening_at, p.opening_at, x.read.budget))
    fail("FieldMilitaryDutyOriginalPeriodRequired", p.period_id);
  const auto &common = w.common.state();
  for (const auto &row : common.facts.rows())
    if (field_unselected_duty_kind(row.second.kind))
      fail("FieldUnselectedMilitaryDutyFact", row.first);
  for (const auto &row : common.actions.rows())
    if (field_unselected_duty_kind(row.second.kind))
      fail("FieldUnselectedMilitaryDutyAction", row.first);
  for (const auto &row : common.results.rows())
    if (row.second && field_unselected_duty_kind(row.second->kind))
      fail("FieldUnselectedMilitaryDutyResult", row.first);
  for (const auto &row : common.history)
    if (field_unselected_duty_kind(row.kind))
      fail("FieldUnselectedMilitaryDutyHistory", row.result_id);
  const auto reservation = [](const ReservationRef &r) {
    if (field_unselected_duty_kind(r.activity_kind))
      fail("FieldUnselectedMilitaryDutyReservation");
  };
  for (const auto &row : common.wallets.rows())
    for (const auto &held : row.second.reserved)
      reservation(held.action_id);
  for (const auto &row : common.inventories.rows())
    for (const auto &lot : row.second.lots)
      reservation(lot.reserved_by);
  for (const auto &row : w.action_elapsed->terminal_bindings) {
    if (field_unselected_duty_kind(row.action_kind))
      fail("FieldUnselectedMilitaryDutyElapsedTerminal", row.action_ref);
    reject_field_unselected_duty_source(row.original_execution_source_ref);
    std::visit([](const auto &v) {
      reject_field_unselected_duty_source(v.source_ref);
    }, row.started_source_ref);
    std::visit([](const auto &v) {
      reject_field_unselected_duty_source(v.source_ref);
    }, row.ended_source_ref);
  }
  for (const auto &row : w.action_elapsed->fixed_service_transitions) {
    reject_field_unselected_duty_source(row.execution_source_ref);
    std::visit([](const auto &v) {
      using T = std::decay_t<decltype(v)>;
      if constexpr (std::is_same_v<T, FixedOwnerTerminated>)
        reject_field_unselected_duty_source(v.owner_result_ref);
      else if constexpr (std::is_same_v<T, FixedEligibilityChanged>)
        reject_field_unselected_duty_source(v.original_cause_ref);
      else
        reject_field_unselected_duty_source(v.owner_source_ref);
    }, row.cause);
    for (const auto *inputs :
         {&row.input_basis_refs, &row.previous_input_basis_refs})
      for (const auto &input : *inputs)
        if (const auto *applied = std::get_if<FixedAppliedInput>(&input))
          reject_field_unselected_duty_source(applied->source_ref);
  }
}
// One bindings() snapshot per acquisition. A later reconstruction by an inner
// lease is not permission to keep references to its previous snapshot.
struct ReadAccess {
  std::shared_ptr<const MilitaryExactReadLease> lease;
  MilitaryExactReadBinding binding;
};
struct WriteAccess {
  std::shared_ptr<MilitaryExactWriteLease> lease;
  MilitaryExactWriteBinding binding;
};
ReadAccess read(MilitaryExactWorldFactory factory, const WorldBundle &w,
                const MilitaryExactPhaseSelection &selected,
                const WorldMoment &at) {
  if (!factory.read || !factory.write)
    fail("MilitaryExactWorldFactoryRequired");
  auto lease = take(factory.read(w, at));
  if (!lease)
    fail("MilitaryExactReadLeaseRequired");
  auto binding = lease->bindings();
  read_binding(w, binding, selected, at);
  return {std::move(lease), binding};
}
WriteAccess write(MilitaryExactWorldFactory factory, WorldBundle &w,
                  const MilitaryExactPhaseSelection &selected,
                  const WorldMoment &at, ExactPhaseWriteContext &writes) {
  if (!factory.read || !factory.write)
    fail("MilitaryExactWorldFactoryRequired");
  auto lease = take(factory.write(w, at, writes));
  if (!lease)
    fail("MilitaryExactWriteLeaseRequired");
  auto binding = lease->bindings();
  write_binding(w, binding, selected, at);
  return {std::move(lease), binding};
}
template <class T> void append(std::vector<T> &target, std::vector<T> value) {
  target.insert(target.end(), std::make_move_iterator(value.begin()),
                std::make_move_iterator(value.end()));
}
MilitaryActionTimeReadContexts
action_contexts(const MilitaryExactReadBinding &x) {
  MilitaryActionTimeReadContexts out;
  if (x.office)
    out.office.emplace(*x.office);
  if (x.formation)
    out.formation.emplace(*x.formation);
  if (x.training)
    out.training.emplace(*x.training);
  if (x.returning)
    out.returning.emplace(*x.returning);
  if (x.resupply)
    out.resupply.emplace(*x.resupply);
  return out;
}
void validate_sources(const MilitaryExactReadBinding &x,
                      const MilitaryExactPhaseSelection &selected,
                      const WorldMoment &at) {
  if (selected.field_original) {
    if (!x.field_supply)
      fail("FieldMilitarySupplyOriginalSourcesRequired");
    take(validate_military_state(x.common, x.military, x.catalog, x.access,
                                 *x.field_supply));
  }
  if (x.action_time) {
    NativeMilitaryActionTimeRead actions(x.common, x.military, *x.action_time,
                                         x.elapsed, selected.actions,
                                         action_contexts(x));
    take(actions.validate_selected_sources());
    if (x.office)
      take(validate_exact_office_sources(x.common, x.military, *x.action_time,
                                         x.elapsed, *x.office));
    if (x.formation)
      take(validate_exact_formation_sources(
          x.common, x.military, *x.action_time, x.elapsed, *x.formation));
    if (x.training)
      take(validate_exact_military_training_sources(
          x.common, x.military, *x.action_time, x.elapsed, *x.training));
    if (x.returning)
      take(validate_exact_return_sources(x.common, x.military, *x.action_time,
                                         x.elapsed, *x.returning));
    if (x.resupply)
      take(validate_exact_resupply_sources(x.common, x.military, *x.action_time,
                                           x.elapsed, *x.resupply));
    if (x.naval_repair)
      take(validate_exact_naval_repair_sources(x.common, x.military, *x.naval,
                                               *x.action_time, x.elapsed,
                                               *x.naval_repair));
    else if (!x.action_time->naval_repair_time_sources.empty())
      fail("UnselectedMilitaryExactNavalRepairSource");
  }
  if (selected.dynamic_schema) {
    if (!selected.station && !x.dynamic->duty_sources.station_sources.empty())
      fail("UnselectedMilitaryExactStationSource");
    take(validate_military_duty_sources(x.common, x.dynamic->duty_sources,
                                        *x.duty, at));
    take(validate_exact_dynamic_military_phase_sources(
        x.common, x.military, *x.dynamic, *x.dynamic_sources, at));
  }
}
// Routing uses the actual retained original tag. The selected specialized
// validator still proves all terms/source/history; no result is reconstructed.
const StableId &fixed_kind(const MilitaryActionTimeState &s,
                           const StableId &id) {
  const auto *action = s.dynamic_action_facts.find(id);
  const auto *logistics = s.dynamic_logistics_facts.find(id);
  if (bool(action) == bool(logistics))
    fail("MilitaryExactActionOwnerAmbiguous", id);
  return action ? action->command : logistics->command;
}
Outcome<ExactEventApplied> apply_fixed(MilitaryExactWriteBinding &x,
                                       const ExactEventPrediction &p,
                                       ExactPhaseWriteContext &writes,
                                       bool field_original) {
  if (!x.action_time)
    fail("MilitaryExactActionTimeRequired");
  const auto &kind = fixed_kind(*x.action_time, p.actual_source_basis.id);
  if (kind == "office_accept" && x.office)
    return apply_exact_office_event(x.common, x.military, *x.action_time,
                                    x.elapsed, *x.office, p, writes);
  if (kind == "army_form" && x.formation) {
    // 只为显式 Field 原编军补齐资源捕获；动态旧选型及借来捕获均保持原契约。
    std::optional<ResourceExecutionCoverageBoundary> resource_coverage;
    if (field_original &&
        x.formation->read.access.military_action_resources_schema &&
        !x.common.resource_execution_capture_identity())
      resource_coverage.emplace(take(begin_resource_execution_coverage(x.common)));
    auto applied = take(apply_exact_formation_event(
        x.common, x.military, *x.action_time, x.elapsed, *x.formation, p, writes));
    // 成败原正文、预约释放及持久资源来源都已写完；只终结自己开启的捕获。
    // 覆盖失败保原错误并撤整个外层候选，不交付伪成功。
    if (resource_coverage)
      take(finish_resource_execution_coverage(x.common, *resource_coverage));
    return {true, std::move(applied), {}};
  }
  if (kind == "army_return" && x.returning)
    return apply_exact_return_event(x.common, x.military, *x.action_time,
                                    x.elapsed, *x.returning, p, writes);
  if (kind == "army_resupply" && x.resupply)
    return apply_exact_resupply_event(x.common, x.military, *x.action_time,
                                      x.elapsed, *x.resupply, p, writes);
  fail("UnselectedMilitaryExactAction", kind);
  return {};
}
Status validate_fixed(const MilitaryExactReadBinding &x,
                      const ExactEventApplied &event) {
  if (!x.action_time)
    fail("MilitaryExactActionTimeRequired");
  const auto &kind = fixed_kind(*x.action_time, event.applied_result.id);
  if (kind == "office_accept" && x.office)
    return validate_exact_office_applied_source(
        x.common, x.military, *x.action_time, x.elapsed, *x.office, event);
  if (kind == "army_form" && x.formation)
    return validate_exact_formation_applied_source(
        x.common, x.military, *x.action_time, x.elapsed, *x.formation, event);
  if (kind == "army_return" && x.returning)
    return validate_exact_return_applied_source(
        x.common, x.military, *x.action_time, x.elapsed, *x.returning, event);
  if (kind == "army_resupply" && x.resupply)
    return validate_exact_resupply_applied_source(
        x.common, x.military, *x.action_time, x.elapsed, *x.resupply, event);
  fail("UnselectedMilitaryExactAction", kind);
  return {};
}
} // namespace
// 沿原显式阶段选型读取；仅真正未选返回零，已选物理与Duty要求全部保留。
Outcome<MilitaryExactPhaseSelection>
read_military_exact_phase_selection(const PreparedCatalog &catalog) {
  return guard<MilitaryExactPhaseSelection>([&] {
    if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
        !catalog.exact_time->complete)
      fail("MilitaryExactCompleteProfileRequired");
    const ExactPhaseSelection *phase = nullptr;
    for (const auto &item : catalog.exact_time->complete->phase_providers)
      if (item.provider == ExactProviderId::military) {
        if (phase)
          fail("DuplicateMilitaryExactPhaseSelection");
        phase = &item;
      }
    if (!phase || phase->profile_id != military_exact_phase_profile_id ||
        phase->version != 1)
      fail("MilitaryExactOriginalPhaseRequired");
    take(validate_native_military_state_catalog(catalog));
    const auto physical =
        take(selected_native_military_state_versions(catalog.identity));
    bool field = false;
    for (const auto &item : catalog.identity.module_versions)
      if (item.first == "field_battle") {
        if (field || item.second != "1")
          fail("MilitaryExactFieldSelectionMismatch");
        field = true;
      }
    if (field != bool(catalog.field_battle))
      fail("MilitaryExactFieldSelectionMismatch");
    if (!physical.dynamic_military && !field) {
      absent_military_catalog(catalog);
      return MilitaryExactPhaseSelection{};
    }
    if (physical.dynamic_military) {
      if (physical.military_duty_catalog != 1 || !catalog.military_duty_catalog)
        fail("MilitaryExactDutyCatalogRequired");
      if (physical.dynamic_military != 2 && physical.dynamic_military != 3 &&
          physical.dynamic_military != 4)
        fail("MilitaryExactDynamicStateRequired");
    } else if (physical.military_duty_catalog ||
               catalog.military_duty_catalog ||
               catalog.dynamic_military_actions ||
               catalog.military_spatial_definitions ||
               catalog.military_session_definitions) {
      fail("UnselectedMilitaryDynamicCatalogPresent");
    }
    MilitaryExactPhaseSelection selected;
    selected.dynamic_schema = physical.dynamic_military;
    selected.field_original = field && !physical.dynamic_military;
    const auto &actions = *catalog.exact_time->complete;
    selected.actions.office_accept =
        selected_action(actions, exact_office_action_registration());
    selected.actions.army_form =
        selected_action(actions, exact_formation_action_registration());
    selected.actions.military_train =
        selected_action(actions, exact_military_training_registration());
    selected.actions.army_return =
        selected_action(actions, exact_return_action_registration());
    selected.actions.army_resupply =
        selected_action(actions, exact_resupply_action_registration());
    selected.naval_repair =
        selected_action(actions, exact_naval_repair_action_registration());
    selected.station =
        selected_action(actions, military_station_action_time_registration());
    if (selected.field_original &&
        (selected.actions.military_train || selected.actions.army_return ||
         selected.actions.army_resupply || selected.naval_repair ||
         selected.station || catalog.naval))
      fail("MilitaryFieldOriginalActionSelectionMismatch");
    // 本 Field 分支只接原基础粮费；未接的旧动态后果不能由空 provider 跳过。
    if (selected.field_original &&
        (catalog.military->supply_attrition_policy ||
         catalog.military->recovery_policy ||
         catalog.military->garrison_rest_policy ||
         catalog.military->battle_catalog.supply_attrition))
      fail("MilitaryFieldOriginalSupplyConsequencesRequired");
    if ((any_action(selected.actions) || selected.naval_repair) &&
        physical.military_action_time != 1)
      fail("MilitaryExactActionStateRequired");
    if (selected.naval_repair &&
        (!catalog.naval || !catalog.naval->repair_policy))
      fail("MilitaryExactOriginalNavalRepairRequired");
    return selected;
  });
}
std::vector<ActionTimeRegistration> military_exact_action_time_registrations(
    const MilitaryExactPhaseSelection &selected) {
  auto actions = selected_military_action_time_registrations(selected.actions);
  if (selected.naval_repair)
    actions.push_back(exact_naval_repair_action_registration());
  if (selected.station)
    actions.push_back(military_station_action_time_registration());
  return actions;
}
// 未选使用严格阴性日历屏障；已选仍借原军旅scope和原业务writer。
ExactPhaseRegistration
make_military_exact_phase_registration(MilitaryExactWorldFactory factory,
                                       MilitaryExactPhaseSelection selected) {
  if (!selected.field_original && !selected.dynamic_schema &&
      !any_action(selected.actions) && !selected.naval_repair &&
      !selected.station)
    return absent_military_registration();
  const auto compact = compact_selection(selected);
  ExactPhaseRegistration r;
  r.provider = ExactProviderId::military;
  r.profile_id = military_exact_phase_profile_id;
  r.version = 1;
  r.stages = {ExactStage::military_siege_period,
              ExactStage::military_siege_battles_finished,
              ExactStage::siege_results, ExactStage::campaign_adoption};
  r.calendar_stages = {ExactStage::military_siege_period,
                       ExactStage::military_siege_battles_finished,
                       ExactStage::siege_results,
                       ExactStage::campaign_adoption};
  if (any_action(selected.actions) || selected.naval_repair)
    r.stages.push_back(ExactStage::action_completions);
  if (selected.actions.military_train)
    r.stages.push_back(ExactStage::immediate_consequences);
  r.interval_writer = true;
  r.interval_after = {ExactProviderId::motion};
  if (selected.dynamic_schema) {
    r.stages.insert(r.stages.begin(), {ExactStage::same_moment_hits,
                                       ExactStage::military_duty_period});
  }
  if (selected.field_original && !selected.dynamic_schema) {
    r.stages.push_back(ExactStage::military_duty_period);
    r.calendar_stages.push_back(ExactStage::military_duty_period);
  }
  r.effect_writer_keys = {"military.original_siege_period",
                          "military.original_siege_battles_finished",
                          "military.original_siege_results",
                          "military.original_campaign_adoption"};
  if (selected.dynamic_schema) {
    r.effect_writer_keys.push_back("military.dynamic_land_effects");
    r.effect_writer_keys.push_back("military.original_duty_service");
    r.effect_writer_keys.push_back("military.original_duty_period");
  }
  if (selected.field_original && !selected.dynamic_schema)
    r.effect_writer_keys.push_back("military.original_duty_period");
  if (selected.actions.office_accept)
    r.effect_writer_keys.push_back("military.original_office_action");
  if (selected.actions.army_form)
    r.effect_writer_keys.push_back("military.original_formation_action");
  if (selected.actions.military_train)
    r.effect_writer_keys.push_back("military.original_training_action");
  if (selected.actions.army_return)
    r.effect_writer_keys.push_back("military.original_return_action");
  if (selected.actions.army_resupply)
    r.effect_writer_keys.push_back("military.original_resupply_action");
  if (selected.naval_repair)
    r.effect_writer_keys.push_back("military.original_naval_repair_action");
  r.validate_author_selection = [factory, compact](const WorldBundle &w) {
    const auto selected = restore_selection(compact);
    return status([&] {
      const auto at = take(w.actual_phase_moment());
      auto lease = read(factory, w, selected, at);
      const auto &x = lease.binding;
      if (selected.dynamic_schema)
        take(validate_military_duty_catalog(x.duty->catalog));
    });
  };
  r.validate_world_sources = [factory, compact](const WorldBundle &w,
                                                 const WorldMoment &at) {
    const auto selected = restore_selection(compact);
    return status([&] {
      auto lease = read(factory, w, selected, at);
      validate_sources(lease.binding, selected, at);
    });
  };
  r.predict = [factory, compact](const WorldBundle &w, const WorldMoment &from,
                                  const WorldMoment &to) {
    const auto selected = restore_selection(compact);
    return guard<std::vector<ExactEventPrediction>>([&] {
      auto lease = read(factory, w, selected, from);
      const auto &x = lease.binding;
      std::vector<ExactEventPrediction> out;
      if (x.office)
        append(out,
               take(predict_exact_office_events(
                   x.common, x.military, *x.action_time, *x.office, from, to)));
      if (x.formation)
        append(out, take(predict_exact_formation_events(
                        x.common, x.military, *x.action_time, *x.formation,
                        from, to)));
      if (x.training)
        append(out, take(predict_exact_military_training_events(
                        x.common, x.military, *x.action_time, *x.training, from,
                        to)));
      if (x.returning)
        append(out, take(predict_exact_return_events(x.common, x.military,
                                                     *x.action_time,
                                                     *x.returning, from, to)));
      if (x.resupply)
        append(out, take(predict_exact_resupply_events(x.common, x.military,
                                                       *x.action_time,
                                                       *x.resupply, from, to)));
      if (x.naval_repair)
        append(out, take(predict_exact_naval_repair_events(
                        x.common, x.military, *x.naval, *x.action_time,
                        *x.naval_repair, from, to)));
      if (selected.dynamic_schema) {
        append(out, take(predict_exact_dynamic_military_events(
                        x.common, x.military, *x.dynamic, *x.dynamic_sources,
                        from, to)));
        append(out, take(predict_military_duty_events(x.dynamic->duty_sources,
                                                      *x.duty, from, to)));
      }
      return out;
    });
  };
  r.provide_interval = [factory, compact](WorldBundle &w,
                                           const MomentSpan &span,
                                           ExactPhaseWriteContext &writes) {
    const auto selected = restore_selection(compact);
    return status([&] {
      auto lease = write(factory, w, selected, span.to, writes);
      auto &x = lease.binding;
      if (x.office)
        take(provide_exact_office_interval(x.common, x.military, *x.action_time,
                                           x.elapsed, *x.office, span));
      if (x.formation)
        take(provide_exact_formation_interval(x.common, x.military,
                                              *x.action_time, x.elapsed,
                                              *x.formation, span));
      if (x.training)
        take(provide_exact_military_training_interval(
            x.common, x.military, *x.action_time, x.elapsed, *x.training, span,
            writes));
      if (x.returning)
        take(provide_exact_return_interval(x.common, x.military, *x.action_time,
                                           x.elapsed, *x.returning, span));
      if (x.resupply)
        take(provide_exact_resupply_interval(x.common, x.military,
                                             *x.action_time, x.elapsed,
                                             *x.resupply, span));
      if (x.naval_repair)
        take(provide_exact_naval_repair_interval(x.common, x.military, *x.naval,
                                                 *x.action_time, x.elapsed,
                                                 *x.naval_repair, span));
      if (selected.dynamic_schema) {
        take(provide_exact_dynamic_military_tactical_interval(
            x.common, *x.dynamic, x.dynamic_sources->actions, span));
        take(provide_military_duty_interval(x.common, x.dynamic->duty_sources,
                                            *x.read.duty, span));
      }
    });
  };
  r.apply_due = [factory, compact](WorldBundle &w,
                                    const ExactEventPrediction &p,
                                    ExactPhaseWriteContext &writes) {
    const auto selected = restore_selection(compact);
    return guard<ExactEventApplied>([&] {
      auto lease = write(factory, w, selected, p.at, writes);
      auto &x = lease.binding;
      const auto &source = p.actual_source_basis;
      if (p.stage == ExactStage::same_moment_hits && selected.dynamic_schema)
        return take(apply_exact_dynamic_military_event(
            x.common, x.military, *x.dynamic, *x.dynamic_sources, p, writes));
      if (p.stage == ExactStage::military_duty_period &&
          selected.dynamic_schema)
        return take(apply_military_duty_event(x.common, x.dynamic->duty_sources,
                                              *x.read.duty, p, writes));
      if (source.domain == "military_naval" &&
          source.kind == "naval_repair_start" && x.naval_repair)
        return take(apply_exact_naval_repair_event(
            x.common, x.military, *x.naval, *x.action_time, x.elapsed,
            *x.naval_repair, p, writes));
      if (source.domain == "military" && source.kind == "military_training" &&
          x.training)
        return take(apply_exact_military_training_event(
            x.common, x.military, *x.action_time, x.elapsed, *x.training, p,
            writes));
      if (source.domain == "military" && source.kind == "military_action")
        return take(apply_fixed(x, p, writes, selected.field_original));
      fail("UnsupportedMilitaryExactDueSource", source.id);
      return ExactEventApplied{};
    });
  };
  r.validate_applied_source = [factory, compact](const WorldBundle &w,
                                         const ExactEventApplied &event) {
    const auto selected = restore_selection(compact);
    return status([&] {
      const auto at = take(w.actual_phase_moment());
      auto lease = read(factory, w, selected, at);
      const auto &x = lease.binding;
      const auto &source = event.applied_result;
      if (source.domain == "military_naval" && x.naval_repair) {
        take(validate_exact_naval_repair_applied_source(
            x.common, x.military, *x.naval, *x.action_time, x.elapsed,
            *x.naval_repair, event));
        return;
      }
      if (source.domain != "military")
        fail("UnsupportedMilitaryExactAppliedSource", source.id);
      if (source.kind == "military_action_completed" ||
          source.kind == "military_logistics_completed") {
        take(validate_fixed(x, event));
        return;
      }
      if ((source.kind == "military_training_lesson" ||
           source.kind == "military_training_completed") &&
          x.training) {
        take(validate_exact_military_training_applied_source(
            x.common, x.military, *x.action_time, x.elapsed, *x.training,
            event));
        return;
      }
      if (selected.dynamic_schema &&
          (source.kind == "military_duty_period" ||
           source.kind == "military_station_terminal" ||
           source.kind == "military_duty_execution_stopped")) {
        take(validate_military_duty_applied_source(
            x.common, x.dynamic->duty_sources, *x.duty, event));
        return;
      }
      if (!selected.dynamic_schema)
        fail("UnsupportedMilitaryExactAppliedSource", source.id);
      take(validate_exact_dynamic_military_applied_source(
          x.common, x.military, *x.dynamic, *x.dynamic_sources, event));
    });
  };
  if (selected.field_original) {
    r.period_source_owner = true;
    r.period_closure_kind = ExactPeriodClosureKind::stage_barrier;
    r.opening_after = {ExactProviderId::local_economy};
    r.closing_after = {ExactProviderId::local_economy};
    r.effect_writer_keys.push_back("military.original_field_opening_supply");
    r.effect_writer_keys.push_back("travel.original_military_supply_rebind");
    // 借经济已落的唯一 Opening，不创建第二军旅期间或新成果头。
    r.open_period = [factory, compact](WorldBundle &w,
                                        const ExactPeriodContext &p,
                                        ExactPhaseWriteContext &writes) {
      const auto selected = restore_selection(compact);
      return guard<ExactPeriodOwnerSources>([&] {
        // 先真实捕获供养前值并释放读域；原供养写完，再接同 M 余段的新映射。
        const auto prepared=take(prepare_native_field_supply_movements(w,p));
        SourceRef opening;
        {
          auto lease = write(factory, w, selected, p.opening_at, writes);
          auto &x = lease.binding;
          if (!x.read.field_supply)fail("FieldMilitarySupplyOriginalSourcesRequired");
          take(open_field_military_supply_in_candidate(x.common,x.military,x.read.catalog,
              x.read.access,*x.read.field_supply,p,prepared));
          opening=take(x.read.field_supply->periods.read_training_period(
              p.opening_at.half_day_tick)).original_opening.source_ref;
        }
        take(apply_native_field_supply_movements(w,p,prepared));
        return ExactPeriodOwnerSources{opening,1,{},{}};
      });
    };
    r.close_period = [factory, compact](WorldBundle &w,
                                         const ExactPeriodContext &p,
                                         ExactPhaseWriteContext &) {
      const auto selected = restore_selection(compact);
      return guard<ExactPeriodOwnerSources>([&] {
        const auto at = take(w.actual_phase_moment());
        if (!same_moment(at, WorldMoment{p.through_tick, {}}))
          fail("FieldMilitarySupplyCloseBoundaryRequired");
        auto lease = read(factory, w, selected, at);
        const auto &x = lease.binding;
        const auto &sources = *x.field_supply;
        const auto original = take(
            sources.periods.read_training_period(p.opening_at.half_day_tick));
        if (original.period_ref != p.period_id ||
            original.to_tick != p.through_tick ||
            !same_moment(original.opening_at, p.opening_at, x.budget))
          fail("FieldMilitarySupplyActualOpeningRequired");
        take(read_field_military_supply_opening_sources(
            x.common, x.military, x.catalog, x.access, sources.periods,
            sources.formations, original.from_tick, at, x.budget,
            sources.returns));
        return ExactPeriodOwnerSources{
            original.original_opening.source_ref, 1, {}, {}};
      });
    };
    r.read_period_sources = [factory, compact](const WorldBundle &w,
                                                const ExactPeriodContext &p) {
      const auto selected = restore_selection(compact);
      return guard<ExactPeriodOwnerSources>([&] {
        const auto at = take(w.actual_phase_moment());
        auto lease = read(factory, w, selected, at);
        const auto &x = lease.binding;
        const auto &sources = *x.field_supply;
        const auto original = take(
            sources.periods.read_training_period(p.opening_at.half_day_tick));
        if (original.period_ref != p.period_id ||
            original.to_tick != p.through_tick ||
            !same_moment(original.opening_at, p.opening_at, x.budget))
          fail("FieldMilitarySupplyActualOpeningRequired");
        take(read_field_military_supply_opening_sources(
            x.common, x.military, x.catalog, x.access, sources.periods,
            sources.formations, original.from_tick, at, x.budget,
            sources.returns));
        return ExactPeriodOwnerSources{
            original.original_opening.source_ref, 1, {}, {}};
      });
    };
  }
  r.apply_calendar_stage = [factory, compact](WorldBundle &w, ExactStage stage,
                                               const ExactPeriodContext &period,
                                               ExactPhaseWriteContext &writes) {
    const auto selected = restore_selection(compact);
    return status([&] {
      const auto at = take(w.actual_phase_moment());
      auto lease = write(factory, w, selected, at, writes);
      auto &x = lease.binding;
      auto *candidate = take(
          acquire_military_calendar_candidate(writes, period, x.read.budget));
      switch (stage) {
      case ExactStage::military_duty_period:
        field_unselected_duty_barrier(w, x, selected, period, writes);
        break;
      case ExactStage::military_siege_period:
        take(apply_military_siege_period_in_candidate(
            x.calendar, *candidate, period, writes, x.read.budget));
        break;
      case ExactStage::military_siege_battles_finished:
        take(apply_military_siege_battles_finished_in_candidate(
            x.calendar, *candidate, period, writes, x.read.budget));
        break;
      case ExactStage::siege_results:
        take(apply_military_siege_results_in_candidate(
            x.calendar, *candidate, period, writes, x.read.budget));
        break;
      case ExactStage::campaign_adoption:
        take(apply_military_campaign_adoption_in_candidate(
            x.calendar, *candidate, period, writes, x.read.budget));
        break;
      default:
        fail("UnsupportedMilitaryExactCalendarStage");
      }
    });
  };
  return r;
}
} // namespace hanmo::resident
