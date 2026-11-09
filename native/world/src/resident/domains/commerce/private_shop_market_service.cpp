#include "private_shop_market_service.hpp"
#include "private_shop_capability.hpp"
#include "private_shop_staff.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/fixed_service_history.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
void take(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
bool rule(const ActorFoundationRuleRef &a, const ActorFoundationRuleRef &b) {
  return a.id == b.id && a.version == b.version &&
         a.catalog_digest == b.catalog_digest;
}
SourceRef cause(const MarketServiceChangeCause &c) {
  return std::visit(
      [](const auto &v) -> SourceRef {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, MarketServiceInitialized>)
          return v.original_opening_acceptance_ref;
        else if constexpr (std::is_same_v<T, MarketServiceOpeningChanged>)
          return v.original_opening_change_ref;
        else if constexpr (std::is_same_v<T, MarketServiceStaffingChanged>)
          return v.original_staffing_event_ref;
        else if constexpr (std::is_same_v<T, MarketServiceFacilityChanged>)
          return v.original_condition_result_ref;
        else
          return v.original_right_change_ref;
      },
      c);
}
bool condition_source(const SourceRef &s) {
  return s.domain == "actor_foundation" && s.kind == "condition_receipt";
}
std::size_t position(const PrivateShopOperationSourceContext &x,
                     const SourceRef &s) {
  return take(read_actual_common_fact_position(x.shops.common, s));
}
const FixedServiceTransition &
actual_body_transition(const PrivateShopOperationSourceContext &x,
                       const SourceRef &s) {
  const FixedServiceTransition *out = nullptr;
  for (const auto &t : x.elapsed.fixed_service_transitions)
    if (const auto *c = std::get_if<FixedEligibilityChanged>(&t.cause))
      if (c->original_cause_schema == 2 &&
          same_source_ref(c->original_cause_ref, s) &&
          t.execution_source_schema == 2 &&
          same_source_ref(
              t.execution_source_ref,
              private_shop_operation_started_source(t.action_ref))) {
        if (out)
          fail("AmbiguousPrivateMarketBodyTransition", s.id);
        out = &t;
      }
  if (!out || out->transition_id != s.id ||
      (out->kind != FixedTransitionKind::suspended &&
       out->kind != FixedTransitionKind::resumed &&
       out->kind != FixedTransitionKind::input_rebound))
    fail("PrivateMarketActualBodyTransitionRequired", s.id);
  const auto source = take(read_character_condition_source(x.body, s, 2));
  const auto *o = x.shops.state.operations.find(out->action_ref);
  if (!o || source.actor_id != o->actor_id ||
      cmp(source.occurred_at, out->occurred_at))
    fail("PrivateMarketBodyTransitionMismatch", s.id);
  NativePrivateShopActionTimeRead owner(x);
  const auto &r = private_shop_operation_action_time_registration();
  auto proof = take(owner.read_action_time_owner(r, o->id));
  const auto *entry = find_fixed_elapsed(x.elapsed, o->id);
  if (!entry)
    fail("PrivateMarketOriginalElapsedRequired", o->id);
  std::vector<FixedServiceTransition> history;
  for (const auto &t : x.elapsed.fixed_service_transitions)
    if (t.action_ref == o->id)
      history.push_back(t);
  take(owner.validate_fixed_action_sources(r, proof, *entry, history));
  return *out;
}
const PrivateShopOperationAcceptance &
accepted(const PrivateShopOperationSourceContext &x, const StableId &id) {
  return *take(read_original_private_shop_operation_acceptance(
      x, {private_shop_operation_started_source(id), 2}));
}
const PrivateShopOperationAcceptance &
adoption(const PrivateShopOperationSourceContext &x,
         const MarketServiceDefinition &d, const SourceRef &s) {
  if (d.slot_ref.provider_domain != "commerce" ||
      d.slot_ref.slot_kind != "market_service")
    fail("PrivateMarketDefinitionRequired", d.definition_ref.id);
  if (condition_source(s)) {
    const auto &t = actual_body_transition(x, s);
    const auto &a = accepted(x, t.action_ref);
    if (!rule(a.market_definition_ref, d.definition_ref))
      fail("PrivateMarketBodySlotMismatch", s.id);
    return a;
  }
  const auto cut = position(x, s);
  const PrivateShopOperationAcceptance *out = nullptr;
  std::size_t last = 0;
  for (const auto &a : x.sources.accepted_operations)
    if (rule(a.market_definition_ref, d.definition_ref)) {
      const auto &v = accepted(x, a.operation_ref);
      const auto at =
          position(x, private_shop_operation_started_source(a.operation_ref));
      if (at <= cut && (!out || at > last)) {
        out = &v;
        last = at;
      }
    }
  if (!out)
    fail("PrivateMarketActualOpeningAcceptanceRequired", s.id);
  return *out;
}
} // namespace
Status
NativePrivateMarketServiceOriginalRead::validate_capacity_source_coverage(
    const HouseholdPurchaseState &purchases) const {
  try {
    take(validate_market_service_condition_selection(identity_, x_.shops.common,
                                                     conditions_));
    std::vector<StableId> matched;
    auto adopted =
        [&](const PrivateShopOperationAcceptance &a,
            const SourceRef &source) -> const MarketServiceCapacitySource & {
      const auto actual = take(read_private_market_change_source(source));
      const MarketServiceCapacitySource *found = nullptr;
      for (const auto &row : purchases.market_service_sources.private_sources)
        if (rule(row.definition_ref, a.market_definition_ref) &&
            same_source_ref(cause(row.change_cause), source)) {
          if (found)
            fail("DuplicatePrivateMarketActualCapacityCause", source.id);
          found = &row;
        }
      if (!found)
        fail("PrivateMarketActualCapacityChangeMissing", source.id);
      const auto *definition =
          static_cast<const MarketServiceDefinition *>(nullptr);
      for (const auto &d : x_.markets.market_service_definitions)
        if (rule(d.definition_ref, a.market_definition_ref)) {
          if (definition)
            fail("DuplicatePrivateMarketDefinition", d.definition_ref.id);
          definition = &d;
        }
      if (!definition || found->slot_ref != definition->slot_ref ||
          cmp(found->occurred_at,
              take(read_private_market_change_moment(source))))
        fail("PrivateMarketCapacitySourceMismatch", source.id);
      const auto expected =
          take(read_private_market_service_change(*definition, actual));
      if (expected.index() != found->change_cause.index())
        fail("PrivateMarketCapacityCauseMismatch", source.id);
      if (std::find(matched.begin(), matched.end(), found->source_id) ==
          matched.end())
        matched.push_back(found->source_id);
      return *found;
    };
    for (const auto &a : x_.sources.accepted_operations) {
      accepted(x_, a.operation_ref);
      const auto &start =
          adopted(a, private_shop_operation_started_source(a.operation_ref));
      std::size_t previous =
          position(x_, {start.slot_ref.provider_domain,
                        "market_service_capacity", start.source_id});
      std::vector<const FixedServiceTransition *> transitions;
      for (const auto &t : x_.elapsed.fixed_service_transitions)
        if (t.action_ref == a.operation_ref &&
            t.kind != FixedTransitionKind::started)
          transitions.push_back(&t);
      std::sort(transitions.begin(), transitions.end(),
                [](const auto *l, const auto *r) {
                  return l->transition_ordinal < r->transition_ordinal;
                });
      for (const auto *t : transitions) {
        SourceRef source;
        if (const auto *body = std::get_if<FixedEligibilityChanged>(&t->cause))
          source = body->original_cause_ref;
        else if (const auto *terminal =
                     std::get_if<FixedOwnerTerminated>(&t->cause))
          source = terminal->owner_result_ref;
        else
          fail("PrivateMarketActualTransitionCauseRequired", t->transition_id);
        const auto &capacity = adopted(a, source);
        const auto actual =
            position(x_, {capacity.slot_ref.provider_domain,
                          "market_service_capacity", capacity.source_id});
        if (actual <= previous)
          fail("PrivateMarketCausalCapacityOrderMismatch", capacity.source_id);
        previous = actual;
      }
    }
    // A genuine conditioned premises change after the first enablement also has
    // its original P05 successor, even when there are no buyers to interrupt.
    if (conditions_)
      for (const auto &change : conditions_->changes.rows()) {
        const PrivateShopOperationAcceptance *first = nullptr;
        for (const auto &a : x_.sources.accepted_operations) {
          const auto *o = x_.shops.state.operations.find(a.operation_ref);
          const auto *shop =
              o ? x_.shops.state.shops.find(o->shop_id) : nullptr;
          if (shop && shop->terms.premises_id == change.second.facility_ref &&
              (!first || position(x_, private_shop_operation_started_source(
                                          a.operation_ref)) <
                             position(x_, private_shop_operation_started_source(
                                              first->operation_ref))))
            first = &a;
        }
        if (first) {
          const auto *header = x_.shops.common.state().facts.find(change.first);
          if (!header)
            fail("PrivateMarketOriginalConditionHeaderRequired", change.first);
          const SourceRef source{header->domain, header->kind, header->id};
          if (position(x_, source) >
              position(x_, private_shop_operation_started_source(
                               first->operation_ref)))
            adopted(*first, source);
        }
      }
    if (matched.size() !=
        purchases.market_service_sources.private_sources.size())
      fail("PrivateMarketCapacityOriginalCoverageMismatch");
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<FacilitySourceRef>
NativePrivateMarketServiceOriginalRead::read_private_market_change_source(
    const SourceRef &s) const {
  try {
    take(validate_market_service_condition_selection(identity_, x_.shops.common,
                                                     conditions_));
    if (condition_source(s)) {
      actual_body_transition(x_, s);
      return {true, {s, 2}, {}};
    }
    if (s.domain == private_shop_module &&
        s.kind == "private_shop_operating_started") {
      accepted(x_, s.id);
      return {true, {s, 2}, {}};
    }
    if (s.domain == private_shop_module &&
        (s.kind == "private_shop_operating_completed" ||
         s.kind == "private_shop_operating_interrupted")) {
      take(read_original_private_shop_operation_terminal(x_, {s, 2}));
      return {true, {s, 2}, {}};
    }
    if (s.kind == "facility_condition_change") {
      const auto *r = conditions_ ? conditions_->changes.find(s.id) : nullptr;
      if (!r || r->change_id != s.id)
        fail("PrivateMarketOriginalConditionChangeRequired", s.id);
      position(x_, s);
      return {true, {s, 1}, {}};
    }
    fail("PrivateMarketExactOriginalChangeRequired", s.id);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<WorldMoment>
NativePrivateMarketServiceOriginalRead::read_private_market_change_moment(
    const SourceRef &s) const {
  try {
    take(read_private_market_change_source(s));
    if (condition_source(s))
      return {true, actual_body_transition(x_, s).occurred_at, {}};
    if (s.kind == "private_shop_operating_started")
      return {true, accepted(x_, s.id).accepted_at, {}};
    if (s.kind == "facility_condition_change")
      return {true, conditions_->changes.find(s.id)->occurred_at, {}};
    return {true,
            take(read_original_private_shop_operation_terminal(x_, {s, 2}))
                ->occurred_at,
            {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<MarketServiceChangeCause>
NativePrivateMarketServiceOriginalRead::read_private_market_service_change(
    const MarketServiceDefinition &d, const FacilitySourceRef &r) const {
  try {
    const auto actual = take(read_private_market_change_source(r.source_ref));
    if (actual.source_schema != r.source_schema)
      fail("PrivateMarketOriginalChangeSchemaMismatch", r.source_ref.id);
    const auto &a = adoption(x_, d, r.source_ref);
    if (r.source_ref.kind == "private_shop_operating_started") {
      if (a.operation_ref != r.source_ref.id)
        fail("PrivateMarketActualOperationOpeningRequired", r.source_ref.id);
      bool first = true;
      for (const auto &prior : x_.sources.accepted_operations)
        if (rule(prior.market_definition_ref, d.definition_ref) &&
            position(x_, private_shop_operation_started_source(
                             prior.operation_ref)) < position(x_, r.source_ref))
          first = false;
      if (first)
        return {true,
                MarketServiceInitialized{d.asset_source_ref, r.source_ref},
                {}};
      return {true, MarketServiceOpeningChanged{r.source_ref}, {}};
    }
    if (r.source_ref.kind == "facility_condition_change") {
      const auto *change =
          conditions_ ? conditions_->changes.find(r.source_ref.id) : nullptr;
      const auto *o = x_.shops.state.operations.find(a.operation_ref);
      const auto *shop = o ? x_.shops.state.shops.find(o->shop_id) : nullptr;
      if (!change || !shop || change->facility_ref != shop->terms.premises_id)
        fail("PrivateMarketOriginalFacilityMismatch", r.source_ref.id);
      return {true, MarketServiceFacilityChanged{r.source_ref}, {}};
    }
    if (!condition_source(r.source_ref)) {
      const auto &terminal = *take(
          read_original_private_shop_operation_terminal(x_, {r.source_ref, 2}));
      if (terminal.operation_ref != a.operation_ref)
        fail("PrivateMarketTerminalOperationMismatch", r.source_ref.id);
    }
    return {true, MarketServiceStaffingChanged{r.source_ref}, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<PrivateMarketServiceBasis>
NativePrivateMarketServiceOriginalRead::read_private_market_basis(
    const MarketServiceDefinition &d,
    const MarketServiceChangeCause &changed) const {
  try {
    const auto source = cause(changed);
    const auto original = take(read_private_market_change_source(source));
    const auto expected = take(read_private_market_service_change(d, original));
    if (expected.index() != changed.index())
      fail("PrivateMarketChangeKindMismatch", source.id);
    const auto &a = adoption(x_, d, source);
    const auto *o = x_.shops.state.operations.find(a.operation_ref);
    const auto *shop = take(read_original_private_shop_opening(
        x_.shops, a.original_shop_epoch_ref));
    if (!o || o->shop_id != shop->id ||
        shop->terms.premises_id != d.asset_source_ref.id)
      fail("PrivateMarketOriginalAssetMismatch", source.id);
    PrivateMarketServiceBasis out;
    out.asset = {shop->id, a.original_shop_epoch_ref, shop->terms.premises_id,
                 a.original_shop_epoch_ref};
    out.opening = {a.market_definition_ref,
                   private_shop_operation_started_source(o->id),
                   {a.committed_service_window}};
    out.usage = {d.usage_rule_ref,
                 a.original_shop_epoch_ref,
                 private_shop_operation_started_source(o->id),
                 {a.committed_service_window}};
    bool terminal = source.domain == private_shop_module &&
                    (source.kind == "private_shop_operating_completed" ||
                     source.kind == "private_shop_operating_interrupted");
    if (!terminal) {
      PrivateMarketOperationSource staffing{o->id, {}};
      if (!o->staff_contract_id.empty())
        staffing.original_staff_participation_ref =
            SourceRef{private_shop_module, "private_shop_staff_accepted",
                      o->staff_contract_id};
      out.staffing.push_back(std::move(staffing));
    }
    return {true, std::move(out), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<std::vector<MarketServiceWorkerSegment>>
NativePrivateMarketServiceOriginalRead::read_private_market_workers(
    const PrivateMarketOperationSource &p, const MomentSpan &requested) const {
  try {
    if (cmp(requested.from, requested.to) >= 0)
      fail("PrivateMarketPositiveWorkerWindowRequired", p.operation_ref);
    const auto &a = accepted(x_, p.operation_ref);
    take(read_original_private_shop_operation_profile(x_, p.operation_ref));
    const auto *o = x_.shops.state.operations.find(p.operation_ref);
    if (!o)
      fail("PrivateMarketOriginalOperationRequired", p.operation_ref);
    if (o->staff_contract_id.empty()) {
      if (p.original_staff_participation_ref)
        fail("PrivateMarketOwnerStaffAlias", p.operation_ref);
    } else if (!p.original_staff_participation_ref ||
               !same_source_ref(*p.original_staff_participation_ref,
                                {private_shop_module,
                                 "private_shop_staff_accepted",
                                 o->staff_contract_id}))
      fail("PrivateMarketOriginalStaffBindingRequired", p.operation_ref);
    const auto *entry = find_fixed_elapsed(x_.elapsed, o->id);
    if (!entry)
      fail("PrivateMarketOriginalElapsedRequired", o->id);
    const auto now = take(x_.clock.read_current_moment());
    MomentSpan window = requested;
    if (cmp(window.from, a.committed_service_window.from) < 0)
      window.from = a.committed_service_window.from;
    if (cmp(window.to, a.committed_service_window.to) > 0)
      window.to = a.committed_service_window.to;
    std::vector<MarketServiceWorkerSegment> out;
    if (cmp(window.from, window.to) >= 0)
      return {true, std::move(out), {}};
    WorldMoment past_until = cmp(window.to, entry->last_settled_at) < 0
                                 ? window.to
                                 : entry->last_settled_at;
    if (cmp(window.from, past_until) < 0) {
      const bool due =
          std::holds_alternative<ElapsedRunning>(entry->timing) &&
          take(compare_fractions(entry->credited_units.value,
                                 entry->required_units.value)) == 0;
      auto service = take(due ? read_fixed_service_intervals_at_completion_due(
                                    x_.elapsed, o->id, window.from, past_until,
                                    now, x_.scale, x_.spatial.budget)
                              : read_fixed_service_intervals(
                                    x_.elapsed, o->id, window.from, past_until,
                                    now, x_.scale, x_.spatial.budget));
      class Workers final : public PrivateShopLaborSpanVisitor {
        const PrivateShopOperationSourceContext &x_;
        const PrivateShopOperation &o_;
        std::vector<MarketServiceWorkerSegment> &out_;

      public:
        Workers(const PrivateShopOperationSourceContext &x,
                const PrivateShopOperation &o,
                std::vector<MarketServiceWorkerSegment> &out)
            : x_(x), o_(o), out_(out) {}
        Status
        applied_span(const MomentSpan &span, const CharacterCapabilityView &v,
                     const ActorStationaryLocationSpan &location) override {
          try {
            MarketServiceWorkerSegment row{
                span, 1, {private_shop_operation_started_source(o_.id)}};
            for (const auto &s :
                 take(private_shop_capability_condition_sources(x_.body, v)))
              row.actual_sources.push_back(s.source_ref);
            for (const auto &s : location.original_sources)
              if (std::none_of(row.actual_sources.begin(),
                               row.actual_sources.end(), [&](const auto &old) {
                                 return same_source_ref(old, s.source_ref);
                               }))
                row.actual_sources.push_back(s.source_ref);
            if (!o_.staff_contract_id.empty())
              row.actual_sources.push_back({private_shop_module,
                                            "private_shop_staff_accepted",
                                            o_.staff_contract_id});
            out_.push_back(std::move(row));
            return common_success();
          } catch (Failure &e) {
            return {false, {}, std::move(e.error)};
          }
        }
      } visitor(x_, *o, out);
      take(visit_private_shop_original_provided_spans(x_, o->id, service,
                                                      visitor));
    }
    // This is a future capacity promise bounded by the actual current S0 due
    // and immutable accepted window, not a fabricated future labor/capability
    // proof.
    if (const auto *running = std::get_if<ElapsedRunning>(&entry->timing)) {
      MomentSpan future{cmp(window.from, now) > 0 ? window.from : now,
                        cmp(window.to, running->completion_due_at) < 0
                            ? window.to
                            : running->completion_due_at};
      if (cmp(future.from, future.to) < 0) {
        if (cmp(entry->last_settled_at, now))
          fail("PrivateMarketActualSettledBoundaryRequired", o->id);
        NativePrivateShopActionTimeRead owner(x_);
        const auto &r = private_shop_operation_action_time_registration();
        auto proof = take(owner.read_action_time_owner(r, o->id));
        if (proof.phase != ActionTimeOwnerPhase::working)
          fail("PrivateMarketActualWorkingOwnerRequired", o->id);
        take(read_action_time_projection(r, proof, x_.elapsed, now, x_.scale,
                                         owner, x_.spatial.budget));
        auto inputs = take(read_private_shop_current_service_inputs(x_, o->id));
        MarketServiceWorkerSegment row{
            future, 1, {private_shop_operation_started_source(o->id)}};
        for (const auto &v : inputs)
          if (const auto *s = std::get_if<FixedAppliedInput>(&v))
            if (!same_source_ref(s->source_ref, row.actual_sources.front()))
              row.actual_sources.push_back(s->source_ref);
        if (!o->staff_contract_id.empty()) {
          const auto *staff = take(read_original_private_shop_staff_contract(
              x_.shops, {private_shop_module, "private_shop_staff_accepted",
                         o->staff_contract_id}));
          if (staff->status != "active")
            fail("PrivateMarketActualStaffDutyRequired", o->id);
          if (cmp(row.window.to, {staff->through_tick, {}}) > 0)
            row.window.to = {staff->through_tick, {}};
          row.actual_sources.push_back(
              {private_shop_module, "private_shop_staff_accepted", staff->id});
        }
        if (cmp(row.window.from, row.window.to) < 0)
          out.push_back(std::move(row));
      }
    }
    return {true, std::move(out), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
