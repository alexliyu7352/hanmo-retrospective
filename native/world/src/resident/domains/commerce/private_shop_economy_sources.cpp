// LE 早位核原私约及原付款的精确来源；不回调 Shop/法律/Task
// 全验，末位仍须完整经营历史。
#include "daily_rent/private_shop_daily_rent.hpp"
#include "private_shop_asset_responsibilities.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <map>
#include <set>
namespace hanmo::resident {
Outcome<PrivateShopEconomySourceIds> read_private_shop_economy_source_ids(
    const PrivateShopResponsibilitySourceContext &x) {
  auto fail = [](const StableId &code,
                 const StableId &id =
                     StableId{}) -> Outcome<PrivateShopEconomySourceIds> {
    return {false, {}, common_failure(code, id).error};
  };
  auto structural = validate_private_shop_public_responsibility_sources(
      x.common, x.economy, x.economy_catalog, x.state, x.catalog, x.phase,
      x.read_boundary3, x.moment_clock3);
  if (!structural.ok)
    return {false, {}, structural.error};
  if (bool(x.daily_rent_catalog) != bool(x.daily_rent))
    return fail("PrivateShopRentPairRequired");
  if (x.daily_rent) {
    auto rent = validate_private_shop_daily_rent_sources(
        x.common, x.economy, x.state, *x.daily_rent_catalog, *x.daily_rent,
        x.phase);
    if (!rent.ok)
      return {false, {}, rent.error};
  }
  const auto &c = x.common;
  const auto &e = x.economy;
  auto observed = read_private_shop_responsibility_observed_tick(x);
  if (!observed.ok)
    return {false, {}, observed.error};
  const Integer limit = observed.value;
  PrivateShopEconomySourceIds out;
  std::set<StableId> obligations, dues, payments;
  std::map<StableId, Integer> remainders;
  std::map<StableId, StableId> last;
  auto head = [&](const StableId &id, const StableId &prefix,
                  const StableId &kind, const StableId &domain) {
    const auto *h = c.state().facts.find(id);
    auto n = organization_instance_sequence(id, prefix);
    return n && *n < c.state().next_instance_id && h && h->id == id &&
           h->kind == kind && h->domain == domain;
  };
  auto before = [&](const StableId &a, const StableId &b) {
    const auto &order = c.state().facts.order();
    auto left = std::find(order.begin(), order.end(), a),
         right = std::find(order.begin(), order.end(), b);
    return left != order.end() && right != order.end() && left < right;
  };
  for (const auto &row : x.state.shops.rows())
    if (!obligations.insert(row.second.rent_obligation_id).second)
      return fail("PrivateShopDuplicateEconomyObligation", row.first);
  for (const auto &row : x.state.staff.contracts.rows())
    if (!obligations.insert(row.second.obligation_id).second)
      return fail("PrivateShopDuplicateEconomyObligation", row.first);
  // 业务源、LE 应付、LE
  // 实付保持同一全局分配顺序；不接受只有相同金额的替代付款。
  auto receipt = [&](const StableId &source, const StableId &due_id,
                     const StableId &paid_id, const StableId &obligation_id,
                     Integer tick, Integer amount) -> Status {
    const auto *o = e.obligations.find(obligation_id);
    const auto *a = e.arrears.find(due_id);
    const auto *p = e.payments.find(paid_id);
    auto d = organization_instance_sequence(due_id, "local_due"),
         n = organization_instance_sequence(paid_id, "local_payment");
    if (!o || !a || !p || a->id != due_id || p->id != paid_id || !d || !n ||
        *d == INT64_MAX || *n != *d + 1 ||
        !head(due_id, "local_due",
              o->kind == ObligationKind::wage ? "wage_due"
                                              : "contract_payment_due",
              "local_economy") ||
        !head(paid_id, "local_payment", "obligation_payment",
              "local_economy") ||
        !before(source, due_id) || !before(due_id, paid_id) ||
        !dues.insert(due_id).second || !payments.insert(paid_id).second ||
        tick < 0 || tick > limit || amount <= 0 ||
        a->obligation_id != obligation_id || a->due_tick != tick ||
        a->original_due != amount || a->paid_amount != amount ||
        p->arrear_id != due_id || p->obligation_id != obligation_id ||
        p->from_wallet_id != o->source_wallet_or_inventory ||
        p->to_wallet_id != o->destination || p->tick != tick ||
        p->due_tick != tick || p->amount != amount ||
        p->authority_ref != o->authority_ref)
      return common_failure("PrivateShopEconomyReceiptMismatch", source);
    last[obligation_id] = paid_id;
    return common_success();
  };
  std::size_t operations = 0, services = 0;
  for (const auto &actual : c.state().facts.rows()) {
    // 日租须先通过原 LE 完整来源；未付应期不冒最后实付，最后回执仍只指原
    // payment。
    if (x.daily_rent) {
      if (const auto *a = x.daily_rent->assessments.find(actual.first))
        if (!dues.insert(a->arrear_id).second)
          return fail("PrivateShopDuplicateEconomyDue", a->id);
      if (const auto *p = x.daily_rent->payments.find(actual.first)) {
        const auto *receipt = e.payments.find(p->local_payment_id);
        if (!receipt || !payments.insert(receipt->id).second)
          return fail("PrivateShopDuplicateEconomyPayment", p->id);
        last[receipt->obligation_id] = receipt->id;
      }
    }
    if (const auto *o = x.state.operations.find(actual.first)) {
      const auto *shop = x.state.shops.find(o->shop_id);
      auto n = organization_instance_sequence(o->id, "action"),
           d = organization_instance_sequence(o->rent_arrear_id, "local_due");
      if (o->id != actual.first || !shop ||
          !head(o->id, "action", "private_shop_operating_started",
                private_shop_module) ||
          !n || (!x.daily_rent && (!d || *n == INT64_MAX || *d != *n + 1)) ||
          (x.daily_rent &&
           (!o->rent_arrear_id.empty() || !o->rent_payment_id.empty())) ||
          !before(shop->id, o->id) || o->start_tick < shop->opened_tick)
        return fail("PrivateShopEconomyOperationSourceMismatch", actual.first);
      if (!x.daily_rent) {
        auto valid = receipt(o->id, o->rent_arrear_id, o->rent_payment_id,
                             shop->rent_obligation_id, o->start_tick,
                             shop->terms.daily_rent);
        if (!valid.ok)
          return {false, {}, valid.error};
      }
      ++operations;
    }
    if (const auto *s = x.state.staff.services.find(actual.first)) {
      const auto *contract = x.state.staff.contracts.find(s->contract_id);
      const auto *o = x.state.operations.find(s->operation_id);
      auto n = organization_instance_sequence(s->id,
                                              "private_shop_staff_service"),
           d = organization_instance_sequence(s->arrear_id, "local_due");
      const Integer prior = remainders[s->contract_id],
                    amount = s->closing_fraction ? 1 : (3 + prior) / 2,
                    next = s->closing_fraction ? 0 : (3 + prior) % 2;
      if (s->id != actual.first || !contract || !o ||
          o->staff_contract_id != contract->id ||
          o->shop_id != contract->terms.shop_id ||
          o->actor_id != contract->terms.worker_actor_id ||
          o->item_id != contract->terms.service_item_id ||
          !head(s->id, "private_shop_staff_service",
                "private_shop_staff_served", private_shop_module) ||
          !n || !d || *n == INT64_MAX || *d != *n + 1 ||
          !before(contract->id, o->id) || !before(o->id, s->id) ||
          s->tick < o->start_tick || s->tick > contract->through_tick ||
          s->attendance_index < 1 ||
          s->attendance_index > Integer(o->attendance_ticks.size()) ||
          s->remainder_before != prior || s->remainder_after != next ||
          s->amount != amount ||
          (!contract->terminal_fact_id.empty() &&
           !before(s->payment_id, contract->terminal_fact_id)))
        return fail("PrivateShopEconomyWageSourceMismatch", actual.first);
      if (s->closing_fraction) {
        const auto *r = x.state.operating_results.find(o->result_fact_id);
        if (prior != 1 || !r || r->id != o->result_fact_id ||
            r->operation_id != o->id || r->completed ||
            o->attendance_ticks.size() != 1 || s->attendance_index != 1 ||
            r->tick != s->tick ||
            !head(r->id, "private_shop_day",
                  "private_shop_operating_interrupted", private_shop_module) ||
            !before(s->payment_id, r->id))
          return fail("PrivateShopEconomyFractionSourceMismatch", s->id);
        out.fractional_arrear_ids.push_back(s->arrear_id);
      } else if (o->attendance_ticks[std::size_t(s->attendance_index - 1)] !=
                 s->tick)
        return fail("PrivateShopEconomyAttendanceMismatch", s->id);
      auto valid = receipt(s->id, s->arrear_id, s->payment_id,
                           contract->obligation_id, s->tick, s->amount);
      if (!valid.ok)
        return {false, {}, valid.error};
      remainders[s->contract_id] = next;
      ++services;
    }
  }
  if (operations != x.state.operations.size() ||
      services != x.state.staff.services.size())
    return fail("PrivateShopEconomySourceCoverageMismatch");
  for (const auto &id : obligations) {
    const auto *o = e.obligations.find(id);
    if (!o || o->last_receipt_id != last[id])
      return fail("PrivateShopEconomyLastReceiptMismatch", id);
    out.obligation_ids.push_back(id);
  }
  for (const auto &row : x.state.staff.contracts.rows()) {
    const auto *remainder =
        e.obligation_remainders.find(row.second.obligation_id);
    if (!remainder || *remainder != remainders[row.first])
      return fail("PrivateShopEconomyRemainderMismatch", row.first);
  }
  for (const auto &row : e.arrears.rows())
    if (obligations.count(row.second.obligation_id) && !dues.count(row.first))
      return fail("PrivateShopUnprovenEconomyDue", row.first);
  for (const auto &row : e.payments.rows())
    if (obligations.count(row.second.obligation_id) &&
        !payments.count(row.first))
      return fail("PrivateShopUnprovenEconomyPayment", row.first);
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
