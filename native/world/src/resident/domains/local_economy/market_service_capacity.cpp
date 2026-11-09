#include "market_service_capacity.hpp"
#include "household_purchase_contract.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace market_capacity {
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
// 公市容量只消费真实人员上下文；缺源原样拒绝，不返回空人员成功。
const PublicMarketStaffingContext &
public_staffing(const MarketServiceCapacityContext &x) {
  if (!x.public_staffing)
    throw Failure{common_failure("PublicMarketStaffingContextRequired", {},
                                 ErrorKind::unsupported_capability)
                      .error};
  return *x.public_staffing;
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
bool same(const ActorFoundationRuleRef &a, const ActorFoundationRuleRef &b) {
  return std::tie(a.id, a.version, a.catalog_digest) ==
         std::tie(b.id, b.version, b.catalog_digest);
}
FacilitySourceRef ref(const MarketServiceCapacitySource &s) {
  return {{s.slot_ref.provider_domain, "market_service_capacity", s.source_id},
          s.source_schema};
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
const MarketServiceDefinition &definition(const MarketServiceCapacityContext &x,
                                          const ActorFoundationRuleRef &r) {
  const MarketServiceDefinition *out = nullptr;
  for (const auto &d : x.purchases.market_service_definitions)
    if (same(d.definition_ref, r)) {
      if (out)
        fail("DuplicateMarketServiceDefinition", r.id);
      out = &d;
    }
  if (!out)
    fail("MarketServiceDefinitionMissing", r.id);
  return *out;
}
const std::vector<MarketServiceCapacitySource> &
sources(const MarketServiceCapacityContext &x, const FacilitySlotRef &slot) {
  if (slot.slot_kind != "market_service" ||
      (slot.provider_domain != "local_economy" &&
       slot.provider_domain != "commerce"))
    fail("UnsupportedMarketServiceSlot", slot.facility_id);
  if (slot.provider_domain == "local_economy")
    public_staffing(x);
  return slot.provider_domain == "local_economy"
             ? x.purchases.market_service_sources.public_sources
             : x.purchases.market_service_sources.private_sources;
}
std::size_t order(const MarketServiceCapacityContext &, const SourceRef &);
std::vector<const MarketServiceCapacitySource *>
chain(const MarketServiceCapacityContext &x, const FacilitySlotRef &slot) {
  std::vector<const MarketServiceCapacitySource *> rows;
  for (const auto &s : sources(x, slot))
    if (s.slot_ref == slot) {
      if (s.source_revision != static_cast<Integer>(rows.size()) + 1 ||
          s.source_schema != 1 || s.source_id.empty() ||
          cmp(s.occurred_at, x.now) > 0)
        fail("MarketServiceSourceChainMismatch", s.source_id);
      if (rows.empty()) {
        if (s.previous_source_ref ||
            !std::holds_alternative<MarketServiceInitialized>(s.change_cause))
          fail("MarketServiceGenesisRequired", s.source_id);
      } else if (!s.previous_source_ref ||
                 !same_source_ref(*s.previous_source_ref,
                                  ref(*rows.back()).source_ref) ||
                 cmp(rows.back()->occurred_at, s.occurred_at) > 0 ||
                 std::holds_alternative<MarketServiceInitialized>(
                     s.change_cause))
        fail("MarketServicePredecessorMismatch", s.source_id);
      if (!rows.empty() &&
          order(x, ref(*rows.back()).source_ref) >= order(x, ref(s).source_ref))
        fail("MarketServiceActualSourceAppendOrderMismatch", s.source_id);
      rows.push_back(&s);
    }
  return rows;
}
const MarketServiceCapacitySource &source(const MarketServiceCapacityContext &x,
                                          const FacilitySourceRef &r,
                                          const FacilitySlotRef &slot) {
  if (r.source_schema != 1 || r.source_ref.domain != slot.provider_domain ||
      r.source_ref.kind != "market_service_capacity")
    fail("MarketServiceSourceReferenceMismatch", r.source_ref.id);
  for (const auto *s : chain(x, slot))
    if (s->source_id == r.source_ref.id)
      return *s;
  fail("MarketServiceSourceMissing", r.source_ref.id);
}
std::size_t order(const MarketServiceCapacityContext &x, const SourceRef &r) {
  const auto *f = x.common.state().facts.find(r.id);
  if (!f || f->domain != r.domain || f->kind != r.kind)
    fail("MarketOriginalFactMissing", r.id);
  const auto &ids = x.common.state().facts.order();
  return static_cast<std::size_t>(std::find(ids.begin(), ids.end(), r.id) -
                                  ids.begin());
}
bool owned_body_source(const FacilitySourceRef &r) {
  return r.source_schema == 2 && r.source_ref.domain == "actor_foundation" &&
         r.source_ref.kind == "condition_receipt";
}
FacilitySourceRef original_change(const MarketServiceCapacityContext &x,
                                  const MarketServiceCapacitySource &s) {
  return take(x.originals.read_original_market_change_source(s.slot_ref,
                                                             s.change_cause));
}
std::size_t change_position(const MarketServiceCapacityContext &x,
                            const MarketServiceCapacitySource &s) {
  const auto original = original_change(x, s);
  if (owned_body_source(original))
    return order(x, ref(s).source_ref);
  return order(x, original.source_ref);
}
std::size_t mutation_position(const MarketServiceCapacityContext &x,
                              const FacilitySlotRef &slot,
                              const SourceRef &source) {
  for (const auto *s : chain(x, slot))
    if (same_source_ref(cause(s->change_cause), source)) {
      const auto original = original_change(x, *s);
      if (owned_body_source(original))
        return order(x, ref(*s).source_ref);
    }
  return order(x, source);
}
bool covers(const std::vector<MomentSpan> &intervals, const MomentSpan &s) {
  for (const auto &i : intervals)
    if (cmp(i.from, s.from) <= 0 && cmp(s.to, i.to) <= 0)
      return true;
  return false;
}
void intervals(const std::vector<MomentSpan> &spans) {
  std::optional<WorldMoment> last;
  for (const auto &s : spans) {
    if (cmp(s.from, s.to) >= 0 || (last && cmp(*last, s.from) > 0))
      fail("InvalidMarketServiceIntervals");
    last = s.to;
  }
}
void validate_inputs(const MarketServiceCapacityContext &x,
                     const MarketServiceCapacitySource &s) {
  const auto &d = definition(x, s.definition_ref);
  if (d.slot_ref.provider_domain == "local_economy")
    public_staffing(x);
  if (d.slot_ref != s.slot_ref || d.physical_service_slots <= 0 ||
      d.place_ref.empty() || d.definition_ref.version <= 0 ||
      d.definition_ref.catalog_digest !=
          x.common.state().metadata.content_digest ||
      d.staffing_rule !=
          MarketStaffingRule::one_qualified_worker_per_service_slot ||
      s.staffing_bindings.size() > 64)
    fail("InvalidMarketServiceDefinition", s.source_id);
  auto one = take(normalize_fraction("1", "1"));
  if (take(compare_fractions(d.max_claim_half_days.value, one)) < 0)
    fail("MarketServiceClaimLimitRequired", d.definition_ref.id);
  intervals(d.opening_intervals);
  intervals(s.opening_basis.committed_intervals);
  intervals(s.usage_right_basis.valid_intervals);
  if (!same(d.usage_rule_ref, s.usage_right_basis.usage_rule_ref) ||
      !valid_source_ref(s.opening_basis.accepted_opening_source_ref) ||
      !valid_source_ref(s.usage_right_basis.original_right_ref) ||
      !valid_source_ref(
          s.usage_right_basis.original_acceptance_or_approval_ref) ||
      !valid_source_ref(cause(s.change_cause)))
    fail("MarketServiceOriginalBasisMissing", s.source_id);
  if ((s.slot_ref.provider_domain == "local_economy") !=
      std::holds_alternative<PublicMarketAssetBasis>(s.asset_basis))
    fail("MarketServiceAssetProviderMismatch", s.source_id);
  StableId previous;
  for (const auto &b : s.staffing_bindings) {
    StableId key;
    bool pub = std::holds_alternative<PublicMarketStaffingSource>(b);
    if (pub)
      key = std::get<PublicMarketStaffingSource>(b).staffing_binding_ref;
    else {
      const auto &p = std::get<PrivateMarketOperationSource>(b);
      key = p.operation_ref + "/" +
            (p.original_staff_participation_ref
                 ? p.original_staff_participation_ref->id
                 : StableId{});
    }
    if (key.empty() || (!previous.empty() && key <= previous) ||
        pub != (s.slot_ref.provider_domain == "local_economy"))
      fail("MarketServiceStaffingBindingOrder", s.source_id);
    previous = key;
  }
  take(x.originals.validate_original_market_service_inputs(d, s));
  auto at = take(
      x.originals.read_original_market_change_at(s.slot_ref, s.change_cause));
  if (cmp(at, s.occurred_at))
    fail("MarketServiceSourceTimeMismatch", s.source_id);
}
MarketServiceCapacitySource
prospective(const MarketServiceCapacityContext &x,
            const ActorFoundationRuleRef &r,
            const MarketServiceChangeCause &changed) {
  const auto &d = definition(x, r);
  auto history = chain(x, d.slot_ref);
  auto input =
      take(x.originals.read_original_market_service_inputs(d, changed));
  MarketServiceCapacitySource s;
  s.slot_ref = d.slot_ref;
  s.source_revision = static_cast<Integer>(history.size()) + 1;
  if (!history.empty())
    s.previous_source_ref = ref(*history.back()).source_ref;
  s.occurred_at =
      take(x.originals.read_original_market_change_at(d.slot_ref, changed));
  if (cmp(s.occurred_at, x.now))
    fail("MarketServiceChangeNotCurrent", r.id);
  s.definition_ref = r;
  s.asset_basis = std::move(input.asset);
  s.opening_basis = std::move(input.opening);
  s.staffing_bindings = std::move(input.staffing);
  s.condition_basis = std::move(input.condition);
  s.usage_right_basis = std::move(input.usage);
  s.change_cause = changed;
  validate_inputs(x, s);
  return s;
}
std::vector<ProspectiveMarketCapacitySegment>
project(const MarketServiceCapacityContext &x,
        const MarketServiceCapacitySource &s, const MomentSpan &requested) {
  if (cmp(requested.from, requested.to) >= 0)
    fail("InvalidMarketServiceWindow");
  const auto &d = definition(x, s.definition_ref);
  if (d.slot_ref.provider_domain == "local_economy")
    public_staffing(x);
  std::vector<MarketServiceWorkerSegment> workers;
  for (const auto &binding : s.staffing_bindings) {
    if (const auto *public_staff =
            std::get_if<PublicMarketStaffingSource>(&binding)) {
      auto actual = take(read_public_market_staffing_labor_history(
          public_staffing(x), public_staff->staffing_binding_ref, requested));
      for (const auto &segment : actual.eligible_service_segments)
        workers.push_back({segment.span, segment.eligible_workers,
                           segment.actual_qualification_source_refs});
    } else {
      auto actual = take(x.originals.read_private_market_service_workers(
          std::get<PrivateMarketOperationSource>(binding), requested));
      workers.insert(workers.end(), actual.begin(), actual.end());
    }
  }
  auto conditions = take(x.originals.read_market_service_condition(
      d, s.condition_basis, requested));
  std::vector<WorldMoment> cuts{requested.from, requested.to};
  auto cut = [&](const MomentSpan &span) {
    if (cmp(span.from, span.to) >= 0)
      fail("InvalidMarketServiceCoverage");
    if (cmp(requested.from, span.from) < 0 && cmp(span.from, requested.to) < 0)
      cuts.push_back(span.from);
    if (cmp(requested.from, span.to) < 0 && cmp(span.to, requested.to) < 0)
      cuts.push_back(span.to);
  };
  for (const auto &i : d.opening_intervals)
    cut(i);
  for (const auto &i : s.opening_basis.committed_intervals)
    cut(i);
  for (const auto &i : s.usage_right_basis.valid_intervals)
    cut(i);
  for (const auto &i : workers)
    cut(i.window);
  for (const auto &i : conditions)
    cut(i.window);
  std::sort(cuts.begin(), cuts.end(),
            [](const auto &a, const auto &b) { return cmp(a, b) < 0; });
  cuts.erase(
      std::unique(cuts.begin(), cuts.end(),
                  [](const auto &a, const auto &b) { return cmp(a, b) == 0; }),
      cuts.end());
  std::vector<ProspectiveMarketCapacitySegment> out;
  for (std::size_t n = 0; n + 1 < cuts.size(); ++n) {
    MomentSpan part{cuts[n], cuts[n + 1]};
    Integer qualified = 0, physical = 0;
    std::size_t condition_matches = 0;
    for (const auto &i : conditions)
      if (cmp(i.window.from, part.from) <= 0 &&
          cmp(part.to, i.window.to) <= 0) {
        if (i.available_physical_slots < 0 ||
            i.available_physical_slots > d.physical_service_slots ||
            !valid_source_ref(i.actual_source))
          fail("InvalidMarketPhysicalCapacity", s.source_id);
        physical = i.available_physical_slots;
        ++condition_matches;
      }
    if (condition_matches != 1)
      fail("MarketConditionCoverageRequired", s.source_id);
    for (const auto &i : workers)
      if (cmp(i.window.from, part.from) <= 0 &&
          cmp(part.to, i.window.to) <= 0) {
        if (i.qualified_workers < 0 || i.actual_sources.empty() ||
            qualified >
                std::numeric_limits<Integer>::max() - i.qualified_workers)
          fail("InvalidMarketStaffCoverage", s.source_id);
        qualified += i.qualified_workers;
      }
    Integer capacity = std::min(physical, qualified);
    if (!covers(d.opening_intervals, part) ||
        !covers(s.opening_basis.committed_intervals, part) ||
        !covers(s.usage_right_basis.valid_intervals, part))
      capacity = 0;
    ProspectiveMarketCapacitySegment projection;
    projection.window = part;
    projection.slot_ref = s.slot_ref;
    projection.place_ref = d.place_ref;
    projection.capacity = capacity;
    projection.prospective_revision = s.source_revision;
    if (capacity > 0)
      projection.availability_intervals.push_back(part);
    projection.usage_right_ref = s.usage_right_basis.original_right_ref;
    projection.original_change = cause(s.change_cause);
    projection.previous_source_ref = s.previous_source_ref;
    out.push_back(std::move(projection));
  }
  return out;
}
FacilityCapacityChange change(const MarketServiceCapacitySource &s) {
  return {ref(s), s.occurred_at, s.source_revision,
          s.previous_source_ref
              ? std::optional<FacilitySourceRef>{{*s.previous_source_ref, 1}}
              : std::nullopt};
}
} // namespace market_capacity
using namespace market_capacity;
Status NativeMarketServiceCapacityRead::validate_market_source(
    const MarketServiceCapacitySource &s) const {
  return guard<Done>([&] {
    const auto &actual = source(x_, ref(s), s.slot_ref);
    if (&actual != &s)
      fail("MarketServiceBorrowedSourceRequired", s.source_id);
    validate_inputs(x_, s);
    const auto original = original_change(x_, s);
    const auto cut = order(x_, ref(s).source_ref);
    if (!owned_body_source(original) && order(x_, original.source_ref) >= cut)
      fail("MarketCapacityBeforeOriginalChange", s.source_id);
    return Done{};
  });
}
Outcome<WorldMoment> NativeMarketServiceCapacityRead::initial_source_at(
    const FacilitySlotRef &slot) const {
  return guard<WorldMoment>([&] {
    auto rows = chain(x_, slot);
    if (rows.empty())
      fail("MarketServiceSourceMissing", slot.facility_id);
    return rows.front()->occurred_at;
  });
}
Outcome<FacilitySourceRef> NativeMarketServiceCapacityRead::capacity_source_at(
    const FacilitySlotRef &slot, const WorldMoment &at) const {
  return guard<FacilitySourceRef>([&] {
    const MarketServiceCapacitySource *found = nullptr;
    for (const auto *s : chain(x_, slot))
      if (cmp(s->occurred_at, at) <= 0)
        found = s;
    if (!found)
      fail("MarketServiceSourceMissing", slot.facility_id);
    take(validate_market_source(*found));
    return ref(*found);
  });
}
Outcome<std::vector<VerifiedFacilityCapacitySegment>>
NativeMarketServiceCapacityRead::read_verified_capacity(
    const FacilitySourceRef &r, const FacilitySlotRef &slot,
    const MomentSpan &requested) const {
  return guard<std::vector<VerifiedFacilityCapacitySegment>>([&] {
    const auto &s = source(x_, r, slot);
    take(validate_market_source(s));
    MomentSpan window = requested;
    if (cmp(window.from, s.occurred_at) < 0)
      window.from = s.occurred_at;
    for (const auto *next : chain(x_, slot))
      if (next->source_revision > s.source_revision) {
        if (cmp(next->occurred_at, window.to) < 0)
          window.to = next->occurred_at;
        break;
      }
    if (cmp(window.from, window.to) >= 0)
      return std::vector<VerifiedFacilityCapacitySegment>{};
    std::vector<VerifiedFacilityCapacitySegment> result;
    for (const auto &part : project(x_, s, window)) {
      FacilityCapacitySource2 value;
      value.slot_ref = part.slot_ref;
      value.place_ref = part.place_ref;
      value.capacity = part.capacity;
      value.capacity_unit_id = market_service_unit;
      value.capacity_revision = part.prospective_revision;
      value.availability_intervals = part.availability_intervals;
      value.usage_right_ref = part.usage_right_ref;
      value.source_fact_ref = ref(s).source_ref;
      value.identity = x_.common.state().identity;
      value.admission_disposition = AdmitWithinCapacity{};
      result.push_back({part.window, std::move(value)});
    }
    return result;
  });
}
Outcome<std::vector<ProspectiveMarketCapacitySegment>>
read_prospective_market_service_capacity(
    const MarketServiceCapacityContext &x,
    const ActorFoundationRuleRef &definition,
    const MarketServiceChangeCause &cause, const MomentSpan &span) {
  return guard<std::vector<ProspectiveMarketCapacitySegment>>([&] {
    auto candidate = prospective(x, definition, cause);
    if (cmp(span.from, candidate.occurred_at) < 0)
      fail("MarketProspectiveWindowBeforeChange");
    return project(x, candidate, span);
  });
}
Outcome<std::vector<ProspectiveMarketCapacitySegment>>
read_market_service_adopted_change_projection(
    const MarketServiceCapacityContext &x, const FacilitySlotRef &slot,
    const SourceRef &original, const WorldMoment &at, const MomentSpan &span) {
  return guard<std::vector<ProspectiveMarketCapacitySegment>>([&] {
    const MarketServiceCapacitySource *found = nullptr;
    for (const auto *row : chain(x, slot))
      if (!std::holds_alternative<MarketServiceInitialized>(
              row->change_cause) &&
          same_source_ref(cause(row->change_cause), original)) {
        if (found || cmp(row->occurred_at, at))
          fail("MarketAdoptedChangeAmbiguous", original.id);
        found = row;
      }
    if (!found || cmp(span.from, at) < 0 || cmp(at, x.now) > 0)
      fail("MarketAdoptedChangeRequired", original.id);
    NativeMarketServiceCapacityRead read{x};
    take(read.validate_market_source(*found));
    if (!owned_body_source(original_change(x, *found)) &&
        order(x, original) >= order(x, ref(*found).source_ref))
      fail("MarketCapacityBeforeOriginalChange", original.id);
    return project(x, *found, span);
  });
}
Outcome<std::vector<FacilityCapacityChange>>
NativeMarketServiceCapacityRead::enumerate_capacity_changes(
    const FacilitySlotRef &slot, const WorldMoment &from,
    const WorldMoment &through) const {
  return guard<std::vector<FacilityCapacityChange>>([&] {
    std::vector<FacilityCapacityChange> out;
    for (const auto *s : chain(x_, slot))
      if (cmp(from, s->occurred_at) <= 0 && cmp(s->occurred_at, through) <= 0) {
        take(validate_market_source(*s));
        out.push_back(change(*s));
      }
    return out;
  });
}
Outcome<FacilityCapacityChange>
NativeMarketServiceCapacityRead::read_capacity_history_base(
    const FacilitySlotRef &slot, const FacilityHistoryBase &base) const {
  return guard<FacilityCapacityChange>([&] {
    auto rows = chain(x_, slot);
    if (rows.empty())
      fail("MarketServiceSourceMissing", slot.facility_id);
    if (std::holds_alternative<InitialEmptyFacilityClaims>(base))
      return change(*rows.front());
    const auto &pruned = std::get<VacantPrunedFacilityPrefix>(base);
    auto adopted = mutation_position(x_, slot, pruned.last_close_result_ref);
    const MarketServiceCapacitySource *selected = nullptr;
    for (const auto *s : rows)
      if (cmp(s->occurred_at, pruned.cut_at) < 0 ||
          (cmp(s->occurred_at, pruned.cut_at) == 0 &&
           change_position(x_, *s) <= adopted))
        selected = s;
    if (!selected)
      fail("MarketServicePrunedBaseMissing", slot.facility_id);
    return change(*selected);
  });
}
Outcome<FacilitySourceRef>
NativeMarketServiceCapacityRead::read_capacity_reconciliation_source(
    const FacilitySlotRef &slot, const FacilityCapacityChange &c) const {
  return guard<FacilitySourceRef>([&] {
    const auto &s = source(x_, c.source, slot);
    take(validate_market_source(s));
    if (c.revision != s.source_revision || cmp(c.occurred_at, s.occurred_at))
      fail("MarketServiceChangeMismatch", s.source_id);
    return original_change(x_, s);
  });
}
Outcome<FacilityMutationCapacityOrder>
NativeMarketServiceCapacityRead::compare_mutation_to_capacity_change(
    const FacilitySlotRef &slot, const FacilityClaimMutationReceipt &m,
    const FacilityCapacityChange &c) const {
  return guard<FacilityMutationCapacityOrder>([&] {
    auto root = take(read_capacity_reconciliation_source(slot, c));
    if (m.source_schema == root.source_schema &&
        same_source_ref(m.source_result_ref, root.source_ref)) {
      take(validate_mutation_source(slot, m));
      return FacilityMutationCapacityOrder::reconciles;
    }
    const auto &original = source(x_, c.source, slot);
    auto a = mutation_position(x_, slot, m.source_result_ref),
         b = change_position(x_, original);
    if (a == b)
      fail("MarketMutationOrderAmbiguous", m.source_result_ref.id);
    return a < b ? FacilityMutationCapacityOrder::before
                 : FacilityMutationCapacityOrder::after;
  });
}
Outcome<std::vector<FacilityClaimDependency>>
NativeMarketServiceCapacityRead::enumerate_claim_dependencies(
    const FacilitySlotRef &slot) const {
  return x_.holders.enumerate_holder_claim_dependencies(slot);
}
Status NativeMarketServiceCapacityRead::validate_claim_admission(
    const FacilityClaim2 &c, const FacilityClaimMutationReceipt &r) const {
  return guard<Done>([&] {
    const MarketServiceCapacitySource *adopted = nullptr;
    for (const auto &capacity : r.capacity_source_refs) {
      if (capacity.source_ref.kind != "market_service_capacity" ||
          capacity.source_ref.domain != c.slot_ref.provider_domain)
        continue;
      const auto &original = source(x_, capacity, c.slot_ref);
      if (original.source_revision == c.capacity_revision) {
        if (adopted)
          fail("DuplicateMarketAdmissionCapacity", c.claim_id);
        adopted = &original;
      }
    }
    if (!adopted)
      fail("MarketAdmissionCapacitySourceMissing", c.claim_id);
    const auto &d = definition(x_, adopted->definition_ref);
    auto duration = take(elapsed_world_duration(c.from, c.until));
    if (take(compare_fractions(duration.value, d.max_claim_half_days.value)) >
        0)
      fail("MarketClaimDurationExceeded", c.claim_id);
    take(x_.holders.validate_holder_claim_admission(c, r));
    return Done{};
  });
}
Status NativeMarketServiceCapacityRead::validate_claim_close(
    const FacilityClaim2 &c, FacilityClaimCloseReason why,
    const FacilityClaimMutationReceipt &r) const {
  return guard<Done>([&] {
    auto direct = x_.holders.validate_holder_claim_close(c, why, r);
    if (direct.ok)
      return Done{};
    Integer matches = 0;
    for (const auto &episode : x_.purchases.named_service_claim_episodes)
      if (episode.claim_id == c.claim_id && episode.close_result_ref) {
        const auto *event = x_.purchases.named_service_events.find(
            episode.close_result_ref->id);
        if (!event || !event->cause_ref ||
            cmp(event->occurred_at, r.occurred_at))
          continue;
        if (event->kind != HouseholdServiceEventKind::completed &&
            !same_source_ref(*event->cause_ref, r.source_result_ref))
          continue;
        if (event->kind == HouseholdServiceEventKind::completed &&
            !(r.source_schema == 2 &&
              r.source_result_ref.domain == "actor_foundation" &&
              r.source_result_ref.kind == "condition_receipt") &&
            order(x_, *episode.close_result_ref) <
                mutation_position(x_, c.slot_ref, r.source_result_ref))
          continue;
        auto original = r;
        original.source_result_ref = *episode.close_result_ref;
        original.source_schema = 2;
        original.closed_claims = {{c.claim_id, why}};
        original.admitted_claims.clear();
        take(x_.holders.validate_holder_claim_close(c, why, original));
        ++matches;
      }
    const auto *owner = x_.purchases.purchases.find(c.holder.id);
    if (owner &&
        std::holds_alternative<CohortHouseholdBuyer>(owner->executor)) {
      if (why == FacilityClaimCloseReason::suspended) {
        for (const auto &row : x_.purchases.phase_changes.rows()) {
          const auto &change = row.second;
          if (change.purchase_id != owner->purchase_id ||
              change.before != HouseholdPurchasePhase::servicing ||
              change.after != HouseholdPurchasePhase::blocked ||
              cmp(change.occurred_at, r.occurred_at) ||
              !same_source_ref(change.actual_cause, r.source_result_ref))
            continue;
          auto original = r;
          original.source_result_ref = {
              "local_economy", "household_purchase_phase_changed", change.id};
          original.source_schema = 1;
          original.closed_claims = {{c.claim_id, why}};
          original.admitted_claims.clear();
          auto valid = x_.holders.validate_holder_claim_close(c, why, original);
          if (valid.ok)
            ++matches;
        }
      } else if (why == FacilityClaimCloseReason::completed) {
        for (const auto &row : x_.purchases.service_results.rows()) {
          const auto &result = row.second;
          const auto *period = x_.purchases.work_periods.find(row.first);
          if (result.purchase_id != owner->purchase_id || !period ||
              period->disposition != WorkPeriodDisposition::provided ||
              period->facility_claim_refs !=
                  std::vector<StableId>{c.claim_id} ||
              cmp(result.completed_at, r.occurred_at) ||
              (!(r.source_schema == 2 &&
                 r.source_result_ref.domain == "actor_foundation" &&
                 r.source_result_ref.kind == "condition_receipt") &&
               order(x_, result.coverage_source) <
                   mutation_position(x_, c.slot_ref, r.source_result_ref)))
            continue;
          auto original = r;
          original.source_result_ref = result.coverage_source;
          original.source_schema = 1;
          original.closed_claims = {{c.claim_id, why}};
          original.admitted_claims.clear();
          take(x_.holders.validate_holder_claim_close(c, why, original));
          ++matches;
        }
      }
    }
    if (matches != 1)
      fail("MarketClaimOriginalCloseMissing", c.claim_id);
    return Done{};
  });
}
Status NativeMarketServiceCapacityRead::validate_mutation_source(
    const FacilitySlotRef &slot, const FacilityClaimMutationReceipt &r) const {
  return guard<Done>([&] {
    auto direct = x_.holders.validate_holder_mutation_source(slot, r);
    if (direct.ok)
      return Done{};
    bool original = false;
    for (const auto *s : chain(x_, slot))
      if (s->source_revision > 1 &&
          same_source_ref(cause(s->change_cause), r.source_result_ref) &&
          r.source_schema == original_change(x_, *s).source_schema &&
          cmp(s->occurred_at, r.occurred_at) == 0) {
        take(validate_market_source(*s));
        if (original)
          fail("DuplicateMarketCapacityCause", r.source_result_ref.id);
        original = true;
      }
    if (!original || !r.admitted_claims.empty())
      fail("MarketCapacityDispositionRequired", r.source_result_ref.id);
    for (const auto &close : r.closed_claims) {
      auto episode =
          take(read_facility_claim_episode(x_.claims, close.claim_id, x_.now));
      take(validate_claim_close(episode.original_claim, close.reason, r));
    }
    return Done{};
  });
}
Status NativeMarketServiceCapacityRead::validate_pruned_prefix(
    const FacilitySlotRef &slot, const VacantPrunedFacilityPrefix &p) const {
  return guard<Done>([&] {
    if (p.last_removed_revision <= 0 || p.last_close_source_schema <= 0)
      fail("InvalidMarketPrunedPrefix", slot.facility_id);
    mutation_position(x_, slot, p.last_close_result_ref);
    take(read_capacity_history_base(slot, p));
    for (const auto &d : take(enumerate_claim_dependencies(slot)))
      if (d.receipt && d.receipt->slot_ref == slot &&
          d.receipt->claim_set_revision <= p.last_removed_revision)
        fail("MarketClaimHistoryStillRequired", d.claim_id);
    return Done{};
  });
}
Outcome<Integer>
NativeMarketServiceCapacityRead::read_additional_normal_capacity(
    const FacilitySlotRef &slot, const MomentSpan &span,
    const std::vector<const FacilityClaim2 *> &claims,
    const FacilityCapacitySource2 &capacity) const {
  return guard<Integer>([&] {
    if (capacity.slot_ref != slot ||
        capacity.admission_disposition.index() != 0 || capacity.capacity < 0 ||
        cmp(span.from, span.to) >= 0)
      fail("InvalidMarketServiceCapacity");
    Integer used = 0;
    for (const auto *c : claims) {
      if (!c || c->slot_ref != slot || c->units <= 0 ||
          cmp(c->from, span.from) > 0 || cmp(span.to, c->until) > 0 ||
          used > std::numeric_limits<Integer>::max() - c->units)
        fail("InvalidMarketServiceOccupancy");
      used += c->units;
    }
    if (used > capacity.capacity)
      fail("MarketServiceCapacityExceeded", slot.facility_id);
    return capacity.capacity - used;
  });
}
Status
NativeMarketServiceCapacityRead::validate_retention_and_physical_occupancy(
    const FacilitySlotRef &slot, const MomentSpan &span,
    const std::vector<const FacilityClaim2 *> &claims,
    const FacilityCapacitySource2 &capacity) const {
  auto result = read_additional_normal_capacity(slot, span, claims, capacity);
  return result.ok ? common_success() : Status{false, {}, result.error};
}
Outcome<bool> NativeMarketServiceCapacityRead::is_verified_retained_claim(
    const FacilityClaim2 &c, const MomentSpan &span,
    const FacilityCapacitySource2 &capacity) const {
  return guard<bool>([&] {
    if (c.slot_ref != capacity.slot_ref || cmp(span.from, span.to) >= 0)
      fail("InvalidMarketRetentionQuery", c.claim_id);
    if (c.source_result_ref.kind == "facility_retention")
      fail("MarketServiceCannotRetainBody", c.claim_id);
    return false;
  });
}
Outcome<bool> NativeMarketServiceCapacityRead::source_change_affects_claims(
    const FacilityCapacityChange &c,
    const std::vector<const FacilityClaim2 *> &claims) const {
  return guard<bool>([&] {
    for (const auto *claim : claims) {
      if (!claim)
        fail("NullMarketServiceClaim");
      if (cmp(c.occurred_at, claim->until) < 0)
        return true;
    }
    return false;
  });
}
Outcome<bool> NativeMarketServiceCapacityRead::conflicting_source_visible(
    const ReservationRef &r, const FacilityClaim2 &c) const {
  return x_.originals.market_claim_conflict_visible(r, c);
}
Outcome<StableId> record_market_service_capacity_source_in_candidate(
    CommonStore &common, HouseholdPurchaseState &state,
    const MarketServiceCapacityContext &x, const ActorFoundationRuleRef &d,
    const MarketServiceChangeCause &changed) {
  return guard<StableId>([&] {
    if (&common != &x.common || &state != &x.purchases)
      fail("MarketServiceCandidateMismatch");
    const auto &definition = market_capacity::definition(x, d);
    for (const auto *old : chain(x, definition.slot_ref))
      if (old->change_cause.index() == changed.index() &&
          same_source_ref(cause(old->change_cause), cause(changed))) {
        if (!same(old->definition_ref, d))
          fail("MarketServiceCauseAlreadyApplied");
        return old->source_id;
      }
    auto next = prospective(x, d, changed);
    next.source_id = take(common.next_id("market_service_capacity"));
    take(common.append_fact({next.source_id, "market_service_capacity",
                             next.slot_ref.provider_domain}));
    auto &table = next.slot_ref.provider_domain == "local_economy"
                      ? state.market_service_sources.public_sources
                      : state.market_service_sources.private_sources;
    table.push_back(next);
    NativeMarketServiceCapacityRead read(x);
    take(read.validate_market_source(table.back()));
    return next.source_id;
  });
}
Status validate_market_service_sources(const MarketServiceCapacityContext &x) {
  return guard<Done>([&] {
    NativeMarketServiceCapacityRead read(x);
    std::set<StableId> ids;
    std::set<std::tuple<StableId, StableId, StableId>> definitions;
    std::map<StableId, StableId> physical_owner;
    for (const auto &d : x.purchases.market_service_definitions) {
      if (!definitions
               .emplace(d.definition_ref.id,
                        std::to_string(d.definition_ref.version),
                        d.definition_ref.catalog_digest)
               .second)
        fail("DuplicateMarketDefinition", d.definition_ref.id);
      auto key = d.asset_source_ref.domain + "/" + d.asset_source_ref.kind +
                 "/" + d.asset_source_ref.id;
      auto old = physical_owner.emplace(key, d.slot_ref.provider_domain + "/" +
                                                 d.slot_ref.facility_id);
      if (!old.second && old.first->second != d.slot_ref.provider_domain + "/" +
                                                  d.slot_ref.facility_id)
        fail("DuplicateMarketPhysicalProvider", d.slot_ref.facility_id);
    }
    for (const auto *table :
         {&x.purchases.market_service_sources.public_sources,
          &x.purchases.market_service_sources.private_sources})
      for (const auto &s : *table) {
        if (!ids.insert(s.source_id).second)
          fail("DuplicateMarketCapacitySource", s.source_id);
        take(read.validate_market_source(s));
        order(x, ref(s).source_ref);
      }
    for (const auto &f : x.common.state().facts.rows())
      if (f.second.kind == "market_service_capacity" && !ids.count(f.first))
        fail("OrphanMarketCapacitySource", f.first);
    return Done{};
  });
}
} // namespace hanmo::resident
