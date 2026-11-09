#include "economic_payment_world_scope.hpp"
#include "obligations/native_service_obligation_sources.hpp"
#include "resident/domains/commerce/repair_service/repair_service.hpp"
#include "resident/domains/travel/transport_payment_physical.hpp"
#include "resident/kernel/native_economic_facility_roots.hpp"
#include "resident/kernel/native_economic_obligation_sources.hpp"
#include "resident/kernel/native_field_passage.hpp"
#include "resident/kernel/native_item_craft_state.hpp"
#include "resident/kernel/native_political_world_scope.hpp"
#include "resident/kernel/native_transport_catalog.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
namespace {
bool aid_kind(std::string_view kind) {
  return kind == "aid_schedule_accepted" ||
         kind == "material_aid_established" ||
         kind == "material_aid_resources_issued" ||
         kind == "material_aid_received" ||
         kind == "aid_future_holds_released" || kind == "material_aid_late";
}
Status required(const char *code, const StableId &id = {}) {
  return common_failure(code, id, ErrorKind::unsupported_capability);
}
Status same_now(const WorldMoment &at, const CurrentMomentRead &clock) {
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto same = compare_world_moments(at, now.value);
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  return same.value ? common_failure("EconomicPaymentOriginalClockMismatch")
                    : common_success();
}
Status absent_aid(const WorldBundle &w) {
  // P1的原finite门明确排除terms轴；没有独立Aid0版本，不从当前空表作选择。
  for (const auto &m : w.catalog->identity.module_versions)
    if (m.first == "political_treaty_terms")
      return required("EconomicPaymentOriginalAidOwnerRequired", m.first);
  for (const auto &row : w.common.state().facts.rows())
    if (aid_kind(row.second.kind))
      return common_failure("EconomicPaymentUnselectedAidSource", row.first);
  const auto aid_authority = [&](const StableId &id) {
    const auto *h = w.common.state().facts.find(id);
    return h && aid_kind(h->kind);
  };
  for (const auto &row : w.local_economy.obligations.rows())
    if (aid_authority(row.second.authority_ref))
      return common_failure("EconomicPaymentUnselectedAidObligation",
                            row.first);
  for (const auto &row : w.local_economy.arrears.rows()) {
    const auto *o = w.local_economy.obligations.find(row.second.obligation_id);
    if (o && aid_authority(o->authority_ref))
      return common_failure("EconomicPaymentUnselectedAidArrear", row.first);
  }
  for (const auto &row : w.local_economy.payments.rows()) {
    const auto *o = w.local_economy.obligations.find(row.second.obligation_id);
    if (aid_authority(row.second.authority_ref) ||
        (o && aid_authority(o->authority_ref)))
      return common_failure("EconomicPaymentUnselectedAidPayment", row.first);
  }
  if (w.economic_obligation_sources) {
    const auto &p = *w.economic_obligation_sources;
    for (const auto &row : p.obligation_sources)
      if (std::holds_alternative<OriginalMaterialAidPayable>(
              row.original_source))
        return common_failure("EconomicPaymentUnselectedAidBinding",
                              row.obligation_ref.obligation_id);
    for (const auto &row : p.payable_sources)
      if (std::holds_alternative<OriginalMaterialAidPayable>(
              row.original_source) ||
          aid_kind(row.canonical_charge_ref.source_ref.kind))
        return common_failure("EconomicPaymentUnselectedAidBinding",
                              row.original_arrear_ref.id);
    for (const auto &row : p.adjustments)
      if (aid_kind(row.original_adjustment_ref.source_ref.kind))
        return common_failure("EconomicPaymentUnselectedAidBinding",
                              row.original_arrear_ref.id);
    for (const auto &row : p.responsibility_epochs)
      if (aid_kind(row.original_source_ref.source_ref.kind) ||
          aid_kind(row.executor_authorization_ref.source_ref.kind) ||
          aid_kind(row.receiver_source_ref.source_ref.kind))
        return common_failure("EconomicPaymentUnselectedAidBinding",
                              row.obligation_ref.obligation_id);
    for (const auto &row : p.payment_responsibility_bindings)
      if (aid_kind(row.original_payment_trigger_ref.source_ref.kind))
        return common_failure("EconomicPaymentUnselectedAidBinding",
                              row.original_payment_ref.id);
    for (const auto &row : p.legacy_responsibility_prefixes)
      if (aid_kind(row.original_responsibility_source.source_ref.kind) ||
          aid_kind(row.actual_cutover_source.source_ref.kind))
        return common_failure("EconomicPaymentUnselectedAidBinding",
                              row.obligation_ref.obligation_id);
  }
  return common_success();
}
Status roots(const WorldBundle &w, const NativeEconomicPaymentWorldSources &s,
             const EconomicObligationReadContext *owned = nullptr,
             bool constructing = false) {
  if (!w.catalog || !w.catalog->local_economy || !s.political ||
      !s.political->bound_to(w))
    return required("EconomicPaymentOriginalP1ScopeRequired");
  auto at = w.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  auto same = compare_world_moments(at.value, s.political->actual_at());
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  const auto p1 = s.political->political_read_lease().bindings();
  if (same.value || &p1.opening.common != &w.common ||
      &p1.opening.economy != &w.local_economy)
    return common_failure("EconomicPaymentP1SnapshotMismatch");
  auto checked = same_now(at.value, p1.opening.clock);
  if (!checked.ok)
    return checked;
  checked = absent_aid(w);
  if (!checked.ok)
    return checked;
  checked = validate_native_economic_facility_layout(w);
  if (!checked.ok)
    return checked;
  checked = validate_native_economic_facility_headers(
      w.common, *w.catalog, &w.local_economy,
      w.economic_facility_capacity_sources
          ? &*w.economic_facility_capacity_sources
          : nullptr,
      w.production_facility_conditions ? &*w.production_facility_conditions
                                       : nullptr,
      w.governance_work_expense ? &*w.governance_work_expense : nullptr);
  if (!checked.ok)
    return checked;
  auto facilities =
      selected_native_economic_facility_versions(w.catalog->identity);
  if (!facilities.ok)
    return {false, {}, std::move(facilities.error)};
  if (bool(facilities.value.work_expense) != bool(s.governance))
    return required("EconomicPaymentOriginalWorkOwnerRequired");
  checked = validate_native_item_craft_layout(w);
  if (!checked.ok)
    return checked;
  checked = validate_native_item_craft_original_headers(
      w.common, *w.catalog, w.item_instances ? &*w.item_instances : nullptr,
      w.craft_jobs ? &*w.craft_jobs : nullptr);
  if (!checked.ok)
    return checked;
  checked = validate_native_item_craft_original_actions(
      w.common, *w.catalog, w.item_instances ? &*w.item_instances : nullptr,
      w.craft_jobs ? &*w.craft_jobs : nullptr);
  if (!checked.ok)
    return checked;
  auto items = selected_native_item_craft_versions(w.catalog->identity);
  if (!items.ok)
    return {false, {}, std::move(items.error)};
  if ((items.value.item_instances == 3) != bool(s.repair))
    return required("EconomicPaymentOriginalRepairOwnerRequired");
  checked = validate_native_transport_layout(w);
  if (!checked.ok)
    return checked;
  auto transport = selected_native_transport_versions(w.catalog->identity);
  if (!transport.ok)
    return {false, {}, std::move(transport.error)};
  bool cargo = false;
  for (const auto &m : w.catalog->identity.module_versions)
    if (m.first == "cargo_handling") {
      if (cargo || m.second != "1")
        return common_failure("UnsupportedCargoHandlingVersion", m.first);
      cargo = true;
    }
  // 明选 Field/Army 的军费沿原单扣，不选择商业 S5；先核真实军队根，
  // 再沿下方原收据/物理归属与内部 P11 全验拒绝任何混入来源。
  // cargo、普通商业 group 与外借 P11 仍要求其真实 transport owner。
  const bool field_army_only = transport.value.group &&
                               w.catalog->field_group_passage && !cargo &&
                               !s.obligations;
  if (field_army_only) {
    checked = validate_native_field_army_transport_roots(w);
    if (!checked.ok)
      return checked;
  }
  const bool transport_selected =
      (transport.value.group && !field_army_only) || cargo;
  if (transport_selected != bool(s.transport))
    return required("EconomicPaymentOriginalTransportOwnerRequired");
  if (!s.transport) {
    if (!w.movement_exact || !w.economic_transport_expenses ||
        !w.catalog->travel)
      return required("EconomicPaymentOriginalTransportRootsRequired");
    const auto &h = w.movement_exact->cargo_handling;
    if (!w.catalog->travel->cargo_handling_rules.empty() || !h.terms.empty() ||
        !h.executions.empty() || !h.work_results.empty() || !h.results.empty())
      return common_failure("EconomicPaymentUnselectedHandlingSource");
    // 真S5收据只有group/crossing/handling owner；个人movement_expense不是此账。
    auto physical = index_transport_payment_physical_owners(
        w.common, *w.movement_exact,
        w.spatial_carriers ? &*w.spatial_carriers : nullptr,
        w.group_movement_executions ? &*w.group_movement_executions : nullptr,
        *w.economic_transport_expenses);
    if (!physical.ok)
      return {false, {}, std::move(physical.error)};
    if (!physical.value.bindings.empty() ||
        !w.economic_transport_expenses->payments.empty())
      return common_failure("EconomicPaymentUnselectedTransportSource");
    for (const auto &row : w.common.state().facts.rows())
      if (row.second.kind == "transport_expense_paid")
        return common_failure("EconomicPaymentUnselectedTransportSource",
                              row.first);
  }
  checked = validate_native_economic_obligation_layout(w);
  if (!checked.ok)
    return checked;
  if (owned && s.obligations)
    return common_failure("EconomicPaymentDuplicateObligationOwner");
  const auto *obligations = s.obligations ? s.obligations.get() : owned;
  if (!w.economic_obligation_sources && obligations)
    return common_failure("EconomicPaymentUnselectedObligationOwner");
  if (w.economic_obligation_sources && !s.obligations &&
      (s.governance || s.repair || s.transport))
    return required("EconomicPaymentSelectedObligationOriginalOwnerRequired");
  // 本轮内部P11前序只在四业务真实未选后构造；selected分支仍借其真实全owner。
  if (w.economic_obligation_sources && !obligations && !constructing)
    return required("EconomicPaymentOriginalObligationOwnerRequired");
  if (obligations &&
      (&obligations->common != &w.common ||
       &obligations->economy != &w.local_economy ||
       &obligations->sources != &*w.economic_obligation_sources ||
       !obligations->original.bound_to(w.common, w.local_economy) ||
       &obligations->clock != &p1.opening.clock))
    return common_failure("EconomicPaymentObligationSnapshotMismatch");
  if (obligations) {
    // 内部拥有与真实外借均走原唯一全验，地址相等不能冒充来源已验。
    checked = validate_economic_obligation_sources(*obligations);
    if (!checked.ok)
      return checked;
  }
  if (owned) {
    // 沿原LE零周期约束作反查；保留原周期工资/口粮/铺租，不造P11递归适配器。
    for (const auto &row : w.local_economy.obligations.rows()) {
      if (row.second.period_ticks != 0)
        continue;
      bool bound = false;
      for (const auto &binding : owned->sources.obligation_sources)
        bound |= binding.obligation_ref.owner_domain == "local_economy" &&
                 binding.obligation_ref.obligation_id == row.first &&
                 binding.scheduler == EconomicObligationScheduler::source_only;
      if (!bound)
        return required("EconomicSourceOnlyBindingRequired", row.first);
    }
    // 两个保留Common头也须有真实LE原行；坏domain不能把孤头藏过未选业务门。
    for (const auto &row : w.common.state().facts.rows()) {
      if (row.second.kind != "contract_payment_due" &&
          row.second.kind != "obligation_payment")
        continue;
      if (row.second.domain != "local_economy" || row.second.id != row.first)
        return common_failure("EconomicPaymentOriginalHeaderMismatch",
                              row.first);
      if (row.second.kind == "contract_payment_due") {
        const auto *arrear = w.local_economy.arrears.find(row.first);
        if (!arrear || arrear->id != row.first ||
            !w.local_economy.obligations.find(arrear->obligation_id))
          return common_failure("EconomicPaymentOriginalArrearRequired",
                                row.first);
      } else {
        const auto *payment = w.local_economy.payments.find(row.first);
        if (!payment || payment->id != row.first ||
            !w.local_economy.obligations.find(payment->obligation_id) ||
            !w.local_economy.arrears.find(payment->arrear_id))
          return common_failure("EconomicPaymentOriginalCashRequired",
                                row.first);
      }
    }
  }
  if (s.transport) {
    if (!w.economic_transport_expenses ||
        &s.transport->state != &*w.economic_transport_expenses ||
        s.transport->sources.obligations != obligations)
      return common_failure("EconomicPaymentTransportSnapshotMismatch");
    auto observed =
        s.transport->sources.actual_sources.current_transport_expense_moment();
    if (!observed.ok)
      return {false, {}, std::move(observed.error)};
    auto same = compare_world_moments(at.value, observed.value);
    if (!same.ok)
      return {false, {}, std::move(same.error)};
    if (same.value)
      return common_failure("EconomicPaymentTransportClockMismatch");
    // 原抽象口无Common/目录绑定证明；空付款账的全验也不能证明同World。
    // 待真实Motion拥有口导出可核绑定后接入，绝不把外借空账当已实现。
    return required("EconomicPaymentOriginalTransportBindingOwnerRequired");
  }
  if (s.governance) {
    const auto &x = *s.governance;
    if (&x.common != &w.common || &x.economy != &w.local_economy ||
        &x.state != &*w.governance_work_expense ||
        &x.clock != &p1.opening.clock || x.obligations != obligations ||
        !x.budget_execution || !x.settlement)
      return common_failure("EconomicPaymentWorkSnapshotMismatch");
  }
  if (s.repair) {
    const auto &x = *s.repair;
    if (&x.common != &w.common || &x.economy != &w.local_economy ||
        &x.items != &*w.item_instances || &x.clock != &p1.opening.clock ||
        &x.policies != w.catalog->repair_policies.get() ||
        x.rules != w.catalog->repair_service_rules.get() ||
        x.obligations != obligations || !x.repair || !x.rules ||
        !x.original.bound_to(w.common, x.items))
      return common_failure("EconomicPaymentRepairSnapshotMismatch");
  }
  return common_success();
}
} // namespace
struct NativeEconomicPaymentWorldScope::Impl final : CheckpointAllocatedObject {
  const WorldBundle &world;
  NativeEconomicPaymentWorldSources sources;
  NativeServiceObligationOriginalRead service_original;
  std::optional<EconomicObligationReadContext> source_only;
  NativeEconomicOriginalPaymentRead payments;
  const EconomicObligationReadContext *obligations() const noexcept {
    return sources.obligations ? sources.obligations.get()
                               : (source_only ? &*source_only : nullptr);
  }
  Impl(const WorldBundle &w, NativeEconomicPaymentWorldSources s)
      : world(w), sources(std::move(s)),
        service_original(w.common, w.local_economy, sources.governance.get(),
                         sources.repair.get()),
        source_only(
            w.economic_obligation_sources && !sources.obligations
                ? std::optional<
                      EconomicObligationReadContext>{EconomicObligationReadContext{
                      w.common, w.local_economy, *w.economic_obligation_sources,
                      service_original,
                      sources.political->political_read_lease()
                          .bindings()
                          .opening.clock}}
                : std::nullopt),
        payments(w.common, w.local_economy, sources.transport.get(),
                 sources.governance.get(), nullptr, sources.repair.get(),
                 obligations()) {}
};
NativeEconomicPaymentWorldScope::NativeEconomicPaymentWorldScope(
    std::unique_ptr<Impl> x)
    : impl_(std::move(x)) {}
NativeEconomicPaymentWorldScope::~NativeEconomicPaymentWorldScope() = default;
Outcome<std::unique_ptr<NativeEconomicPaymentWorldScope>>
NativeEconomicPaymentWorldScope::create(const WorldBundle &w,
                                        NativeEconomicPaymentWorldSources s) {
  auto valid = roots(w, s, nullptr, true);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  auto out = std::unique_ptr<NativeEconomicPaymentWorldScope>(
      new NativeEconomicPaymentWorldScope(
          std::make_unique<Impl>(w, std::move(s))));
  valid = out->validate_bindings();
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  return {true, std::move(out), {}};
}
bool NativeEconomicPaymentWorldScope::bound_to(
    const WorldBundle &w) const noexcept {
  return &impl_->world == &w;
}
Status NativeEconomicPaymentWorldScope::validate_bindings() const {
  return roots(impl_->world, impl_->sources,
               impl_->source_only ? &*impl_->source_only : nullptr);
}
const NativeEconomicOriginalPaymentRead &
NativeEconomicPaymentWorldScope::read() const noexcept {
  return impl_->payments;
}
const EconomicObligationReadContext *
NativeEconomicPaymentWorldScope::obligations() const noexcept {
  return impl_->obligations();
}
} // namespace hanmo::resident
