#include "economic_original_payments.hpp"
#include "material_aid/material_aid.hpp"
#include "obligations/obligation_sources_read.hpp"
#include "resident/domains/commerce/repair_service/repair_service.hpp"
#include <set>
namespace hanmo::resident {
NativeEconomicOriginalPaymentRead::NativeEconomicOriginalPaymentRead(
    const CommonStore &c, const LocalEconomyState &e,
    const TransportOriginalPaymentReadContext *t,
    const GovernanceWorkExpenseReadContext *g, const MaterialAidReadContext *a,
    const RepairServiceReadContext *r, const EconomicObligationReadContext *o)
    : common_(c), economy_(e), transport_(t), governance_(g), material_aid_(a),
      repair_service_(r), source_only_(o) {}
Outcome<EconomicOriginalPaymentSourceIds>
NativeEconomicOriginalPaymentRead::read_original_payment_source_ids() const {
  EconomicOriginalPaymentSourceIds out;
  auto bind_original =
      [&](const EconomicObligationReadContext *context) -> Status {
    if (!context)
      return common_success();
    if (&context->common != &common_ || &context->economy != &economy_ ||
        !context->original.bound_to(common_, economy_) ||
        (out.source_only && out.source_only != context))
      return common_failure("EconomicSourceOnlyCandidateMismatch", {},
                            ErrorKind::internal);
    out.source_only = context;
    return common_success();
  };
  auto base = bind_original(source_only_);
  if (!base.ok)
    return {false, {}, base.error};
  if (transport_) {
    auto bound = bind_original(transport_->sources.obligations);
    if (!bound.ok)
      return {false, {}, bound.error};
    auto valid = validate_economic_transport_payments(
        common_, economy_, transport_->state, transport_->sources);
    if (!valid.ok)
      return {false, {}, valid.error};
    for (const auto &r : transport_->state.payments)
      if (r.expense.amount) {
        out.obligation_ids.push_back(r.original_obligation_id);
        out.arrear_ids.push_back(r.original_arrear_id);
        out.payment_ids.push_back(r.original_payment_receipt_id);
      }
  }
  if (governance_) {
    auto bound = bind_original(governance_->obligations);
    if (!bound.ok)
      return {false, {}, bound.error};
    if (&governance_->common != &common_ || &governance_->economy != &economy_)
      return {false,
              {},
              common_failure("GovernancePaymentCandidateMismatch").error};
    auto valid = validate_governance_work_expenses(*governance_);
    if (!valid.ok)
      return {false, {}, valid.error};
    for (const auto &r : governance_->state.charges.rows())
      if (r.second.earned_qian) {
        out.obligation_ids.push_back(r.second.original_obligation_id);
        out.arrear_ids.push_back(r.second.original_arrear_id);
        out.payment_ids.insert(out.payment_ids.end(),
                               r.second.original_payment_ids.begin(),
                               r.second.original_payment_ids.end());
      }
  }
  if (material_aid_) {
    const auto &x = *material_aid_;
    auto bound = bind_original(x.obligations);
    if (!bound.ok)
      return {false, {}, bound.error};
    if (&x.common != &common_ || &x.economy != &economy_)
      return {false,
              {},
              common_failure("MaterialAidPaymentCandidateMismatch", {},
                             ErrorKind::internal)
                  .error};
    const auto valid = validate_treaty_material_aid(x);
    if (!valid.ok)
      return {false, {}, valid.error};
    for (const auto &row : x.state.obligations.rows()) {
      const auto &obligation = row.second;
      if (!obligation.frozen_money)
        continue;
      // The full owner validator has already checked exact original
      // IDs/amounts, terms, current-clock bound and actual receiving-source
      // reverse coverage.
      out.obligation_ids.push_back(*obligation.original_money_obligation_id);
      out.arrear_ids.push_back(*obligation.original_money_arrear_id);
      for (const auto &id : obligation.receipt_result_refs) {
        const auto *receipt = x.state.receipts.find(id);
        if (!receipt)
          return {false,
                  {},
                  common_failure("MaterialAidReceiptResultRequired", id,
                                 ErrorKind::internal)
                      .error};
        const auto original =
            x.resources.read_actual_aid_receipt(receipt->actual_receipt_ref);
        if (!original.ok)
          return {false, {}, original.error};
        if (std::holds_alternative<BudgetMoneyQian>(original.value.unit)) {
          auto cash = read_material_aid_actual_cash_payment(
              x, {"local_economy", obligation.id}, receipt->actual_receipt_ref);
          if (!cash.ok)
            return {false, {}, cash.error};
          out.payment_ids.push_back(cash.value.payment->id);
        }
      }
    }
  }
  if (repair_service_) {
    const auto &x = *repair_service_;
    auto bound = bind_original(x.obligations);
    if (!bound.ok)
      return {false, {}, bound.error};
    if (&x.common != &common_ || &x.economy != &economy_)
      return {false,
              {},
              common_failure("RepairServicePaymentCandidateMismatch", {},
                             ErrorKind::internal)
                  .error};
    const auto valid = validate_repair_service_contracts(x);
    if (!valid.ok)
      return {false, {}, valid.error};
    const auto &s = x.items.repair_service_contracts;
    for (const auto &row : s.charges.rows())
      if (row.second.earned_qian) {
        out.obligation_ids.push_back(
            row.second.original_obligation_ref->obligation_id);
        out.arrear_ids.push_back(*row.second.original_arrear_id);
      }
    for (const auto &row : s.payments.rows())
      out.payment_ids.push_back(row.second.actual_payment_ref.id);
  }
  for (const auto *ids :
       {&out.obligation_ids, &out.arrear_ids, &out.payment_ids}) {
    std::set<StableId> seen;
    for (const auto &id : *ids)
      if (id.empty() || !seen.insert(id).second)
        return {
            false,
            {},
            common_failure("DuplicateEconomicOriginalMoneySource", id).error};
  }
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
