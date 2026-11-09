#include "household_market_resolver.hpp"
#include "household_purchase_cargo.hpp"
#include "named_household_service.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace named_household {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const StableId &code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{r.error};
  return std::move(r.value);
}
template <class T, class F> Outcome<T> guard(F &&f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
SourceRef event_ref(const StableId &id) {
  return {"local_economy", "household_purchase_service", id};
}
ReservationRef reservation(const StableId &id) {
  return {ReservationRef::Kind::domain_activity, id, "local_economy",
          "household_purchase"};
}
const HouseholdPurchase &purchase(const HouseholdPurchaseContext &x,
                                  const StableId &id) {
  const auto *p = x.state.purchases.find(id);
  if (!p || !std::holds_alternative<NamedHouseholdBuyer>(p->executor) ||
      !std::holds_alternative<ActorActionWorkBinding>(p->execution_binding))
    fail("NamedHouseholdPurchaseRequired", id);
  return *p;
}
const HouseholdPurchaseAcceptedTerms &
accepted(const HouseholdPurchaseContext &x, const HouseholdPurchase &p) {
  const auto *a = x.state.accepted_terms.find(p.accepted_terms_ref.id);
  if (!a || !same_source_ref(a->source, p.accepted_terms_ref))
    fail("HouseholdPurchaseAcceptanceMissing", p.purchase_id);
  return *a;
}
const HouseholdMarketServiceDefinition &
market(const HouseholdPurchaseContext &x, const HouseholdPurchase &p) {
  return *take(read_household_market_service_accepted(
      x.common, x.catalog, x.state, x.private_market_custody, accepted(x, p)));
}
const HouseholdServiceEvent2 &last_event(const HouseholdPurchaseContext &x,
                                         const StableId &id) {
  const HouseholdServiceEvent2 *found = nullptr;
  for (const auto &row : x.state.named_service_events.rows())
    if (row.second.purchase_id == id) {
      if (row.second.event_ordinal != (found ? found->event_ordinal + 1 : 0))
        fail("NamedHouseholdEventOrdinalMismatch", id);
      found = &row.second;
    }
  if (!found)
    fail("NamedHouseholdServiceEventMissing", id);
  return *found;
}
const NamedServiceClaimEpisode2 &episode(const HouseholdPurchaseContext &x,
                                         const StableId &id, Integer ordinal) {
  const NamedServiceClaimEpisode2 *found = nullptr;
  for (const auto &e : x.state.named_service_claim_episodes)
    if (e.purchase_id == id && e.ordinal == ordinal) {
      if (found)
        fail("DuplicateNamedHouseholdEpisode", id);
      found = &e;
    }
  if (!found)
    fail("NamedHouseholdEpisodeMissing", id);
  return *found;
}
NamedServiceClaimEpisode2 &mutable_episode(HouseholdPurchaseState &s,
                                           const StableId &id,
                                           Integer ordinal) {
  for (auto &e : s.named_service_claim_episodes)
    if (e.purchase_id == id && e.ordinal == ordinal)
      return e;
  fail("NamedHouseholdEpisodeMissing", id);
}
Integer revision(const FacilityClaimsState &state,
                 const FacilitySlotRef &slot) {
  Integer rev = 0;
  for (const auto &h : state.slot_histories)
    if (h.slot_ref == slot) {
      if (const auto *b = std::get_if<VacantPrunedFacilityPrefix>(&h.base))
        rev = b->last_removed_revision;
      for (const auto &m : h.mutations)
        rev = m.claim_set_revision;
    }
  if (rev == std::numeric_limits<Integer>::max())
    fail("HouseholdClaimRevisionOverflow");
  return rev;
}
void candidate(CommonStore &c, HouseholdPurchaseState &s,
               const HouseholdPurchaseContext &x) {
  if (&c != &x.common || &s != &x.state)
    fail("HouseholdPurchaseCandidateMismatch");
}
HouseholdServiceEvent2 add_event(CommonStore &c, HouseholdPurchaseState &state,
                                 const HouseholdPurchaseContext &x,
                                 const HouseholdPurchase &p,
                                 HouseholdServiceEventKind kind,
                                 Integer ordinal,
                                 std::optional<SourceRef> cause) {
  HouseholdServiceEvent2 e;
  e.event_id = take(c.next_id("household_service"));
  e.purchase_id = p.purchase_id;
  e.original_action_ref =
      std::get<ActorActionWorkBinding>(p.execution_binding).action_id;
  e.kind = kind;
  e.occurred_at = x.now;
  e.cause_ref = std::move(cause);
  e.episode_ordinal = ordinal;
  if (kind != HouseholdServiceEventKind::accepted) {
    const auto &prior = last_event(x, p.purchase_id);
    if (prior.event_ordinal == std::numeric_limits<Integer>::max() ||
        prior.kind == HouseholdServiceEventKind::completed ||
        prior.kind == HouseholdServiceEventKind::terminated ||
        cmp(prior.occurred_at, x.now) > 0)
      fail("NamedHouseholdServiceAlreadyTerminal", p.purchase_id);
    e.event_ordinal = prior.event_ordinal + 1;
  }
  if ((kind == HouseholdServiceEventKind::accepted) == bool(e.cause_ref))
    fail("NamedHouseholdEventCauseMismatch", p.purchase_id);
  take(c.append_fact(
      {e.event_id, "household_purchase_service", "local_economy"}));
  state.named_service_events.insert(e.event_id, e);
  return e;
}
const FixedServiceTransition &
previous_transition(const HouseholdPurchaseContext &x, const StableId &action) {
  const FixedServiceTransition *found = nullptr;
  for (const auto &t : x.elapsed.fixed_service_transitions)
    if (t.action_ref == action)
      found = &t;
  if (!found)
    fail("HouseholdFixedTransitionMissing", action);
  return *found;
}
FixedServiceTransition transition(const HouseholdPurchaseContext &x,
                                  const HouseholdPurchase &p,
                                  const HouseholdServiceEvent2 &event,
                                  const ActionElapsedState &entry) {
  const auto &previous = previous_transition(x, entry.action_ref);
  if (previous.transition_ordinal == std::numeric_limits<Integer>::max())
    fail("HouseholdTransitionOrdinalOverflow", p.purchase_id);
  FixedServiceTransition t;
  t.transition_id = event.event_id;
  t.action_ref = entry.action_ref;
  t.transition_ordinal = previous.transition_ordinal + 1;
  t.previous_transition_ref = previous.transition_id;
  t.occurred_at = event.occurred_at;
  t.credited_at = entry.credited_units;
  t.execution_source_ref = p.accepted_terms_ref;
  t.execution_source_schema = 1;
  t.previous_input_basis_refs = previous.input_basis_refs;
  if (event.kind == HouseholdServiceEventKind::suspended) {
    t.kind = FixedTransitionKind::suspended;
    t.cause = FixedOwnerStateChanged{event_ref(event.event_id), 2,
                                     x.catalog.continuation_rule_id};
  } else if (event.kind == HouseholdServiceEventKind::resumed) {
    t.kind = FixedTransitionKind::resumed;
    t.cause = FixedOwnerStateChanged{event_ref(event.event_id), 2,
                                     x.catalog.continuation_rule_id};
    t.input_basis_refs = {FixedAppliedInput{event_ref(event.event_id), 2}};
  } else {
    t.kind = FixedTransitionKind::terminal;
    t.cause = FixedOwnerTerminated{event_ref(event.event_id), 2};
    t.terminal_disposition = event.kind == HouseholdServiceEventKind::completed
                                 ? FixedTerminalDisposition::completed
                                 : FixedTerminalDisposition::interrupted;
  }
  return t;
}
void remove_action(CommonStore &common, const HouseholdPurchase &p) {
  const auto &binding = std::get<ActorActionWorkBinding>(p.execution_binding);
  const auto *current = common.action(binding.action_id);
  const auto &actor = std::get<NamedHouseholdBuyer>(p.executor).actor_id;
  if (!current || current->actor_id != actor)
    fail("NamedHouseholdActionMissing", p.purchase_id);
  ActionMutation erase;
  erase.kind = ActionMutation::Kind::erase;
  erase.action.id = binding.action_id;
  take(common.apply_action(erase));
  ActorMutation free;
  free.actor_id = actor;
  free.action_id = StableId{};
  take(common.apply_actor(free));
}
WorldMoment end_from_credit(const HouseholdPurchaseContext &x,
                            const ActionElapsedState &e,
                            const WorldMoment &from) {
  auto remaining =
      take(subtract_work_units(e.required_units, e.credited_units));
  auto duration =
      take(scale_units_to_duration(remaining, x.scale.units_per_half_day));
  return take(add_world_duration(from, duration));
}
WorkUnits supplied(const HouseholdPurchaseContext &x,
                   const HouseholdPurchase &p, const WorldMoment &through) {
  WorkUnits total;
  std::optional<WorldMoment> previous;
  for (const auto &s : x.state.service_segments)
    if (s.purchase_id == p.purchase_id) {
      if (cmp(s.window.from, s.window.to) >= 0 ||
          (previous && cmp(*previous, s.window.from) > 0))
        fail("NamedHouseholdSegmentOrderMismatch", p.purchase_id);
      take(x.environment.validate_purchase_service_segment(p, s));
      bool owned = false;
      for (const auto &ep : x.state.named_service_claim_episodes)
        if (ep.purchase_id == p.purchase_id) {
          const auto *began = take(
              read_named_household_service_event(x, ep.admission_result_ref));
          if (cmp(began->occurred_at, s.window.from) > 0)
            continue;
          auto claim =
              take(read_facility_claim_episode(x.claims, ep.claim_id, x.now));
          auto until = claim.original_claim.until;
          if (ep.close_result_ref) {
            const auto *close = take(
                read_named_household_service_event(x, *ep.close_result_ref));
            until = close->occurred_at;
          }
          if (cmp(claim.original_claim.from, s.window.from) <= 0 &&
              cmp(s.window.to, until) <= 0) {
            if (owned)
              fail("NamedHouseholdSegmentMultipleEpisodes", p.purchase_id);
            owned = true;
          }
        }
      if (!owned)
        fail("NamedHouseholdSegmentWithoutEpisode", p.purchase_id);
      if (cmp(s.window.from, through) < 0) {
        auto end = cmp(s.window.to, through) < 0 ? s.window.to : through;
        auto duration = take(elapsed_world_duration(s.window.from, end));
        total = take(
            add_work_units(total, take(duration_to_scale_units(
                                      duration, x.scale.units_per_half_day))));
      }
      previous = s.window.to;
    }
  return total;
}
} // namespace named_household
using namespace named_household;
Outcome<const HouseholdServiceEvent2 *>
read_named_household_service_event(const HouseholdPurchaseContext &x,
                                   const SourceRef &source) {
  return guard<const HouseholdServiceEvent2 *>([&] {
    if (source.domain != "local_economy" ||
        source.kind != "household_purchase_service")
      fail("WrongNamedHouseholdServiceSource", source.id);
    const auto *e = x.state.named_service_events.find(source.id);
    const auto *f = x.common.state().facts.find(source.id);
    if (!e || !f || f->kind != source.kind || f->domain != source.domain ||
        e->event_id != source.id || e->event_ordinal < 0 ||
        e->episode_ordinal < 0 || cmp(e->occurred_at, x.now) > 0)
      fail("NamedHouseholdServiceSourceMissing", source.id);
    const auto &p = purchase(x, e->purchase_id);
    const auto &a = accepted(x, p);
    if (e->original_action_ref !=
        std::get<ActorActionWorkBinding>(p.execution_binding).action_id)
      fail("NamedHouseholdEventActionMismatch", source.id);
    if (e->kind == HouseholdServiceEventKind::accepted) {
      if (e->event_ordinal || e->episode_ordinal || e->cause_ref ||
          cmp(e->occurred_at, a.accepted_at))
        fail("NamedHouseholdAcceptedEventMismatch", source.id);
    } else {
      if (!e->cause_ref || !valid_source_ref(*e->cause_ref))
        fail("NamedHouseholdEventCauseMissing", source.id);
      if (e->kind == HouseholdServiceEventKind::suspended ||
          e->kind == HouseholdServiceEventKind::terminated)
        take(x.environment.read_named_service_interruption(
            p, *e->cause_ref, e->occurred_at,
            e->kind == HouseholdServiceEventKind::terminated));
      else if (e->kind == HouseholdServiceEventKind::resumed) {
        if (!same_source_ref(*e->cause_ref, a.source))
          fail("NamedHouseholdResumeResponsibilityMismatch", source.id);
      } else if (e->kind == HouseholdServiceEventKind::completed) {
        bool coverage = false;
        for (const auto &s : x.state.service_segments)
          if (s.purchase_id == p.purchase_id &&
              same_source_ref(s.actual_coverage_source, *e->cause_ref) &&
              cmp(s.window.to, e->occurred_at) == 0)
            coverage = true;
        if (!coverage)
          fail("NamedHouseholdCompletionCoverageMissing", source.id);
      }
    }
    return e;
  });
}
Outcome<ActionTimeOwnerProof> read_named_household_action_time_owner(
    const HouseholdPurchaseContext &x,
    const ActionTimeRegistration &registration, const StableId &action) {
  return guard<ActionTimeOwnerProof>([&] {
    if (registration.action_kind != "household_purchase" ||
        registration.owner_domain != "local_economy" ||
        (x.selected_household_version != 2 &&
         x.selected_household_version != 3) ||
        registration.profile_version != x.selected_household_version ||
        registration.source_schema != 1)
      fail("UnsupportedNamedHouseholdTimeProfile");
    const HouseholdPurchase *found = nullptr;
    for (const auto &row : x.state.purchases.rows())
      if (const auto *b = std::get_if<ActorActionWorkBinding>(
              &row.second.execution_binding))
        if (b->action_id == action) {
          if (found)
            fail("DuplicateNamedHouseholdAction", action);
          found = &row.second;
        }
    if (!found)
      fail("NamedHouseholdActionOwnerMissing", action);
    const auto &p = *found;
    const auto &ep = episode(x, p.purchase_id, 0);
    const auto *start =
        take(read_named_household_service_event(x, ep.admission_result_ref));
    const auto &last = last_event(x, p.purchase_id);
    take(read_named_household_service_event(x, event_ref(last.event_id)));
    const auto &actor = std::get<NamedHouseholdBuyer>(p.executor).actor_id;
    CheckpointVector<std::string_view> participants, gates;
    participants.reserve(1);
    participants.push_back(actor);
    ActionTimeOwnerPhase current_phase = ActionTimeOwnerPhase::working;
    std::optional<ElapsedTimeSourceBuildView> ended;
    std::optional<std::string_view> terminal;
    if (last.kind == HouseholdServiceEventKind::suspended)
      current_phase = ActionTimeOwnerPhase::suspended;
    else if (last.kind == HouseholdServiceEventKind::completed ||
             last.kind == HouseholdServiceEventKind::terminated) {
      current_phase = ActionTimeOwnerPhase::terminal;
      ended = MomentOwnerSourceBuildView{
          {"local_economy", "household_purchase_service", last.event_id}, 2};
      terminal = last.event_id;
    }
    const SourceRefBuildView execution{p.accepted_terms_ref.domain,
                                       p.accepted_terms_ref.kind,
                                       p.accepted_terms_ref.id};
    return ActionTimeOwnerProof{ActionTimeOwnerProofBuildView{
        action,
        "household_purchase",
        actor,
        {participants},
        execution,
        1,
        MomentOwnerSourceBuildView{
            {"local_economy", "household_purchase_service", start->event_id},
            2},
        current_phase,
        std::nullopt,
        ended,
        terminal,
        {gates},
        std::nullopt}};
  });
}
Status record_named_household_admission_in_candidate(
    CommonStore &c, HouseholdPurchaseState &state,
    const HouseholdPurchaseContext &x, const StableId &id,
    FacilityClaim2 &claim, FacilityClaimMutationReceipt &receipt) {
  return guard<Done>([&] {
    candidate(c, state, x);
    const auto p = purchase(x, id);
    for (const auto &e : state.named_service_claim_episodes)
      if (e.purchase_id == id)
        fail("NamedHouseholdAlreadyAdmitted", id);
    const auto &a = accepted(x, p);
    if (cmp(claim.from, a.accepted_at) || claim.holder != reservation(id) ||
        claim.units != 1 || receipt.admitted_claims.size() != 1)
      fail("NamedHouseholdAdmissionMismatch", id);
    auto e =
        add_event(c, state, x, p, HouseholdServiceEventKind::accepted, 0, {});
    claim.source_result_ref = event_ref(e.event_id);
    receipt.source_result_ref = claim.source_result_ref;
    receipt.source_schema = 2;
    receipt.admitted_claims = {claim};
    state.named_service_claim_episodes.push_back(
        {id,
         0,
         claim.source_result_ref,
         claim.claim_id,
         {claim.slot_ref, receipt.claim_set_revision},
         {},
         {}});
    return Done{};
  });
}
Outcome<NamedHouseholdServiceCoverage>
read_named_household_service_coverage(const HouseholdPurchaseContext &x,
                                      const StableId &id,
                                      const MomentSpan &span) {
  return guard<NamedHouseholdServiceCoverage>([&] {
    const auto &p = purchase(x, id);
    const auto latest = last_event(x, id);
    if (p.state != HouseholdPurchasePhase::servicing ||
        (latest.kind != HouseholdServiceEventKind::accepted &&
         latest.kind != HouseholdServiceEventKind::resumed))
      fail("NamedHouseholdServiceNotRunning", id);
    const auto &ep = episode(x, id, latest.episode_ordinal);
    if (ep.close_result_ref || ep.close_receipt_ref)
      fail("NamedHouseholdEpisodeAlreadyClosed", id);
    auto actual =
        take(read_facility_claim_episode(x.claims, ep.claim_id, x.now));
    if (actual.close_receipt_ref || cmp(span.from, span.to) >= 0 ||
        cmp(actual.original_claim.from, span.from) > 0 ||
        cmp(span.to, actual.original_claim.until) > 0)
      fail("NamedHouseholdServiceOutsideClaim", id);
    auto participation =
        take(x.environment.read_named_service_participation(p, span));
    if (participation.empty())
      fail("NamedHouseholdParticipationSourceRequired", id);
    for (const auto &r : participation)
      if (!valid_source_ref(r))
        fail("NamedHouseholdParticipationSourceRequired", id);
    std::vector<FacilitySourceRef> capacity;
    auto first = take(x.claim_sources.capacity_source_at(
        actual.original_claim.slot_ref, span.from));
    capacity.push_back(first);
    auto changes = take(x.claim_sources.enumerate_capacity_changes(
        actual.original_claim.slot_ref, span.from, span.to));
    for (const auto &change : changes)
      if (cmp(change.occurred_at, span.to) < 0 &&
          std::none_of(capacity.begin(), capacity.end(), [&](const auto &old) {
            return same_source_ref(old.source_ref, change.source.source_ref) &&
                   old.source_schema == change.source.source_schema;
          }))
        capacity.push_back(change.source);
    WorldMoment cursor = span.from;
    for (const auto &r : capacity) {
      auto segments = take(x.claim_sources.read_verified_capacity(
          r, actual.original_claim.slot_ref, span));
      for (const auto &s : segments) {
        if (cmp(s.window.from, cursor) || s.source.capacity < 1 ||
            !same_source_ref(s.source.usage_right_ref,
                             actual.original_claim.usage_right_ref))
          fail("NamedHouseholdCapacityCoverageGap", id);
        bool open = false;
        for (const auto &a : s.source.availability_intervals)
          if (cmp(a.from, s.window.from) <= 0 && cmp(s.window.to, a.to) <= 0)
            open = true;
        if (!open)
          fail("NamedHouseholdMarketNotOpen", id);
        cursor = s.window.to;
      }
    }
    if (cmp(cursor, span.to))
      fail("NamedHouseholdCapacityCoverageGap", id);
    return NamedHouseholdServiceCoverage{
        id,
        std::get<ActorActionWorkBinding>(p.execution_binding).action_id,
        ep.admission_result_ref,
        ep.claim_id,
        span,
        capacity,
        actual.original_claim.usage_right_ref,
        participation};
  });
}
Status provide_named_household_service_interval_in_candidate(
    HouseholdPurchaseState &state, ActionElapsedStore &elapsed,
    const HouseholdPurchaseContext &x, const StableId &id,
    const MomentSpan &span) {
  return guard<Done>([&] {
    if (&state != &x.state || &elapsed != &x.elapsed)
      fail("HouseholdPurchaseCandidateMismatch");
    const auto p = purchase(x, id);
    auto coverage = take(read_named_household_service_coverage(x, id, span));
    auto *entry = find_fixed_elapsed(elapsed, coverage.original_action_ref);
    if (!entry)
      fail("HouseholdElapsedSourceMissing", id);
    auto next = take(prepare_fixed_elapsed_slice(*entry, span, x.scale));
    auto actual = take(x.environment.read_purchase_service_coverage(p, span));
    if (cmp(actual.window.from, span.from) || cmp(actual.window.to, span.to) ||
        !valid_source_ref(actual.actual_source) ||
        !actual.actual_labor_allocations.empty())
      fail("NamedHouseholdActualCoverageMismatch", id);
    HouseholdServiceSegment segment{
        id, span, actual.actual_source, {}, actual.exertion_source_refs};
    take(x.environment.validate_purchase_service_segment(p, segment));
    HouseholdServiceSegment *prior = nullptr;
    for (auto &s : state.service_segments)
      if (s.purchase_id == id)
        prior = &s;
    auto active_claim = take(read_facility_claim_episode(
        x.claims, coverage.active_claim_ref, x.now));
    bool same =
        prior &&
        cmp(active_claim.original_claim.from, prior->window.from) <= 0 &&
        cmp(prior->window.to, span.from) == 0 &&
        same_source_ref(prior->actual_coverage_source,
                        segment.actual_coverage_source) &&
        prior->exertion_source_refs.size() ==
            segment.exertion_source_refs.size();
    if (same)
      for (std::size_t n = 0; n < segment.exertion_source_refs.size(); ++n)
        same &= same_source_ref(prior->exertion_source_refs[n],
                                segment.exertion_source_refs[n]);
    if (same)
      prior->window.to = span.to;
    else
      state.service_segments.push_back(segment);
    entry = find_fixed_elapsed(elapsed, coverage.original_action_ref);
    entry->credited_units = next.credited_units;
    entry->last_settled_at = next.last_settled_at;
    return Done{};
  });
}
namespace named_household {
Outcome<FacilityClaimMutationReceipt>
close(CommonStore &c, HouseholdPurchaseState &state,
      ActionElapsedStore &elapsed, const HouseholdPurchaseContext &x,
      const StableId &id, const std::optional<SourceRef> &interruption,
      bool terminate) {
  return guard<FacilityClaimMutationReceipt>([&] {
    candidate(c, state, x);
    if (&elapsed != &x.elapsed)
      fail("HouseholdPurchaseCandidateMismatch");
    const auto p = purchase(x, id);
    const auto prior = last_event(x, id);
    if (interruption &&
        (prior.kind == HouseholdServiceEventKind::suspended ||
         prior.kind == HouseholdServiceEventKind::terminated) &&
        prior.cause_ref && same_source_ref(*prior.cause_ref, *interruption)) {
      const auto &ep = episode(x, id, prior.episode_ordinal);
      if (!ep.close_receipt_ref)
        fail("NamedHouseholdReplayReceiptMissing", id);
      return take(read_facility_claim_receipt(x.claims, *ep.close_receipt_ref));
    }
    if (prior.kind != HouseholdServiceEventKind::accepted &&
        prior.kind != HouseholdServiceEventKind::resumed)
      fail("NamedHouseholdServiceNotRunning", id);
    const auto ep = episode(x, id, prior.episode_ordinal);
    const auto claim =
        take(read_facility_claim_episode(x.claims, ep.claim_id, x.now));
    if (claim.close_receipt_ref)
      fail("NamedHouseholdClaimAlreadyClosed", id);
    const auto action =
        std::get<ActorActionWorkBinding>(p.execution_binding).action_id;
    auto *entry = find_fixed_elapsed(elapsed, action);
    if (!entry || cmp(entry->last_settled_at, x.now))
      fail("NamedHouseholdOldServiceNotSettled", id);
    auto equal = take(compare_fractions(entry->credited_units.value,
                                        entry->required_units.value));
    if (equal > 0)
      fail("NamedHouseholdServiceOverprovided", id);
    const bool complete = equal == 0;
    if (!interruption && !complete)
      fail("NamedHouseholdServiceNotDue", id);
    FacilityClaimCloseReason reason = FacilityClaimCloseReason::completed;
    std::optional<SourceRef> actual = interruption;
    if (complete) {
      const HouseholdServiceSegment *latest = nullptr;
      for (const auto &s : state.service_segments)
        if (s.purchase_id == id)
          latest = &s;
      if (!latest || cmp(latest->window.to, x.now))
        fail("NamedHouseholdCompletionCoverageMissing", id);
      actual = latest->actual_coverage_source;
      terminate = false;
    } else
      reason = take(x.environment.read_named_service_interruption(
          p, *actual, x.now, terminate));
    auto kind = complete    ? HouseholdServiceEventKind::completed
                : terminate ? HouseholdServiceEventKind::terminated
                            : HouseholdServiceEventKind::suspended;
    auto e = add_event(c, state, x, p, kind, ep.ordinal, actual);
    Integer rev = revision(x.claims, claim.original_claim.slot_ref);
    auto &closed = mutable_episode(state, id, ep.ordinal);
    closed.close_result_ref = event_ref(e.event_id);
    closed.close_receipt_ref =
        FacilityClaimReceiptRef{claim.original_claim.slot_ref, rev + 1};
    auto *edited = state.purchases.find(id);
    if (edited->phase_revision == std::numeric_limits<Integer>::max())
      fail("HouseholdPurchaseRevisionOverflow", id);
    edited->state = complete ? HouseholdPurchasePhase::awaiting_dispatch
                             : HouseholdPurchasePhase::blocked;
    ++edited->phase_revision;
    if (complete || terminate)
      edited->terminal_result_ref = event_ref(e.event_id);
    HouseholdPurchaseContracts sources{x};
    auto t = transition(x, p, e, *entry);
    auto prepared = take(prepare_fixed_service_change(
        elapsed, action, t,
        kind == HouseholdServiceEventKind::suspended
            ? std::optional<StableId>{e.event_id}
            : std::nullopt,
        household_purchase_action_time_registration(x), x.scale, sources));
    take(apply_fixed_service_change_in_candidate(
        elapsed, std::move(prepared),
        household_purchase_action_time_registration(x), x.scale, sources));
    if (complete || terminate)
      remove_action(c, p);
    auto capacity = take(x.claim_sources.capacity_source_at(
        claim.original_claim.slot_ref, x.now));
    return FacilityClaimMutationReceipt{
        rev + 1, x.now,     event_ref(e.event_id), 2, {{ep.claim_id, reason}},
        {},      {capacity}};
  });
}
} // namespace named_household
Outcome<FacilityClaimMutationReceipt>
close_named_household_service_owner_in_candidate(
    CommonStore &c, HouseholdPurchaseState &s, ActionElapsedStore &e,
    const HouseholdPurchaseContext &x, const StableId &id) {
  return named_household::close(c, s, e, x, id, {}, false);
}
Outcome<FacilityClaimMutationReceipt>
suspend_named_household_service_owner_in_candidate(
    CommonStore &c, HouseholdPurchaseState &s, ActionElapsedStore &e,
    const HouseholdPurchaseContext &x, const StableId &id,
    const SourceRef &cause, bool terminal) {
  return named_household::close(c, s, e, x, id, cause, terminal);
}
Outcome<NamedHouseholdResumeQuote>
preview_resume_household_purchase_service(const HouseholdPurchaseContext &x,
                                          const StableId &id) {
  return guard<NamedHouseholdResumeQuote>([&] {
    const auto &p = purchase(x, id);
    const auto &latest = last_event(x, id);
    if (latest.kind != HouseholdServiceEventKind::suspended ||
        p.state != HouseholdPurchasePhase::blocked)
      fail("NamedHouseholdServiceNotResumable", id);
    const auto &old = episode(x, id, latest.episode_ordinal);
    if (!old.close_result_ref || !old.close_receipt_ref)
      fail("NamedHouseholdSuspensionReceiptMissing", id);
    const auto &b = std::get<ActorActionWorkBinding>(p.execution_binding);
    const auto *entry = find_fixed_elapsed(x.elapsed, b.action_id);
    const auto *action = x.common.action(b.action_id);
    if (!entry || !action ||
        !std::holds_alternative<ElapsedSuspended>(entry->timing))
      fail("NamedHouseholdOriginalActionUnavailable", id);
    take(x.environment.validate_named_service_continuation(p, x.now));
    const auto &service = market(x, p);
    auto presence = take(x.environment.read_purchase_presence_for_service(
        accepted(x, p).plan, service, x.now));
    auto until = end_from_credit(x, *entry, x.now);
    if (cmp(x.now, until) >= 0)
      fail("NamedHouseholdNoRemainingService", id);
    auto source =
        take(x.claim_sources.capacity_source_at(service.service_slot, x.now));
    FacilityClaimRequest2 request{service.service_slot,
                                  reservation(id),
                                  x.now,
                                  until,
                                  1,
                                  service.service_usage_right_ref,
                                  {}};
    auto quote = take(quote_facility_claim(x.claims, x.claim_catalog, request,
                                           {source}, x.now, x.claim_sources));
    return NamedHouseholdResumeQuote{x.common.state().identity,
                                     id,
                                     event_ref(latest.event_id),
                                     x.now,
                                     presence,
                                     quote,
                                     {source}};
  });
}
Outcome<StableId> resume_household_purchase_service_in_candidate(
    CommonStore &c, HouseholdPurchaseState &s, FacilityClaimsState &claims,
    ActionElapsedStore &elapsed, const HouseholdPurchaseContext &x,
    const NamedHouseholdResumeQuote &quote) {
  return guard<StableId>([&] {
    candidate(c, s, x);
    if (&claims != &x.claims || &elapsed != &x.elapsed ||
        cmp(quote.resumed_at, x.now))
      fail("HouseholdPurchaseCandidateMismatch");
    const auto &identity = c.state().identity;
    if (std::tie(identity.world_id, identity.execution_epoch, identity.revision,
                 identity.tick) !=
        std::tie(quote.identity.world_id, quote.identity.execution_epoch,
                 quote.identity.revision, quote.identity.tick))
      fail("StaleNamedHouseholdResumeQuote");
    auto current =
        take(preview_resume_household_purchase_service(x, quote.purchase_id));
    if (!same_source_ref(current.suspension_source, quote.suspension_source) ||
        current.claim.capacity_revision != quote.claim.capacity_revision ||
        current.claim.claim_set_revision != quote.claim.claim_set_revision ||
        cmp(current.claim.requested.until, quote.claim.requested.until) ||
        !same_source_ref(current.presence.actual_source,
                         quote.presence.actual_source))
      fail("StaleNamedHouseholdResumeQuote");
    const auto p = purchase(x, quote.purchase_id);
    const auto last = last_event(x, p.purchase_id);
    if (last.episode_ordinal == std::numeric_limits<Integer>::max())
      fail("NamedHouseholdEpisodeOrdinalOverflow");
    auto e = add_event(c, s, x, p, HouseholdServiceEventKind::resumed,
                       last.episode_ordinal + 1, p.accepted_terms_ref);
    auto claim_id = take(c.next_id("household_market_claim"));
    const auto &r = current.claim.requested;
    FacilityClaim2 claim{claim_id,
                         r.slot_ref,
                         r.holder,
                         r.from,
                         r.until,
                         r.units,
                         current.claim.capacity_revision,
                         r.usage_right_ref,
                         event_ref(e.event_id)};
    FacilityClaimMutationReceipt mutation{current.claim.claim_set_revision + 1,
                                          x.now,
                                          event_ref(e.event_id),
                                          2,
                                          {},
                                          {claim},
                                          current.capacity_sources};
    s.named_service_claim_episodes.push_back(
        {p.purchase_id,
         e.episode_ordinal,
         event_ref(e.event_id),
         claim_id,
         {claim.slot_ref, mutation.claim_set_revision},
         {},
         {}});
    auto *edited = s.purchases.find(p.purchase_id);
    if (edited->phase_revision == std::numeric_limits<Integer>::max())
      fail("HouseholdPurchaseRevisionOverflow");
    edited->state = HouseholdPurchasePhase::servicing;
    ++edited->phase_revision;
    auto applied = take(apply_facility_claim_mutation(
        claims, x.claim_catalog, claim.slot_ref, mutation,
        current.claim.claim_set_revision, x.now, x.claim_sources));
    claims = std::move(applied);
    const auto action =
        std::get<ActorActionWorkBinding>(p.execution_binding).action_id;
    const auto *entry = find_fixed_elapsed(elapsed, action);
    HouseholdPurchaseContracts sources{x};
    auto t = transition(x, p, e, *entry);
    auto adopted_inputs =
        take(x.environment.read_named_service_adoption_inputs(p, e));
    t.input_basis_refs.insert(t.input_basis_refs.end(), adopted_inputs.begin(),
                              adopted_inputs.end());
    for (const auto &source : current.capacity_sources)
      t.input_basis_refs.push_back(
          FixedAppliedInput{source.source_ref, source.source_schema});
    auto prepared = take(prepare_fixed_service_change(
        elapsed, action, t, {}, household_purchase_action_time_registration(x),
        x.scale, sources));
    take(apply_fixed_service_change_in_candidate(
        elapsed, std::move(prepared),
        household_purchase_action_time_registration(x), x.scale, sources));
    return e.event_id;
  });
}
namespace named_household {
std::vector<const HouseholdServiceEvent2 *>
events(const HouseholdPurchaseContext &x, const HouseholdPurchase &p) {
  std::vector<const HouseholdServiceEvent2 *> result;
  for (const auto &row : x.state.named_service_events.rows())
    if (row.second.purchase_id == p.purchase_id) {
      const auto *event =
          take(read_named_household_service_event(x, event_ref(row.first)));
      if (event->event_ordinal != static_cast<Integer>(result.size()))
        fail("NamedHouseholdEventOrdinalMismatch", p.purchase_id);
      if (result.empty()) {
        if (event->kind != HouseholdServiceEventKind::accepted)
          fail("NamedHouseholdAcceptedEventMissing", p.purchase_id);
      } else {
        const auto &before = *result.back();
        if (cmp(before.occurred_at, event->occurred_at) > 0)
          fail("NamedHouseholdEventTimeRegression", event->event_id);
        if (event->kind == HouseholdServiceEventKind::resumed) {
          if (before.kind != HouseholdServiceEventKind::suspended ||
              event->episode_ordinal != before.episode_ordinal + 1)
            fail("NamedHouseholdResumeOrderMismatch", event->event_id);
        } else if ((before.kind != HouseholdServiceEventKind::accepted &&
                    before.kind != HouseholdServiceEventKind::resumed) ||
                   event->kind == HouseholdServiceEventKind::accepted ||
                   event->episode_ordinal != before.episode_ordinal)
          fail("NamedHouseholdCloseOrderMismatch", event->event_id);
      }
      result.push_back(event);
    }
  if (result.empty())
    fail("NamedHouseholdAcceptedEventMissing", p.purchase_id);
  return result;
}
WorldMoment closed_at(const HouseholdPurchaseContext &x,
                      const NamedServiceClaimEpisode2 &ep,
                      const FacilityClaimEpisode &original) {
  if (bool(ep.close_result_ref) != bool(ep.close_receipt_ref))
    fail("NamedHouseholdEpisodeClosePairMismatch", ep.claim_id);
  if (ep.close_result_ref) {
    const auto *close =
        take(read_named_household_service_event(x, *ep.close_result_ref));
    if (close->purchase_id != ep.purchase_id ||
        close->episode_ordinal != ep.ordinal)
      fail("NamedHouseholdCloseBacklinkMismatch", ep.claim_id);
    return close->occurred_at;
  }
  return original.original_claim.until;
}
} // namespace named_household
Status validate_named_household_fixed_sources(
    const HouseholdPurchaseContext &x, const ActionTimeOwnerProof &proof,
    const ActionElapsedState &entry,
    const std::vector<FixedServiceTransition> &transitions) {
  return guard<Done>([&] {
    const auto *found = static_cast<const HouseholdPurchase *>(nullptr);
    for (const auto &row : x.state.purchases.rows())
      if (const auto *b = std::get_if<ActorActionWorkBinding>(
              &row.second.execution_binding))
        if (b->action_id == entry.action_ref) {
          if (found)
            fail("DuplicateNamedHouseholdAction", entry.action_ref);
          found = &row.second;
        }
    if (!found)
      fail("NamedHouseholdActionOwnerMissing", entry.action_ref);
    const auto &p = purchase(x, found->purchase_id);
    const auto &a = accepted(x, p);
    auto original = take(read_named_household_action_time_owner(
        x, household_purchase_action_time_registration(x), entry.action_ref));
    auto required =
        take(fixed_duration_work_units(1, x.scale.units_per_half_day));
    if (proof.action_ref != original.action_ref ||
        proof.actor_id != original.actor_id ||
        !same_source_ref(proof.original_execution_source,
                         original.original_execution_source) ||
        proof.phase != original.phase ||
        entry.execution_source_ref != p.purchase_id ||
        entry.duration_definition_ref != x.catalog.duration_definition_id ||
        entry.owner_cost_state_ref != p.purchase_id ||
        entry.owner_work_remainder || cmp(entry.started_at, a.accepted_at) ||
        take(compare_fractions(entry.required_units.value, required.value)))
      fail("NamedHouseholdElapsedOriginalMismatch", p.purchase_id);
    auto original_events = events(x, p);
    if (transitions.size() != original_events.size())
      fail("NamedHouseholdTransitionCoverageMismatch", p.purchase_id);
    for (std::size_t n = 0; n < transitions.size(); ++n) {
      const auto &t = transitions[n];
      const auto &event = *original_events[n];
      if (t.transition_id != event.event_id ||
          t.action_ref != entry.action_ref ||
          t.transition_ordinal != event.event_ordinal ||
          cmp(t.occurred_at, event.occurred_at) ||
          !same_source_ref(t.execution_source_ref, p.accepted_terms_ref) ||
          t.execution_source_schema != 1 ||
          take(compare_fractions(t.credited_at.value,
                                 supplied(x, p, event.occurred_at).value)))
        fail("NamedHouseholdTransitionOriginalMismatch", event.event_id);
      if (event.kind == HouseholdServiceEventKind::accepted ||
          event.kind == HouseholdServiceEventKind::resumed)
        take(x.environment.validate_named_service_adoption_inputs(
            p, event, t.input_basis_refs));
      if (event.kind == HouseholdServiceEventKind::accepted) {
        const auto *cause = std::get_if<FixedOwnerAdopted>(&t.cause);
        if (t.kind != FixedTransitionKind::started || !cause ||
            cause->owner_source_schema != 2 ||
            !same_source_ref(cause->owner_source_ref,
                             event_ref(event.event_id)))
          fail("NamedHouseholdStartTransitionMismatch", event.event_id);
      } else if (event.kind == HouseholdServiceEventKind::resumed ||
                 event.kind == HouseholdServiceEventKind::suspended) {
        const auto *cause = std::get_if<FixedOwnerStateChanged>(&t.cause);
        if (t.kind != (event.kind == HouseholdServiceEventKind::resumed
                           ? FixedTransitionKind::resumed
                           : FixedTransitionKind::suspended) ||
            !cause || cause->owner_source_schema != 2 ||
            cause->transition_rule_ref != x.catalog.continuation_rule_id ||
            !same_source_ref(cause->owner_source_ref,
                             event_ref(event.event_id)))
          fail("NamedHouseholdStateTransitionMismatch", event.event_id);
      } else {
        const auto *cause = std::get_if<FixedOwnerTerminated>(&t.cause);
        if (t.kind != FixedTransitionKind::terminal || !cause ||
            cause->owner_result_schema != 2 ||
            !same_source_ref(cause->owner_result_ref,
                             event_ref(event.event_id)) ||
            t.terminal_disposition !=
                (event.kind == HouseholdServiceEventKind::completed
                     ? FixedTerminalDisposition::completed
                     : FixedTerminalDisposition::interrupted))
          fail("NamedHouseholdTerminalTransitionMismatch", event.event_id);
      }
    }
    for (const auto &segment : x.state.service_segments)
      if (segment.purchase_id == p.purchase_id &&
          cmp(segment.window.to, entry.last_settled_at) > 0)
        fail("NamedHouseholdFutureServiceSegment", p.purchase_id);
    auto total = supplied(x, p, entry.last_settled_at);
    if (take(compare_fractions(total.value, entry.credited_units.value)) ||
        take(compare_fractions(total.value, required.value)) > 0)
      fail("NamedHouseholdCreditedCoverageMismatch", p.purchase_id);
    const auto &last = *original_events.back();
    const auto &ep = episode(x, p.purchase_id, last.episode_ordinal);
    if (last.kind == HouseholdServiceEventKind::accepted ||
        last.kind == HouseholdServiceEventKind::resumed) {
      const auto *running = std::get_if<ElapsedRunning>(&entry.timing);
      auto claim =
          take(read_facility_claim_episode(x.claims, ep.claim_id, x.now));
      if (!running || ep.close_result_ref ||
          cmp(running->completion_due_at, claim.original_claim.until))
        fail("NamedHouseholdRunningClaimMismatch", p.purchase_id);
    } else if (last.kind == HouseholdServiceEventKind::suspended) {
      const auto *suspended = std::get_if<ElapsedSuspended>(&entry.timing);
      if (!suspended || suspended->reason_source_ref != last.event_id ||
          cmp(entry.last_settled_at, last.occurred_at))
        fail("NamedHouseholdSuspendedTimingMismatch", p.purchase_id);
    } else {
      const auto *terminal = std::get_if<ElapsedTerminal>(&entry.timing);
      if (!terminal || terminal->outcome_ref != last.event_id ||
          cmp(entry.last_settled_at, last.occurred_at) ||
          (last.kind == HouseholdServiceEventKind::completed &&
           take(compare_fractions(total.value, required.value))))
        fail("NamedHouseholdTerminalTimingMismatch", p.purchase_id);
    }
    return Done{};
  });
}
Status validate_named_household_service_claim(
    const HouseholdPurchaseContext &x, const FacilityClaim2 &claim,
    const FacilityClaimMutationReceipt &receipt, bool admission) {
  return guard<Done>([&] {
    const auto &p = purchase(x, claim.holder.id);
    const auto &service = market(x, p);
    const NamedServiceClaimEpisode2 *matched = nullptr;
    for (const auto &ep : x.state.named_service_claim_episodes)
      if (ep.claim_id == claim.claim_id) {
        if (matched)
          fail("DuplicateNamedHouseholdClaim", claim.claim_id);
        matched = &ep;
      }
    if (!matched || matched->purchase_id != p.purchase_id ||
        claim.holder != reservation(p.purchase_id) ||
        claim.slot_ref != service.service_slot || claim.units != 1 ||
        !same_source_ref(claim.usage_right_ref,
                         service.service_usage_right_ref) ||
        !same_source_ref(claim.source_result_ref,
                         matched->admission_result_ref))
      fail("NamedHouseholdClaimSourceMismatch", claim.claim_id);
    const auto *begin = take(
        read_named_household_service_event(x, matched->admission_result_ref));
    if (begin->episode_ordinal != matched->ordinal ||
        (begin->kind != HouseholdServiceEventKind::accepted &&
         begin->kind != HouseholdServiceEventKind::resumed) ||
        cmp(begin->occurred_at, claim.from))
      fail("NamedHouseholdClaimAdmissionMismatch", claim.claim_id);
    auto required =
        take(fixed_duration_work_units(1, x.scale.units_per_half_day));
    auto credited = supplied(x, p, begin->occurred_at);
    auto remaining = take(subtract_work_units(required, credited));
    auto duration =
        take(scale_units_to_duration(remaining, x.scale.units_per_half_day));
    auto until = take(add_world_duration(begin->occurred_at, duration));
    if (cmp(claim.until, until) || cmp(claim.from, claim.until) >= 0)
      fail("NamedHouseholdClaimRemainingMismatch", claim.claim_id);
    if (admission) {
      if (receipt.source_schema != 2 ||
          !same_source_ref(receipt.source_result_ref,
                           matched->admission_result_ref) ||
          cmp(receipt.occurred_at, begin->occurred_at) ||
          matched->admission_receipt_ref.slot_ref != claim.slot_ref ||
          matched->admission_receipt_ref.claim_set_revision !=
              receipt.claim_set_revision ||
          receipt.admitted_claims.size() != 1 ||
          receipt.admitted_claims.front().claim_id != claim.claim_id ||
          !receipt.closed_claims.empty())
        fail("NamedHouseholdAdmissionReceiptMismatch", claim.claim_id);
    } else {
      if (!matched->close_result_ref || !matched->close_receipt_ref ||
          receipt.source_schema != 2 ||
          !same_source_ref(receipt.source_result_ref,
                           *matched->close_result_ref) ||
          matched->close_receipt_ref->slot_ref != claim.slot_ref ||
          matched->close_receipt_ref->claim_set_revision !=
              receipt.claim_set_revision ||
          receipt.closed_claims.size() != 1 ||
          receipt.closed_claims.front().claim_id != claim.claim_id ||
          !receipt.admitted_claims.empty())
        fail("NamedHouseholdCloseReceiptMismatch", claim.claim_id);
      const auto *close = take(
          read_named_household_service_event(x, *matched->close_result_ref));
      if (cmp(close->occurred_at, receipt.occurred_at) ||
          cmp(close->occurred_at, claim.from) < 0 ||
          cmp(close->occurred_at, claim.until) > 0)
        fail("NamedHouseholdCloseTimeMismatch", claim.claim_id);
      auto reason =
          close->kind == HouseholdServiceEventKind::completed
              ? FacilityClaimCloseReason::completed
              : take(x.environment.read_named_service_interruption(
                    p, *close->cause_ref, close->occurred_at,
                    close->kind == HouseholdServiceEventKind::terminated));
      if (receipt.closed_claims.front().reason != reason)
        fail("NamedHouseholdCloseReasonMismatch", claim.claim_id);
    }
    return Done{};
  });
}
Status validate_named_household_service(const HouseholdPurchaseContext &x,
                                        const HouseholdPurchase &p) {
  return guard<Done>([&] {
    purchase(x, p.purchase_id);
    const auto &a = accepted(x, p);
    const auto all = events(x, p);
    const auto &binding = std::get<ActorActionWorkBinding>(p.execution_binding);
    if (binding.action_kind != "household_purchase" ||
        !same_source_ref(binding.source_owner_ref, p.accepted_terms_ref) ||
        p.last_consumed_period_ref || x.catalog.continuation_rule_id.empty())
      fail("NamedHouseholdExecutionBindingMismatch", p.purchase_id);
    for (const auto &r : x.state.service_results.rows())
      if (r.second.purchase_id == p.purchase_id)
        fail("NamedHouseholdDuplicateCompletionPayload", p.purchase_id);
    std::vector<const NamedServiceClaimEpisode2 *> episodes;
    std::set<StableId> claims;
    for (const auto &ep : x.state.named_service_claim_episodes)
      if (ep.purchase_id == p.purchase_id) {
        if (ep.ordinal != static_cast<Integer>(episodes.size()) ||
            !claims.insert(ep.claim_id).second ||
            (ep.ordinal == 0 && ep.claim_id != p.service_claim_ref))
          fail("NamedHouseholdEpisodeOrderMismatch", p.purchase_id);
        episodes.push_back(&ep);
      }
    if (episodes.empty())
      fail("NamedHouseholdEpisodeMissing", p.purchase_id);
    std::optional<WorldMoment> previous_close;
    Integer open = 0;
    for (const auto *ep : episodes) {
      auto original =
          take(read_facility_claim_episode(x.claims, ep->claim_id, x.now));
      auto admission = take(
          read_facility_claim_receipt(x.claims, ep->admission_receipt_ref));
      take(validate_named_household_service_claim(x, original.original_claim,
                                                  admission, true));
      if (previous_close &&
          cmp(*previous_close, original.original_claim.from) > 0)
        fail("NamedHouseholdEpisodeOverlap", p.purchase_id);
      if (bool(ep->close_result_ref) != bool(ep->close_receipt_ref) ||
          bool(ep->close_receipt_ref) != bool(original.close_receipt_ref))
        fail("NamedHouseholdEpisodeCloseMismatch", ep->claim_id);
      WorldMoment end = original.original_claim.until;
      if (ep->close_receipt_ref) {
        if (ep->close_receipt_ref->slot_ref !=
                original.close_receipt_ref->slot_ref ||
            ep->close_receipt_ref->claim_set_revision !=
                original.close_receipt_ref->claim_set_revision)
          fail("NamedHouseholdEpisodeReceiptMismatch", ep->claim_id);
        auto close =
            take(read_facility_claim_receipt(x.claims, *ep->close_receipt_ref));
        take(x.claim_sources.validate_claim_close(
            original.original_claim,
            close.closed_claims.empty()
                ? FacilityClaimCloseReason::completed
                : ([&] {
                    for (const auto &c : close.closed_claims)
                      if (c.claim_id == ep->claim_id)
                        return c.reason;
                    fail("NamedHouseholdCloseReceiptMissingClaim",
                         ep->claim_id);
                  }()),
            close));
        end = close.occurred_at;
        previous_close = end;
      } else {
        ++open;
        previous_close.reset();
      }
      WorldMoment cursor = original.original_claim.from;
      for (const auto &segment : x.state.service_segments)
        if (segment.purchase_id == p.purchase_id &&
            cmp(original.original_claim.from, segment.window.from) <= 0 &&
            cmp(segment.window.to, end) <= 0) {
          if (cmp(cursor, segment.window.from))
            fail("NamedHouseholdEpisodeServiceGap", ep->claim_id);
          cursor = segment.window.to;
        }
      if (ep->close_receipt_ref && cmp(cursor, end))
        fail("NamedHouseholdUnsettledEpisodeClose", ep->claim_id);
    }
    const auto *entry = find_fixed_elapsed(x.elapsed, binding.action_id);
    if (!entry)
      fail("NamedHouseholdElapsedMissing", p.purchase_id);
    std::vector<FixedServiceTransition> transitions;
    for (const auto &t : x.elapsed.fixed_service_transitions)
      if (t.action_ref == binding.action_id)
        transitions.push_back(t);
    auto proof = take(read_named_household_action_time_owner(
        x, household_purchase_action_time_registration(x), binding.action_id));
    take(validate_named_household_fixed_sources(x, proof, *entry, transitions));
    const auto &last = *all.back();
    bool ended = last.kind == HouseholdServiceEventKind::completed ||
                 last.kind == HouseholdServiceEventKind::terminated;
    bool running = last.kind == HouseholdServiceEventKind::accepted ||
                   last.kind == HouseholdServiceEventKind::resumed;
    if (open != (running ? 1 : 0) ||
        episodes.size() != static_cast<std::size_t>(last.episode_ordinal) + 1)
      fail("NamedHouseholdActiveEpisodeMismatch", p.purchase_id);
    const auto *action = x.common.action(binding.action_id);
    const auto *actor =
        x.common.actor(std::get<NamedHouseholdBuyer>(p.executor).actor_id);
    if ((ended && action) ||
        (!ended &&
         (!action || !actor || actor->action_id != binding.action_id)))
      fail("NamedHouseholdActionLifecycleMismatch", p.purchase_id);
    const auto *retail = x.retail.state.settlements.find(p.retail_receipt_ref);
    if (!retail)
      fail("NamedHouseholdRetailMissing", p.purchase_id);
    auto cargo_history = take(read_household_purchase_cargo(x, p.purchase_id));
    HouseholdPurchasePhase phase = HouseholdPurchasePhase::servicing;
    Integer revision = 1, total = 0;
    bool completed = false;
    std::optional<SourceRef> terminal;
    for (const auto &f : x.common.state().facts.rows()) {
      if (const auto *event = x.state.named_service_events.find(f.first))
        if (event->purchase_id == p.purchase_id &&
            event->kind != HouseholdServiceEventKind::accepted) {
          if (revision == std::numeric_limits<Integer>::max())
            fail("HouseholdPurchaseRevisionOverflow");
          ++revision;
          if (event->kind == HouseholdServiceEventKind::resumed)
            phase = HouseholdPurchasePhase::servicing;
          else if (event->kind == HouseholdServiceEventKind::completed) {
            phase = HouseholdPurchasePhase::awaiting_dispatch;
            completed = true;
            terminal = event_ref(event->event_id);
          } else {
            phase = HouseholdPurchasePhase::blocked;
            if (event->kind == HouseholdServiceEventKind::terminated)
              terminal = event_ref(event->event_id);
          }
        }
      if (const auto *handover = x.state.handovers.find(f.first))
        if (handover->purchase_id == p.purchase_id) {
          const bool received =
              handover->to_inventory_id == a.plan.destination_inventory;
          if (!completed || handover->id != f.first ||
              f.second.kind != (received
                                    ? "household_purchase_delivered"
                                    : "household_purchase_cargo_handover") ||
              f.second.domain != "local_economy")
            fail("NamedHouseholdHandoverSourceMismatch", f.first);
          if (received) {
            for (const auto &lot : handover->actual_lots) {
              if (lot.quantity <= 0 ||
                  total > std::numeric_limits<Integer>::max() - lot.quantity)
                fail("NamedHouseholdHandoverQuantityMismatch", f.first);
              total += lot.quantity;
            }
            if (total > a.quantity)
              fail("NamedHouseholdHandoverQuantityMismatch", f.first);
            if (total == a.quantity) {
              phase = HouseholdPurchasePhase::received;
              if (revision == std::numeric_limits<Integer>::max())
                fail("HouseholdPurchaseRevisionOverflow");
              ++revision;
            }
          }
        }
      if (const auto *change = x.state.phase_changes.find(f.first))
        if (change->purchase_id == p.purchase_id) {
          if (!completed || change->id != f.first ||
              f.second.kind != "household_purchase_phase_changed" ||
              f.second.domain != "local_economy" || change->before != phase ||
              change->revision_before != revision ||
              revision == std::numeric_limits<Integer>::max() ||
              change->revision_after != revision + 1)
            fail("NamedHouseholdPhaseHistoryMismatch", f.first);
          auto before = p;
          before.state = phase;
          before.phase_revision = revision;
          take(x.environment.validate_purchase_phase_cause(
              before, change->after, change->actual_cause,
              change->occurred_at));
          phase = change->after;
          revision = change->revision_after;
        }
    }
    if (p.state != phase || p.phase_revision != revision ||
        bool(p.terminal_result_ref) != bool(terminal) ||
        (terminal && !same_source_ref(*p.terminal_result_ref, *terminal)))
      fail("NamedHouseholdPhaseProjectionMismatch", p.purchase_id);
    if (total != cargo_history.received_quantity)
      fail("HouseholdCargoReceivedProjectionMismatch", p.purchase_id);
    take(validate_household_purchase_cargo(x, p));
    return Done{};
  });
}

} // namespace hanmo::resident
