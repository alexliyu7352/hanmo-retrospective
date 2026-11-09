#include "native_service_obligation_sources.hpp"
#include "resident/domains/commerce/repair_service/repair_service_internal.hpp"
#include "resident/domains/local_economy/governance_work/governance_work_expense_contract.hpp"
#include "resident/domains/local_economy/governance_work/governance_work_settlement.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
}
void take(Status s) {
  if (!s.ok)
    throw Failure{std::move(s.error)};
}
template <class T> T take(Outcome<T> s) {
  if (!s.ok)
    throw Failure{std::move(s.error)};
  return std::move(s.value);
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
bool same(const SourceRef &a, const SourceRef &b) {
  return same_source_ref(a, b);
}
bool same(const MomentOwnerSource &a, const MomentOwnerSource &b) {
  return a.source_schema == b.source_schema && same(a.source_ref, b.source_ref);
}
std::size_t header(const CommonStore &c, const SourceRef &r) {
  const auto *h = c.state().facts.find(r.id);
  if (!valid_source_ref(r) || !h || h->id != r.id || h->domain != r.domain ||
      h->kind != r.kind)
    fail("OriginalServiceSourceHeaderRequired", r.id);
  std::size_t n = 0;
  for (const auto &v : c.state().facts.rows()) {
    if (&v.second == h)
      return n;
    ++n;
  }
  fail("OriginalServiceSourceOrderMissing", r.id);
}
const GovernanceWorkExpenseTerms &
work_terms(const GovernanceWorkExpenseReadContext &x, const SourceRef &r) {
  const auto *t = x.state.terms.find(r.id);
  if (!t)
    fail("OriginalWorkTermsRequired", r.id);
  return *take(read_governance_work_expense_terms(x, r, t->work_id,
                                                  t->accepted_task_id));
}
MomentOwnerSource work_charge_ref(const StableId &id) {
  return {{"local_economy", "governance_work_charge", id}, 1};
}
} // namespace
NativeServiceObligationOriginalRead::NativeServiceObligationOriginalRead(
    const CommonStore &c, const LocalEconomyState &e,
    const GovernanceWorkExpenseReadContext *w,
    const RepairServiceReadContext *r)
    : common_(c), economy_(e), work_(w), repair_(r) {}
NativeServiceObligationOriginalRead::NativeServiceObligationOriginalRead(
    const GovernanceWorkExpenseReadContext &w,
    const RepairServiceReadContext &r)
    : NativeServiceObligationOriginalRead(w.common, w.economy, &w, &r) {}
bool NativeServiceObligationOriginalRead::bound_to(
    const CommonStore &c, const LocalEconomyState &e) const noexcept {
  return &common_ == &c && &economy_ == &e &&
         (!work_ || (&work_->common == &c && &work_->economy == &e)) &&
         (!repair_ || (&repair_->common == &c && &repair_->economy == &e));
}
Outcome<OriginalEconomicPayable>
NativeServiceObligationOriginalRead::read_original_payable(
    const EconomicObligationOriginalSource &r) const {
  if (const auto *p = std::get_if<OriginalGovernanceWorkCharge>(&r)) {
    if (!work_)
      return {false,
              {},
              common_failure("OriginalGovernanceWorkOwnerRequired",
                             p->original_charge.source_ref.id,
                             ErrorKind::unsupported_capability)
                  .error};
    return read_original_work_charge_for_obligation(*work_, p->original_charge);
  }
  if (const auto *p = std::get_if<OriginalRepairEarnedCharge>(&r)) {
    if (!repair_)
      return {false,
              {},
              common_failure("OriginalRepairServiceOwnerRequired",
                             p->original_charge.source_ref.id,
                             ErrorKind::unsupported_capability)
                  .error};
    return read_original_repair_charge_for_obligation(*repair_,
                                                      p->original_charge);
  }
  return {false,
          {},
          common_failure("OriginalPayableOwnerAdapterRequired", {},
                         ErrorKind::unsupported_capability)
              .error};
}
Outcome<OriginalEconomicAdjustment>
NativeServiceObligationOriginalRead::read_original_adjustment(
    const MomentOwnerSource &r) const {
  return {false,
          {},
          common_failure("OriginalServiceAdjustmentProducerRequired",
                         r.source_ref.id, ErrorKind::unsupported_capability)
              .error};
}
Outcome<OriginalEconomicCashPayment>
NativeServiceObligationOriginalRead::read_original_cash_payment(
    const SourceRef &r) const {
  try {
    if (r.domain != "local_economy" || r.kind != "obligation_payment")
      fail("OriginalEconomicCashSourceRequired", r.id);
    const auto *p = economy_.payments.find(r.id);
    if (!p || p->id != r.id || p->amount <= 0)
      fail("OriginalEconomicCashReceiptRequired", r.id);
    const auto position = header(common_, r);
    if (const auto *v = work_ ? work_->state.payments.find(r.id) : nullptr) {
      if (repair_)
        for (const auto &other :
             repair_->items.repair_service_contracts.payments.rows())
          if (same(other.second.actual_payment_ref, r))
            fail("AmbiguousOriginalServiceCashOwner", r.id);
      const auto *q = work_->state.charges.find(v->charge_id);
      if (!q || v->payment_id != r.id)
        fail("OriginalWorkCashChargeRequired", r.id);
      const auto due = take(read_original_work_charge_for_obligation(
          *work_, work_charge_ref(q->id)));
      std::size_t count = 0;
      for (const auto &id : q->original_payment_ids)
        if (id == r.id)
          ++count;
      if (count != 1 || p->obligation_id != due.obligation_ref.obligation_id ||
          p->arrear_id != due.arrear_ref.id ||
          p->due_tick != due.due_at.half_day_tick ||
          p->tick != v->paid_at.half_day_tick ||
          cmp(v->paid_at, due.due_at) < 0 ||
          position <= header(common_, due.canonical_charge_ref.source_ref))
        fail("OriginalWorkCashBindingMismatch", r.id);
      return {true, {p, v->paid_at, due.canonical_charge_ref}, {}};
    }
    const RepairServicePaymentSource *found = nullptr;
    if (repair_)
      for (const auto &v :
           repair_->items.repair_service_contracts.payments.rows())
        if (same(v.second.actual_payment_ref, r)) {
          if (found)
            fail("DuplicateOriginalRepairCashAdoption", r.id);
          found = &v.second;
        }
    if (found) {
      const auto due = take(read_original_repair_charge_for_obligation(
          *repair_, found->charge_ref));
      const auto *q = repair_->items.repair_service_contracts.charges.find(
          found->charge_ref.source_ref.id);
      const MomentOwnerSource adopted{repair_service_paid_source(found->id), 1};
      std::size_t count = 0;
      for (const auto &id : q->payment_refs)
        if (same(id, adopted.source_ref))
          ++count;
      if (count != 1 || p->obligation_id != due.obligation_ref.obligation_id ||
          p->arrear_id != due.arrear_ref.id ||
          p->due_tick != due.due_at.half_day_tick ||
          p->tick != found->paid_at.half_day_tick ||
          cmp(found->paid_at, due.due_at) < 0 ||
          position <= header(common_, due.canonical_charge_ref.source_ref) ||
          header(common_, adopted.source_ref) <= position)
        fail("OriginalRepairCashBindingMismatch", r.id);
      return {true, {p, found->paid_at, due.canonical_charge_ref}, {}};
    }
    fail("OriginalCashPaymentOwnerAdapterRequired", r.id,
         ErrorKind::unsupported_capability);
  } catch (const Failure &e) {
    return {false, {}, e.error};
  } catch (repair_service_detail::Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
NativeServiceObligationOriginalRead::validate_original_cash_payment_authority(
    const SourceRef &r) const {
  try {
    const auto actual = take(read_original_cash_payment(r));
    if (const auto *v = work_ ? work_->state.payments.find(r.id) : nullptr) {
      const auto *q = work_->state.charges.find(v->charge_id);
      const auto &t = work_terms(*work_, q->expense_terms_ref);
      take(work_->original.validate_work_fee_payment(
          t, actual.payment->amount, actual.actual_at, r, true));
      return common_success();
    }
    if (repair_)
      for (const auto &v :
           repair_->items.repair_service_contracts.payments.rows())
        if (same(v.second.actual_payment_ref, r)) {
          const auto *q = repair_->items.repair_service_contracts.charges.find(
              v.second.charge_ref.source_ref.id);
          const auto &t =
              repair_service_detail::contract(*repair_, q->contract_ref);
          take(repair_->original.validate_original_repair_payment(
              t, {repair_service_paid_source(v.first), 1},
              RepairServicePaymentUse::earned_charge, actual.payment->amount,
              actual.actual_at));
          return common_success();
        }
    fail("OriginalCashPaymentOwnerAdapterRequired", r.id,
         ErrorKind::unsupported_capability);
  } catch (const Failure &e) {
    return {false, {}, e.error};
  } catch (repair_service_detail::Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<ObligationResponsibilityEpoch>
NativeServiceObligationOriginalRead::read_original_responsibility_genesis(
    const EconomicObligationOriginalSource &r) const {
  try {
    const auto due = take(read_original_payable(r));
    ObligationResponsibilityEpoch out;
    out.obligation_ref = due.obligation_ref;
    out.original_source_ref = due.canonical_charge_ref;
    out.change_source = OriginalResponsibilitySource{due.canonical_charge_ref};
    out.accepted_arrear_refs = {due.arrear_ref};
    out.effective_at = due.established_at;
    out.authority_ref = due.canonical_charge_ref.source_ref.id;
    if (const auto *p = std::get_if<OriginalGovernanceWorkCharge>(&r)) {
      const auto *q =
          work_->state.charges.find(p->original_charge.source_ref.id);
      const auto &t = work_terms(*work_, q->expense_terms_ref);
      out.debtor = t.frozen.approval.contract_debtor;
      out.creditor = t.frozen.contract_creditor;
      out.source_wallet_ref = t.frozen.approval.payer_wallet_id;
      out.destination_wallet_ref = t.frozen.payee_wallet_id;
      out.executor_authorization_ref = {t.frozen.approval.approval_ref,
                                        t.frozen.approval.source_schema};
      out.receiver_source_ref = {t.task_acceptance_source, 1};
      out.consent_refs = {{t.frozen.approval.offered_task_ref, 1},
                          {t.task_acceptance_source, 1}};
    } else if (const auto *p = std::get_if<OriginalRepairEarnedCharge>(&r)) {
      const auto *q = repair_->items.repair_service_contracts.charges.find(
          p->original_charge.source_ref.id);
      const auto &t =
          repair_service_detail::contract(*repair_, q->contract_ref);
      const auto parties =
          take(repair_->original.read_original_repair_service_parties(t));
      out.debtor = parties.debtor;
      out.creditor = parties.creditor;
      out.source_wallet_ref = t.terms.payer_wallet_id;
      out.destination_wallet_ref = t.terms.payee_wallet_id;
      out.executor_authorization_ref = t.terms.payer_authority_source;
      out.receiver_source_ref = t.terms.provider_acceptance_source;
      out.consent_refs = {t.terms.customer_acceptance_source,
                          t.terms.provider_acceptance_source};
    } else
      fail("OriginalResponsibilityGenesisAdapterRequired",
           due.obligation_ref.obligation_id, ErrorKind::unsupported_capability);
    // These are immutable accepted source refs. Mutable-input versions, when a
    // later change actually uses them, belong to that real change's own proof.
    return {true, std::move(out), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  } catch (repair_service_detail::Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
NativeServiceObligationOriginalRead::validate_original_responsibility_epoch(
    const ObligationResponsibilityEpoch &e) const {
  try {
    if (e.epoch || e.previous_epoch_ref || e.previous_execution_event_ref ||
        !std::holds_alternative<OriginalResponsibilitySource>(e.change_source))
      fail("OriginalResponsibilityChangeAdapterRequired",
           e.obligation_ref.obligation_id, ErrorKind::unsupported_capability);
    const auto &ref = e.original_source_ref;
    EconomicObligationOriginalSource source;
    if (ref.source_ref.domain == "local_economy" &&
        ref.source_ref.kind == "governance_work_charge")
      source = OriginalGovernanceWorkCharge{ref};
    else if (ref.source_ref.domain == "commerce" &&
             ref.source_ref.kind == "repair_service_charge")
      source = OriginalRepairEarnedCharge{ref};
    else
      fail("OriginalResponsibilityGenesisAdapterRequired", ref.source_ref.id,
           ErrorKind::unsupported_capability);
    const auto actual = take(read_original_responsibility_genesis(source));
    return validate_original_obligation_genesis(e, actual);
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
