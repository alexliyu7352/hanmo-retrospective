#include "political_group_toll.hpp"
#include "resident/kernel/world_stage_observation.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
// 来源损坏/未接真实种类按原错误返回，不转为零收费或业务放行。
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::rule) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
  return std::move(r.value);
}
void take(Status r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
}
int compare(const GroupPoliticalPassageEntryContext &x, const WorldMoment &a,
            const WorldMoment &b) {
  return take(compare_world_moments(a, b, x.arithmetic));
}
// 原carrier完整联合身份含运输主体/组成版；不只比较一个显示key。
bool carrier(const SpatialCarrierRef &a, const SpatialCarrierRef &b) {
  if (a.index() != b.index() ||
      spatial_carrier_key(a) != spatial_carrier_key(b))
    return false;
  if (const auto *p = std::get_if<TransportUnitCarrierRef>(&a)) {
    const auto &q = std::get<TransportUnitCarrierRef>(b);
    return p->composition_revision == q.composition_revision &&
           p->transport_unit_ref.subject.kind ==
               q.transport_unit_ref.subject.kind &&
           p->transport_unit_ref.subject.id == q.transport_unit_ref.subject.id;
  }
  return true;
}
bool execution(const MovementExecutionRef &a, const MovementExecutionRef &b) {
  const auto *p = std::get_if<GroupMovementExecution>(&a);
  const auto *q = std::get_if<GroupMovementExecution>(&b);
  return p && q && p->journey_id == q->journey_id &&
         p->segment_ordinal == q->segment_ordinal &&
         carrier(p->carrier, q->carrier);
}
// 逐字段比较原通行边界，避免与 boundary 命名空间重名。
bool same_passage_boundary(const PassageBoundaryRef &a,
                           const PassageBoundaryRef &b) {
  return a.kind == b.kind && a.id == b.id && a.map_id == b.map_id &&
         a.from_site_id == b.from_site_id && a.to_site_id == b.to_site_id;
}
// 同M因果仅取原Common追加序。
std::size_t position(const CommonStore &c, const SourceRef &ref) {
  const auto *head = c.state().facts.find(ref.id);
  const auto &order = c.state().facts.order();
  const auto it = std::find(order.begin(), order.end(), ref.id);
  if (!head || head->domain != ref.domain || head->kind != ref.kind ||
      it == order.end())
    fail("PoliticalTollActualSourceRequired", ref.id);
  return static_cast<std::size_t>(it - order.begin());
}
// 一个已采用Decision只映射一个原entry单位；不采用今天prefix的后来unit。
const MovementUnitState &
original_unit(const GroupPoliticalPassageEntryContext &x,
              const PoliticalPassageDecisionSource &d) {
  if (d.entry_ordinal < 1)
    fail("PoliticalTollEntryOrdinalRequired", d.id);
  const MovementUnitState *result = nullptr;
  for (const auto &row : x.movement.units.rows())
    if (execution(row.second.execution, d.execution) &&
        row.second.ordinal == d.entry_ordinal - 1) {
      if (result || row.first != row.second.id)
        fail("PoliticalTollUnitNotUnique", d.id);
      result = &row.second;
    }
  if (!result)
    fail("PoliticalTollOriginalUnitRequired", d.id);
  const auto *terms = x.executions.unit_terms.find(result->id);
  const auto *group = std::get_if<GroupMovementExecution>(&d.execution);
  if (!group || !terms || terms->unit_ref != result->id ||
      terms->journey_id != group->journey_id)
    fail("PoliticalTollOriginalUnitTermsRequired", d.id);
  return *result;
}
// 原决定/捕获名册/规则历史单一来源。此处不调用全crossing历史，以供合法同栈前置采用。
const PoliticalPassageDecisionSource &
source(const GroupPoliticalPassageEntryContext &x,
       const PoliticalPassageHistoryRead &history, const SourceRef &ref) {
  const auto *d =
      take(read_political_passage_decision(x.common, x.decisions, ref));
  const auto *group = std::get_if<GroupMovementExecution>(&d->execution);
  if (!group || d->decision.source_schema_version != 2 ||
      d->decision.admission != PassageAdmission::allowed ||
      !carrier(group->carrier, d->carrier))
    fail("PoliticalGroupTollDecision2Required", ref.id,
         ErrorKind::unsupported_capability);
  const auto *journey = x.carriers.journeys.find(group->journey_id);
  // 历史决定只比较其真实 Common 前缀；后来同旅供养不能回写旧决定或借今日版本否定它。
  if (!journey || !carrier(journey->carrier, d->carrier) ||
      take(read_group_composition_revision_before(
          x.common, x.executions, group->journey_id, &ref)) !=
          d->composition_revision ||
      compare(x, journey->started_at, d->occurred_at) > 0 ||
      compare(x, d->occurred_at,
              take(x.clock.read_organization_authority_now())) > 0)
    fail("PoliticalGroupTollJourneyMismatch", ref.id);
  const auto *captured = take(read_historical_transport_entry_manifest(
      x.common, x.carriers, x.movement, x.executions, x.manifest_history,
      d->execution, d->entry_ordinal, d->decision.boundary,
      d->manifest_revision, d->composition_revision, x.arithmetic));
  if (!execution(captured->execution, d->execution) ||
      !carrier(captured->manifest.carrier, d->carrier) ||
      compare(x, captured->occurred_at, d->occurred_at) ||
      position(x.common, {"travel", "transport_entry_manifest",
                          captured->id}) >= position(x.common, ref))
    fail("PoliticalTollManifestCutMismatch", ref.id);
  original_unit(x, *d);
  take(history.validate_historical_decision(*d));
  return *d;
}
// 原采用阶段真实clock必须等于from；不暴露任意at覆盖或借未来正片冒收费前提。
const PoliticalPassageDecisionSource &
at_adoption(const GroupPoliticalPassageEntryContext &x,
            const PoliticalPassageHistoryRead &history, const SourceRef &ref,
            const GroupMovementJourney &journey, const WorldMoment &at) {
  const auto &d = source(x, history, ref);
  const auto &group = std::get<GroupMovementExecution>(d.execution);
  if (x.carriers.journeys.find(journey.journey_id) != &journey ||
      journey.journey_id != group.journey_id || compare(x, at, d.occurred_at) ||
      compare(x, at, take(x.clock.read_organization_authority_now())))
    fail("PoliticalTollActualAdoptionMismatch", ref.id);
  auto actual = take(read_actual_group_passage_entry(
      x.carriers, x.movement, x.travel_catalog, group, d.decision.boundary, at,
      x.arithmetic));
  const auto &unit = original_unit(x, d);
  if (actual.unit_ref != unit.id || actual.entry_ordinal != d.entry_ordinal ||
      actual.manifest_revision != d.manifest_revision ||
      actual.composition_revision != d.composition_revision ||
      compare(x, actual.occurred_at, d.occurred_at))
    fail("PoliticalTollActualEntryMismatch", ref.id);
  return d;
}
// Common原钱包只有qian；不把其它币种数值无声视为钱，也不将军旅改走商业费用。
ActualTransportExpense expense(const GroupPoliticalPassageEntryContext &x,
                               const PoliticalPassageDecisionSource &d) {
  const auto &group = std::get<GroupMovementExecution>(d.execution);
  const auto *journey = x.carriers.journeys.find(group.journey_id);
  if (!journey || !read_commercial_group_funding(journey->expense_authority))
    fail("MilitaryTollOriginalExpenseRequired", d.id,
         ErrorKind::unsupported_capability);
  if (!d.decision.toll)
    fail("PoliticalDecisionHasNoToll", d.id);
  const auto &toll = *d.decision.toll;
  if (toll.currency_id != "qian")
    fail("PoliticalTollCurrencyUnsupported", d.id,
         ErrorKind::unsupported_capability);
  if (toll.amount < 0 || toll.payee_wallet_id.empty() ||
      !valid_source_ref(toll.tariff_source_ref) ||
      d.decision.boundary.to_site_id.empty() ||
      !x.common.wallet(toll.payee_wallet_id))
    fail("PoliticalTollOriginalChargeInvalid", d.id);
  const auto &unit = original_unit(x, d);
  return {{"politics", "political_passage_decided", d.id},
          2,
          group.journey_id,
          unit.id,
          toll.tariff_source_ref.id,
          toll.payee_wallet_id,
          d.decision.boundary.to_site_id,
          TransportExpensePurpose::toll,
          toll.amount,
          d.occurred_at};
}
// 原付款只交叉核一次绑定与真实Common先序；不回调S5费用reader形成循环。
void payment(const GroupPoliticalPassageEntryContext &x,
             const EconomicTransportExpenseState &payments,
             const PoliticalPassageDecisionSource &d,
             const BoundaryCrossingReceipt &crossing, bool stored) {
  const SourceRef ref{"politics", "political_passage_decided", d.id};
  if (!d.decision.toll) {
    if (!crossing.payment_refs.empty())
      fail("PoliticalUntolledCrossingHasPayment", crossing.receipt_id);
    for (const auto &paid : payments.payments)
      if (same_source_ref(paid.request.actual_movement_or_handling_source, ref))
        fail("PoliticalUntolledDecisionHasPayment", d.id);
    return;
  }
  if (crossing.payment_refs.size() != 1)
    fail("PoliticalTollPaymentCoverageRequired", crossing.receipt_id);
  const auto &payment_ref = crossing.payment_refs.front();
  if (payment_ref.domain != "local_economy" ||
      payment_ref.kind != "transport_expense_paid")
    fail("PoliticalTollActualPaymentRequired", crossing.receipt_id);
  const auto *paid = take(read_transport_payment(payments, ref, 2));
  const auto expected = expense(x, d);
  const auto &actual = paid->expense;
  if (paid->id != payment_ref.id || !same_source_ref(actual.source_ref, ref) ||
      actual.source_schema != 2 || actual.journey_id != expected.journey_id ||
      actual.segment_or_handling_id != expected.segment_or_handling_id ||
      actual.cost_rule_id != expected.cost_rule_id ||
      actual.payee_wallet_id != expected.payee_wallet_id ||
      actual.destination_id != expected.destination_id ||
      actual.purpose != expected.purpose || actual.amount != expected.amount ||
      compare(x, actual.occurred_at, expected.occurred_at))
    fail("PoliticalTollActualPaymentMismatch", crossing.receipt_id);
  const auto *journey = x.carriers.journeys.find(expected.journey_id);
  const auto *funding =
      journey ? read_commercial_group_funding(journey->expense_authority)
              : nullptr;
  if (!funding || paid->payer_wallet_id != journey->payer_wallet_id ||
      !same_transport_expense_authority(*funding, paid->request.authority))
    fail("PoliticalTollActualPayerMismatch", crossing.receipt_id);
  const auto paid_at = position(x.common, payment_ref);
  if (position(x.common, ref) >= paid_at ||
      (stored && paid_at >= position(x.common, {"travel", "boundary_crossing",
                                                crossing.receipt_id})))
    fail("PoliticalTollPaymentCausalOrderMismatch", crossing.receipt_id);
}
} // namespace
Outcome<ActualTransportExpense> read_group_political_toll_at_adoption(
    const GroupPoliticalPassageEntryContext &x,
    const PoliticalPassageHistoryRead &history, const SourceRef &ref,
    const GroupMovementJourney &journey, const WorldMoment &at) {
  try {
    return {true, expense(x, at_adoption(x, history, ref, journey, at)), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Outcome<ActualTransportExpense>
read_original_group_political_toll(const GroupPoliticalPassageEntryContext &x,
                                   const PoliticalPassageHistoryRead &history,
                                   const SourceRef &ref, Integer schema) {
  try {
    if (schema != 2)
      fail("PoliticalGroupTollSource2Required", ref.id,
           ErrorKind::unsupported_capability);
    const auto &d = source(x, history, ref);
    NativeGroupPoliticalPassageEntryRead entries(x);
    take(entries.validate_historical_entry(d));
    return {true, expense(x, d), {}};
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Status validate_pending_group_political_crossing(
    const GroupPoliticalPassageEntryContext &x,
    const PoliticalPassageHistoryRead &history,
    const EconomicTransportExpenseState &payments,
    const BoundaryCrossingReceipt &crossing) {
  try {
    if (crossing.receipt_id.empty() ||
        x.carriers.crossings.find(crossing.receipt_id) ||
        x.common.state().facts.find(crossing.receipt_id) ||
        crossing.safe_exit_binding_ref)
      fail("PoliticalPendingCrossingRequired", crossing.receipt_id);
    const auto *group =
        std::get_if<GroupMovementExecution>(&crossing.execution);
    const auto *journey =
        group ? x.carriers.journeys.find(group->journey_id) : nullptr;
    if (!journey)
      fail("PoliticalPendingGroupJourneyRequired", crossing.receipt_id);
    const auto &d = at_adoption(x, history, crossing.decision_source_ref,
                                *journey, crossing.entry_at);
    if (!execution(crossing.execution, d.execution) ||
        !carrier(crossing.carrier, d.carrier) ||
        crossing.composition_revision != d.composition_revision ||
        crossing.manifest_revision != d.manifest_revision ||
        crossing.entry_ordinal != d.entry_ordinal ||
        !same_passage_boundary(crossing.boundary, d.decision.boundary))
      fail("PoliticalPendingCrossingMismatch", crossing.receipt_id);
    payment(x, payments, d, crossing, false);
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Status validate_original_group_political_crossing(
    const GroupPoliticalPassageEntryContext &x,
    const PoliticalPassageHistoryRead &history,
    const EconomicTransportExpenseState &payments,
    const BoundaryCrossingReceipt &crossing) {
  try {
    if (x.carriers.crossings.find(crossing.receipt_id) != &crossing)
      fail("PoliticalStoredCrossingRequired", crossing.receipt_id);
    world_stage_observation::observe_node({world_stage_observation::NodeMetric::edge_traversal, "politics.stored_crossing.decision", &x.common.state(), nullptr, {"fact", "travel", "boundary_crossing", crossing.receipt_id, 1}, {"fact", crossing.decision_source_ref.domain, crossing.decision_source_ref.kind, crossing.decision_source_ref.id, 0}, -1});
    const auto &d = source(x, history, crossing.decision_source_ref);
    NativeGroupPoliticalPassageEntryRead entries(x);
    take(entries.validate_historical_entry(d));
    if (!execution(crossing.execution, d.execution) ||
        !carrier(crossing.carrier, d.carrier) ||
        crossing.composition_revision != d.composition_revision ||
        crossing.manifest_revision != d.manifest_revision ||
        crossing.entry_ordinal != d.entry_ordinal ||
        !same_passage_boundary(crossing.boundary, d.decision.boundary) ||
        compare(x, crossing.entry_at, d.occurred_at))
      fail("PoliticalStoredCrossingMismatch", crossing.receipt_id);
    payment(x, payments, d, crossing, true);
    world_stage_observation::observe_node({world_stage_observation::NodeMetric::resolved_entry, "politics.stored_crossing.validated", &x.common.state(), nullptr, {"fact", "travel", "boundary_crossing", crossing.receipt_id, 1}, {}, -1, crossing.entry_at.half_day_tick, true});
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
Outcome<ActualTransportExpense>
NativeHistoricalGroupPoliticalTollRead::read_actual_transport_expense(
    const SourceRef &ref, Integer schema) const {
  return read_original_group_political_toll(x_, history_, ref, schema);
}
Outcome<WorldMoment>
NativeHistoricalGroupPoliticalTollRead::current_transport_expense_moment()
    const {
  return x_.clock.read_organization_authority_now();
}
Status
NativeHistoricalGroupPoliticalTollRead::validate_transport_expense_authority(
    const SourceRef &ref, Integer schema,
    const TransportExpenseAuthority &authority) const {
  try {
    const auto cost = take(read_actual_transport_expense(ref, schema));
    const auto *journey = x_.carriers.journeys.find(cost.journey_id);
    const auto *funding =
        journey ? read_commercial_group_funding(journey->expense_authority)
                : nullptr;
    if (!funding || !same_transport_expense_authority(*funding, authority))
      fail("PoliticalTollAcceptedFundingMismatch", ref.id);
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
} // namespace hanmo::resident
