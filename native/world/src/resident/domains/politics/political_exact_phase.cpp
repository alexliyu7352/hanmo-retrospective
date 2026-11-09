#include "political_exact_phase.hpp"
#include "resident/domains/organization/membership_offer.hpp"
#include "resident/domains/organization/named_membership_rules.hpp"
#include "resident/domains/organization/named_membership_validation.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> x) {
  if (!x.ok)
    throw Failure{std::move(x.error)};
  return std::move(x.value);
}
void take(Status x) {
  if (!x.ok)
    throw Failure{std::move(x.error)};
}
template <class F> Status guard(F &&f) {
  try {
    f();
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
void equal(const WorldMoment &a, const WorldMoment &b) {
  if (take(compare_world_moments(a, b)))
    fail("PoliticalExactClockMismatch");
}
// 三个后置轴没有原 World 根；这里只验证实际未选，不创建空业务表或成果。
// 原 Takeover 的 integration_ticks 不等于 occupation_integration 新能力。
bool deferred_tail_module(std::string_view name) {
  return name == "political_continuity" || name == "occupation_integration" ||
         name == "political_achievement_scope";
}
bool deferred_tail_kind(std::string_view kind) {
  // 仅列原 continuity 已存在的两种事实名，不猜尚无 producer 的事实名。
  return kind == "foundation_intent_created" ||
         kind == "foundation_support_decided";
}
void tail_kind(std::string_view kind, const StableId &id) {
  if (deferred_tail_kind(kind))
    fail("UnselectedPoliticalTailRetainedSource", id);
}
void tail_source(const SourceRef &source) {
  if (deferred_tail_module(source.domain) || deferred_tail_kind(source.kind))
    fail("UnselectedPoliticalTailRetainedSource", source.id);
}
void tail_sources(const std::vector<SourceRef> &sources) {
  for (const auto &source : sources)
    tail_source(source);
}
void tail_optional(const std::optional<SourceRef> &source) {
  if (source)
    tail_source(*source);
}
void tail_exile(OrganizationStatus status, const StableId &id) {
  if (status == OrganizationStatus::exile)
    fail("UnselectedPoliticalExileState", id);
}
void tail_terminal(const std::optional<OrganizationTerminalReason> &reason,
                   const StableId &id) {
  if (reason == OrganizationTerminalReason::continuity_exhausted)
    fail("UnselectedPoliticalExileState", id);
}
void tail_work_binding(const WorkExecutionBinding &binding) {
  if (const auto *actor = std::get_if<ActorActionWorkBinding>(&binding)) {
    tail_kind(actor->action_kind, actor->action_id);
    tail_source(actor->source_owner_ref);
  }
}
void validate_unselected_political_tails(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->organization ||
      !w.catalog->organization_lifecycle || !w.organization_runtime)
    fail("PoliticalTailOriginalWorldRequired");
  for (const auto &module : w.catalog->identity.module_versions)
    if (deferred_tail_module(module.first))
      throw Failure{common_failure("NOT_IMPLEMENTED", module.first,
                                    ErrorKind::unsupported_capability).error};
  const auto &common = w.common.state();
  for (const auto &row : common.facts.rows())
    if (deferred_tail_module(row.second.domain) ||
        deferred_tail_kind(row.second.kind))
      fail("UnselectedPoliticalTailFact", row.first);
  for (const auto &row : common.actions.rows())
    tail_kind(row.second.kind, row.first);
  for (const auto &row : common.history)
    tail_kind(row.kind, row.result_id);
  for (const auto &row : common.results.rows())
    if (row.second)
      tail_kind(row.second->kind, row.first);
  // 作者及运行原 lifecycle 同时反扫，不能把空 continuity 索引当无流亡证明。
  for (const auto &row : w.catalog->organization->organizations.rows())
    tail_exile(row.second.status, row.first);
  for (const auto &row : w.organization.organizations.rows())
    tail_exile(row.second.status, row.first);
  for (const auto &row : w.catalog->organization_lifecycle->seed_policies.rows()) {
    const auto &v = row.second;
    tail_exile(v.initial_lifecycle, row.first);
    tail_terminal(v.terminal_reason, row.first);
    tail_optional(v.base_right_ref);
    tail_sources(v.mobile_carrier_refs);
    tail_sources(v.initial_responsibility_refs);
  }
  const auto &runtime = *w.organization_runtime;
  for (const auto &row : runtime.organization_metadata.rows()) {
    tail_source(row.second.origin_ref);
    tail_optional(row.second.last_lifecycle_transition_ref);
  }
  for (const auto &row : runtime.lifecycle_origins.rows())
    std::visit([&](const auto &v) {
      using T = std::decay_t<decltype(v)>;
      if constexpr (std::is_same_v<T, OrganizationFoundedOrigin>) {
        tail_source(v.founding_decision_ref);
        tail_source(v.registration_result_ref);
      } else if constexpr (std::is_same_v<T, OrganizationMigratedOrigin>) {
        if (v.original_status == "exile")
          fail("UnselectedPoliticalExileState", row.first);
        tail_source(v.migration_decision_ref);
      }
    }, row.second.origin);
  for (const auto &row : runtime.lifecycle_migration_decisions.rows()) {
    const auto &v = row.second;
    tail_exile(v.selected_lifecycle, row.first);
    tail_terminal(v.selected_terminal_reason, row.first);
    if (v.original_status == "exile")
      fail("UnselectedPoliticalExileState", row.first);
    tail_sources(v.evidence_refs);
  }
  for (const auto &row : runtime.lifecycle_transitions.rows()) {
    const auto &v = row.second;
    if (v.before_status)
      tail_exile(*v.before_status, row.first);
    tail_exile(v.after_status, row.first);
    tail_terminal(v.terminal_reason, row.first);
    if (v.cause_kind == OrganizationLifecycleCause::exile_continuity_applied ||
        v.cause_kind == OrganizationLifecycleCause::restoration_applied ||
        v.cause_kind == OrganizationLifecycleCause::continuity_failed)
      fail("UnselectedPoliticalExileState", row.first);
    tail_source(v.cause_ref);
    tail_optional(v.outcome_ref);
  }
  for (const auto &row : runtime.lifecycle_outcomes.rows()) {
    const auto &v = row.second;
    if (v.status == OrganizationLifecycleOutcomeKind::exile)
      fail("UnselectedPoliticalExileState", row.first);
    tail_source(v.cause_ref);
    tail_sources(v.member_transition_refs);
    tail_sources(v.control_transition_refs);
    tail_sources(v.asset_receipts);
    tail_sources(v.responsibility_dispositions);
  }
  for (const auto &row : runtime.terminal_dispositions.rows()) {
    tail_source(row.second.lifecycle_outcome_ref);
    tail_sources(row.second.responsibility_disposition_refs);
  }
  for (const auto &row : runtime.office_transition_sources.rows())
    tail_source(row.second.source);
  for (const auto &row : runtime.grant_versions.rows())
    tail_source(row.second.change_source);
  for (const auto &row : runtime.relations.rows())
    tail_source(row.second.source_ref);
  for (const auto &row : runtime.authority_bases.rows()) {
    tail_source(row.second.issued_source);
    tail_kind(row.second.command_kind, row.first);
    if (row.second.budget_ref)
      tail_source(row.second.budget_ref->approval_source);
  }
  for (const auto &row : w.organization.facts.rows()) {
    tail_kind(row.second.kind, row.first);
    if (row.second.office_change_payload)
      if (const auto *v = std::get_if<ByOfficeTransition>(
              &row.second.office_change_payload->cause))
        tail_source(v->transition_source_ref);
  }
  for (const auto &row : w.politics.facts.rows())
    tail_kind(row.second.kind, row.first);
  // 借扫原保留来源，不调用会深复制来源集合的便利查询。
  for (const auto &row : w.social.events.rows()) {
    tail_kind(row.second.kind, row.first);
    if (row.second.original_application)
      tail_source(row.second.original_application->original_period_opening.source_ref);
  }
  if (w.social.dynamic) {
    const auto &social = *w.social.dynamic;
    for (const auto &row : social.reports.rows()) {
      tail_kind(row.second.payload_kind, row.first);
      tail_kind(row.second.observation.kind, row.first);
    }
    for (const auto &row : social.pending_messages.rows())
      if (row.second.address_binding)
        tail_source(row.second.address_binding->original_business_fact_ref);
    if (social.time2) {
      const auto &time = *social.time2;
      for (const auto &row : time.source_time_bindings.rows()) {
        tail_source(row.second.actual_source_ref);
        tail_optional(row.second.learned_after_common_fact);
        if (const auto *v = std::get_if<SocialExactSource>(&row.second.time_source))
          tail_source(v->original_timed_source_ref.source_ref);
      }
      for (const auto &row : time.message_execution_bindings.rows()) {
        if (const auto *v = std::get_if<SocialS3BusinessLink>(&row.second.source_binding))
          tail_source(v->original_business_fact_ref);
        tail_optional(row.second.actual_delivery_ref);
        tail_optional(row.second.actual_dossier_access_ref);
      }
      for (const auto &row : time.propagation_decisions.rows())
        tail_sources(row.second.causal_source_refs);
      for (const auto &row : time.propagation_cooldowns.rows())
        tail_source(row.second.source_decision_ref);
    }
  }
  if (w.interaction.communication_links) {
    const auto &links = *w.interaction.communication_links;
    for (const auto &row : links.links.rows()) {
      const auto &v = row.second;
      tail_source(v.original_business_fact_ref);
      tail_sources(v.route_source_refs);
      tail_sources(v.dossier_source_refs);
      if (v.cancellation_source)
        tail_source(v.cancellation_source->source_ref);
      if (v.official_content_ref)
        if (const auto *terms = std::get_if<SourceFactTerms>(
                &v.official_content_ref->terms_revision))
          tail_source(terms->source);
    }
    for (const auto &row : links.deliveries.rows()) {
      tail_source(row.second.arrival_ref);
      tail_source(row.second.authority_source);
    }
    for (const auto &row : links.dossier_accesses.rows()) {
      tail_source(row.second.original_access_ref);
      tail_source(row.second.authority_source);
    }
  }
  if (w.interaction.response_sources) {
    const auto &responses = *w.interaction.response_sources;
    for (const auto &row : responses.session_boundaries.rows()) {
      tail_optional(row.second.current_response_fact_ref);
      tail_optional(row.second.last_consumed_response_fact_ref);
    }
    for (const auto &row : responses.response_sources.rows()) {
      tail_source(row.second.committed_result_ref);
      tail_optional(row.second.tail_consumed_result_ref);
    }
    for (const auto &row : responses.selected_response_tails.rows())
      tail_source(row.second.response_fact_ref);
  }
  if (w.action_elapsed) {
    const auto &elapsed = *w.action_elapsed;
    for (const auto &v : elapsed.terminal_bindings) {
      tail_kind(v.action_kind, v.action_ref);
      tail_source(v.original_execution_source_ref);
      std::visit([](const auto &s) { tail_source(s.source_ref); }, v.started_source_ref);
      std::visit([](const auto &s) { tail_source(s.source_ref); }, v.ended_source_ref);
    }
    for (const auto &v : elapsed.fixed_service_transitions) {
      tail_source(v.execution_source_ref);
      std::visit([](const auto &cause) {
        using T = std::decay_t<decltype(cause)>;
        if constexpr (std::is_same_v<T, FixedOwnerAdopted> ||
                      std::is_same_v<T, FixedOwnerStateChanged>)
          tail_source(cause.owner_source_ref);
        else if constexpr (std::is_same_v<T, FixedEligibilityChanged>)
          tail_source(cause.original_cause_ref);
        else
          tail_source(cause.owner_result_ref);
      }, v.cause);
      for (const auto *inputs : {&v.input_basis_refs, &v.previous_input_basis_refs})
        for (const auto &input : *inputs)
          if (const auto *source = std::get_if<FixedAppliedInput>(&input))
            tail_source(source->source_ref);
    }
    for (const auto &entry : elapsed.entries)
      if (const auto *shared = std::get_if<SharedWorkElapsedState>(&entry)) {
        if (shared->execution.last_processed_period)
          tail_work_binding(shared->execution.last_processed_period->binding);
        std::visit([](const auto &gate) {
          using T = std::decay_t<decltype(gate)>;
          if constexpr (std::is_same_v<T, CraftWorkGate>)
            tail_work_binding(gate.execution_binding);
          else if constexpr (std::is_same_v<T, OriginalHalfDayGate>) {
            tail_work_binding(gate.original_execution_binding);
            if (gate.last_consumed_period_ref)
              tail_work_binding(gate.last_consumed_period_ref->binding);
          } else
            tail_source(gate.stage_rule_source_ref);
        }, shared->gate);
      }
    for (const auto &gate : elapsed.gate_results)
      std::visit([](const auto &v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, LifeHalfDayStageGateResult>) {
          tail_source(v.work_period_ref.source_ref);
          tail_sources(v.original_stage_result_refs);
        } else {
          tail_work_binding(v.execution_binding);
          if constexpr (std::is_same_v<T, OriginalHalfDayGateResult>) {
            tail_work_binding(v.period_source_ref.binding);
            tail_sources(v.stage_result_refs);
          }
        }
      }, gate);
  }
}
// 同时覆盖成功、失败及异常；下一真实consumer必须重新发行phase-current读票。
struct WriterBoundary {
  WorldBundle &world;
  explicit WriterBoundary(WorldBundle &w) : world(w) {}
  ~WriterBoundary() { world.note_write_boundary(); }
  WriterBoundary(const WriterBoundary &) = delete;
  WriterBoundary &operator=(const WriterBoundary &) = delete;
};
void read_binding(const WorldBundle &w, const PoliticalExactReadBinding &b,
                  const WorldMoment &at) {
  const auto &o = b.organization;
  const auto &m = o.market;
  if (&m.common != &w.common || &m.organization != &w.organization ||
      &m.economy != &w.local_economy || &b.politics != &w.politics ||
      &b.opening.common != &m.common ||
      &b.opening.organization != &m.organization ||
      &b.opening.economy != &m.economy || &o.runtime != &m.runtime ||
      !w.organization_runtime || &o.runtime != &*w.organization_runtime ||
      !b.qualifications || !m.catalog.enabled ||
      !m.catalog.authority_lifecycle_enabled ||
      !m.catalog.exact_fact_clock_enabled || !b.politics_catalog.enabled)
    fail("PoliticalExactReadBindingMismatch");
  if (!o.staffing &&
      (!w.catalog || !o.household_catalog || !o.retail_catalog ||
       o.household_catalog != w.catalog->household_purchases.get() ||
       o.retail_catalog != w.catalog->household_retail_settlements.get()))
    fail("PoliticalExactReadBindingMismatch");
  equal(at, take(w.actual_phase_moment()));
  equal(at, take(o.clock.read_organization_authority_now()));
  equal(at, take(m.clock.read_organization_authority_now()));
  equal(at, take(b.opening.clock.read_current_moment()));
}
void write_binding(WorldBundle &w, const PoliticalExactWriteBinding &b,
                   const WorldMoment &at) {
  const auto &c = b.organization;
  const auto &o = c.organization;
  if (&o.common != &w.common || &o.organization != &w.organization ||
      &o.economy != &w.local_economy || &b.politics != &w.politics ||
      &o.runtime != &c.original_runtime.runtime ||
      &o.runtime != &b.politics_runtime.runtime || !w.organization_runtime ||
      &o.runtime != &*w.organization_runtime ||
      !b.politics_runtime.qualifications || &c.opening.common != &o.common ||
      &c.opening.organization != &o.organization ||
      &c.opening.economy != &o.economy || !o.catalog.enabled ||
      !o.catalog.authority_lifecycle_enabled ||
      !o.catalog.exact_fact_clock_enabled || !b.politics_catalog.enabled ||
      !c.original_runtime.calendar)
    fail("PoliticalExactWriteBindingMismatch");
  if (bool(c.staffing) != bool(c.staffing_effects) ||
      (!c.staffing &&
       (!w.catalog || !c.household_catalog || !c.retail_catalog ||
        c.household_catalog != w.catalog->household_purchases.get() ||
        c.retail_catalog != w.catalog->household_retail_settlements.get())))
    fail("PoliticalExactWriteBindingMismatch");
  equal(at, take(w.actual_phase_moment()));
  equal(at, take(o.clock.read_organization_authority_now()));
  equal(at, take(c.original_runtime.clock.read_organization_authority_now()));
  equal(at, take(b.politics_runtime.clock.read_organization_authority_now()));
  equal(at, take(c.opening.clock.read_current_moment()));
}
std::shared_ptr<const PoliticalExactReadLease>
read(PoliticalExactWorldFactory f, const WorldBundle &w,
     const WorldMoment &at) {
  if (!f.read || !f.write)
    fail("PoliticalExactWorldFactoryRequired");
  auto owner = take(f.read(w, at));
  if (!owner)
    fail("PoliticalExactReadLeaseRequired");
  read_binding(w, owner->bindings(), at);
  return owner;
}
std::shared_ptr<PoliticalExactWriteLease>
write(PoliticalExactWorldFactory f, WorldBundle &w, const WorldMoment &at) {
  if (!f.read || !f.write)
    fail("PoliticalExactWorldFactoryRequired");
  auto owner = take(f.write(w, at));
  if (!owner)
    fail("PoliticalExactWriteLeaseRequired");
  write_binding(w, owner->bindings(), at);
  return owner;
}
void validate(const PoliticalExactReadBinding &b) {
  const auto &o = b.organization;
  const auto &m = o.market;
  take(validate_complete_organization(
      m.common, m.organization, m.catalog, m.economy, &b.results, &b.activities,
      b.resource_history, b.prospective_grants, o));
  take(validate_named_membership(m.common, m.organization, m.catalog, m.economy,
                                 b.economy_catalog, &o.clock));
  take(validate_politics(m.common, b.common_catalog, b.politics,
                         b.politics_catalog, m.organization, m.catalog,
                         b.territories, &o.clock));
  take(validate_politics_time2(m.common, b.politics, o.runtime, o.clock));
  take(validate_politics_time2_authorities(m.common, b.politics, m.organization,
                                           m.catalog, o.runtime, o.clock));
}
} // namespace
Status apply_original_political_calendar_in_candidate(
    WorldBundle &w, const ExactPeriodContext &p, PoliticalExactWorldFactory f) {
  return guard([&] {
    if (p.period_id.empty() ||
        p.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
        p.through_tick != p.opening_at.half_day_tick + 1 ||
        take(compare_fractions(p.opening_at.sub_half_day, CanonicalFraction{})))
      fail("PoliticalCalendarPeriodMismatch", p.period_id);
    const WorldMoment at{p.through_tick, {}};
    equal(at, take(w.actual_phase_moment()));
    std::vector<StableId> activities;
    {
      auto owner = read(f, w, at);
      auto input =
          take(read_economic_calendar_input(owner->bindings().opening, p));
      activities = std::move(input.existing_activity_ids);
    }
    // 当期开后新增活动不会进入冻结集合。每次原writer后的下一次读均重绑本候选。
    for (const auto &id : activities) {
      auto owner = write(f, w, at);
      auto b = owner->bindings();
      WriterBoundary boundary(w);
      take(step_organization_calendar_activity_in_candidate(b.organization, p,
                                                            id));
    }
    {
      auto owner = write(f, w, at);
      auto b = owner->bindings();
      auto &c = b.organization;
      auto &o = c.organization;
      WriterBoundary boundary(w);
      take(reconcile_organization(o.common, o.organization, o.catalog,
                                  o.economy, o.activity, p.through_tick,
                                  &c.original_runtime));
    }
    bool named = false;
    {
      auto owner = read(f, w, at);
      named = owner->bindings()
                  .organization.market.catalog.named_membership_enabled;
    }
    if (named) {
      {
        auto owner = write(f, w, at);
        auto b = owner->bindings();
        auto &o = b.organization.organization;
        WriterBoundary boundary(w);
        take(expire_membership_offers(o.common, o.organization, o.catalog,
                                      p.through_tick, &o.clock));
      }
      {
        auto owner = write(f, w, at);
        auto b = owner->bindings();
        auto &c = b.organization;
        auto &o = c.organization;
        MembershipOfficeTransitionWriteContext transitions{
            o.runtime.office_transition_sources, c.original_runtime.clock,
            c.original_runtime.stamp};
        WriterBoundary boundary(w);
        take(reconcile_named_membership(o.common, o.organization, o.catalog,
                                        o.economy, b.economy_catalog,
                                        o.activity, c.original_runtime.releases,
                                        p.through_tick, &transitions));
      }
    }
    {
      auto owner = write(f, w, at);
      auto b = owner->bindings();
      auto &o = b.organization.organization;
      WriterBoundary boundary(w);
      take(reconcile_politics(o.common, b.politics, b.politics_catalog,
                              o.organization, o.catalog, o.economy,
                              &b.politics_runtime));
    }
  });
}
Outcome<std::shared_ptr<const PoliticalExactReadLease>>
bind_original_political_exact_read(const WorldBundle &w, const WorldMoment &at,
                                   PoliticalExactWorldFactory factory) {
  try {
    return {true, read(factory, w, at), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Outcome<std::shared_ptr<PoliticalExactWriteLease>>
bind_original_political_exact_write(WorldBundle &w, const WorldMoment &at,
                                    PoliticalExactWorldFactory factory) {
  try {
    return {true, write(factory, w, at), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Status validate_original_political_exact_author_selection(
    const WorldBundle &w, PoliticalExactWorldFactory factory) {
  return guard([&] {
    validate_unselected_political_tails(w);
    auto at = take(w.actual_phase_moment());
    auto owner = read(factory, w, at);
    const auto b = owner->bindings();
    if (!b.opening.sources.calendar_origin)
      fail("PoliticalOriginalEconomicCalendarRequired");
  });
}
Status validate_original_political_exact_world_sources(
    const WorldBundle &w, const WorldMoment &at,
    PoliticalExactWorldFactory factory) {
  return guard([&] {
    validate_unselected_political_tails(w);
    auto owner = read(factory, w, at);
    validate(owner->bindings());
  });
}
ExactPhaseRegistration
make_political_exact_phase_registration(PoliticalExactWorldFactory factory) {
  ExactPhaseRegistration r;
  r.provider = ExactProviderId::politics;
  r.profile_id = "organization_politics_original_calendar";
  r.version = 1;
  r.schedule_kind = ExactScheduleKind::boundary_only;
  r.stages = {ExactStage::political_due_close, ExactStage::exile_review,
              ExactStage::occupancy_evaluation, ExactStage::achievement};
  r.calendar_stages = r.stages;
  r.effect_writer_keys = {"organization/original_calendar_activities",
                          "organization/original_membership_reconcile",
                          "organization/membership_offer_expiry",
                          "politics/appointment_reconcile"};
  r.causal_edges = {
      {ExactStage::economic_supply_affairs, ExactStage::political_due_close},
      {ExactStage::due_message_receipt, ExactStage::political_due_close},
      {ExactStage::political_due_close, ExactStage::knowledge_reaction_close},
      {ExactStage::exile_review, ExactStage::occupancy_evaluation},
      {ExactStage::occupancy_evaluation, ExactStage::achievement}};
  r.validate_author_selection = [factory](const WorldBundle &w) {
    return validate_original_political_exact_author_selection(w, factory);
  };
  r.validate_world_sources = [factory](const WorldBundle &w,
                                       const WorldMoment &at) {
    return validate_original_political_exact_world_sources(w, at, factory);
  };
  r.apply_calendar_stage = [factory](WorldBundle &w, ExactStage stage,
                                     const ExactPeriodContext &p,
                                     ExactPhaseWriteContext &context) {
    if (stage == ExactStage::political_due_close)
      return apply_original_political_calendar_in_candidate(w, p, factory);
    if (stage != ExactStage::exile_review &&
        stage != ExactStage::occupancy_evaluation &&
        stage != ExactStage::achievement)
      return common_failure("PoliticalCalendarStageMismatch");
    return guard([&] {
      if (w.active_exact_write_context() != &context || !w.world_timeline ||
          w.world_timeline->boundary_state != PeriodBoundaryState::open ||
          w.world_timeline->period_id != p.period_id ||
          p.opening_at.half_day_tick != w.common.tick())
        fail("PoliticalTailActualCalendarPeriodRequired", p.period_id);
      if (p.period_id.empty() || p.opening_at.half_day_tick < 0 ||
          p.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
          p.through_tick != p.opening_at.half_day_tick + 1 ||
          take(compare_fractions(p.opening_at.sub_half_day, CanonicalFraction{})))
        fail("PoliticalCalendarPeriodMismatch", p.period_id);
      const WorldMoment at{p.through_tick, {}};
      auto owner = read(factory, w, at);
      // 只验证实际未选；不调用写者、不发成果，也不把本次结论缓存到后续切点。
      validate_unselected_political_tails(w);
    });
  };
  return r;
}
} // namespace hanmo::resident
