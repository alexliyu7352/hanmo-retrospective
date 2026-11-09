#include "economic_world_scope.hpp"
#include "born_population_sources.hpp"
#include "resident/domains/commerce/private_shop_absence.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
#include "resident/domains/estate/employment/estate_employment.hpp"
#include "resident/domains/estate/employment/estate_employment_profile.hpp"
#include "resident/domains/local_economy/disruptions/production_disruptions.hpp"
#include "resident/domains/local_economy/takeover/execution_phase.hpp"
#include "resident/domains/politics/takeover/politics_occupation_taxpayer.hpp"
#include "resident/kernel/estate_employment_integration.hpp"
#include "resident/kernel/native_economic_obligation_sources.hpp"
#include "resident/kernel/native_political_world_scope.hpp"
#include "resident/kernel/native_private_shop_operations.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
#include <type_traits>
namespace hanmo::resident {
namespace {
Status missing(const char *code) {
  return common_failure(code, {}, ErrorKind::unsupported_capability);
}
bool military_source(const SourceRef &s) {
  return s.domain == "population_military" ||
         s.kind == "population_military_action" ||
         s.kind == "population_enrolled" || s.kind == "population_released" ||
         s.kind == "population_casualty" || s.kind == "population_recovered" ||
         s.kind == "population_deserted" ||
         s.kind == "population_military_integration" ||
         s.kind == "population_loss";
}
Status absent_military_population(const WorldBundle &w) {
  if (!w.catalog->military)
    return missing("EconomicWorldOriginalMilitaryCatalogRequired");
  auto valid = validate_unselected_population_military_sources(
      w.common, w.local_economy, *w.catalog->local_economy, w.military,
      *w.catalog->military, w.population_military,
      w.catalog->population_military.get(), w.catalog->identity);
  if (!valid.ok || !w.population_transport)
    return valid;
  const auto &p = *w.population_transport;
  const auto optional = [](const std::optional<SourceRef> &s) {
    return s && military_source(*s);
  };
  const auto many = [](const auto &sources) {
    return std::any_of(sources.begin(), sources.end(), military_source);
  };
  // 军籍未选还须反查P02中真实保留的资格/地点源，不能靠军籍本表为空跳过。
  for (const auto &row : p.source_revisions.rows())
    if (optional(row.second.condition_source_ref) ||
        optional(row.second.location_binding_ref))
      return common_failure("EconomicWorldUnselectedMilitaryPopulationSource",
                            row.first);
  for (const auto &row : p.revision_sources)
    if (military_source(row.actual_source) || optional(row.condition_after) ||
        optional(row.location_after))
      return common_failure("EconomicWorldUnselectedMilitaryPopulationSource",
                            row.actual_source.id);
  for (const auto &row : p.selections.rows())
    if (military_source(row.second.owner_business_ref) ||
        military_source(row.second.release_gate))
      return common_failure("EconomicWorldUnselectedMilitaryPopulationSource",
                            row.first);
  for (const auto &row : p.materializations.rows()) {
    const auto &x = row.second;
    if (optional(x.condition_ref) || military_source(x.provision_handoff_ref) ||
        military_source(x.location_binding_ref) ||
        many(x.obligation_continuation_refs))
      return common_failure("EconomicWorldUnselectedMilitaryPopulationSource",
                            row.first);
  }
  for (const auto &row : p.transport_bindings.rows())
    if (military_source(row.second.registration_source_ref))
      return common_failure("EconomicWorldUnselectedMilitaryPopulationSource",
                            row.first);
  for (const auto &row : p.releases)
    if (military_source(row.actual_handover_source))
      return common_failure("EconomicWorldUnselectedMilitaryPopulationSource",
                            row.binding_id);
  for (const auto &row : p.public_market_labor_origins.rows()) {
    const auto &x = row.second;
    if (military_source(x.location_source_ref) ||
        many(x.eligibility_source_refs) ||
        military_source(x.accepted_labor_authority_ref) ||
        military_source(x.approved_org_fact_ref))
      return common_failure("EconomicWorldUnselectedMilitaryPopulationSource",
                            row.first);
  }
  for (const auto &row : p.public_market_labor_transitions) {
    bool retained = military_source(row.origin_ref) ||
                    optional(row.previous_transition_ref);
    std::visit(
        [&](const auto &change) {
          using T = std::decay_t<decltype(change)>;
          if constexpr (std::is_same_v<T, PublicMarketEligibilityChanged>)
            retained = retained ||
                       many(change.actual_location_or_condition_source_refs) ||
                       military_source(change.qualification_change_cause_ref);
          else if constexpr (std::is_same_v<T, PublicMarketAllocationAdjusted>)
            retained = retained ||
                       military_source(change.original_population_change_ref);
          else
            retained = retained ||
                       military_source(change.original_org_boundary_fact_ref);
        },
        row.change);
    if (retained)
      return common_failure("EconomicWorldUnselectedMilitaryPopulationSource",
                            row.transition_id);
  }
  return common_success();
}
Status roots(const WorldBundle &w, const NativePoliticalWorldScope &p,
             const NativeEconomicPaymentWorldScope &payments,
             const NativeEconomicFifoWorldScope &fifo,
             const NativeHouseholdWorldScope &household,
             const NativeEconomicWorldLateSources &late,
             WorldBundle *candidate) {
  if (!w.catalog || !w.catalog->local_economy || !w.catalog->organization ||
      !p.bound_to(w) || !payments.bound_to(w) || !fifo.bound_to(w) ||
      !household.bound_to(w) || (candidate && candidate != &w))
    return common_failure("EconomicWorldScopeSnapshotMismatch");
  auto valid = payments.validate_bindings();
  if (!valid.ok)
    return valid;
  valid = fifo.validate_bindings();
  if (!valid.ok)
    return valid;
  valid = household.validate_bindings();
  if (!valid.ok)
    return valid;
  const auto political = p.political_read_lease().bindings();
  const auto &f = fifo.fifo().context();
  const auto *takeover = p.takeover_context();
  if (!f.work || !fifo.production_inputs() || !w.production_cycles ||
      &f.work->sources != &political.opening.sources ||
      &f.work->clock != &political.opening.clock ||
      &f.work->inputs != &fifo.production_inputs()->inputs() ||
      &household.service().household().common != &w.common ||
      &household.service().household().economy != &w.local_economy)
    return missing("EconomicWorldOriginalProductionSourcesRequired");
  // 原接管读口只随真实选择存在；未选沿生产原叶重核同World全部保留来源。
  const auto &input_owner = *fifo.production_inputs();
  const auto &capacity = input_owner.original_inputs().capacity;
  const bool takeover_selected = w.political_takeover_enabled();
  if (takeover_selected != bool(takeover) ||
      takeover_selected != bool(political.takeover) ||
      takeover_selected != bool(input_owner.takeovers()) ||
      takeover_selected != bool(f.takeover) ||
      capacity.takeovers != input_owner.takeovers())
    return common_failure("EconomicWorldTakeoverSelectionMismatch");
  if (takeover) {
    if (capacity.unselected_takeover_world || takeover->phase != f.phase ||
        &takeover->common != &w.common ||
        &takeover->state != &w.political_takeover ||
        &takeover->catalog != w.catalog->political_takeover.get() ||
        &takeover->organizations != &w.organization ||
        &takeover->organization_catalog != w.catalog->organization.get() ||
        takeover->actual_clock != &political.organization.clock ||
        takeover->organization_runtime != &political.organization.runtime ||
        takeover->politics_state != &w.politics ||
        f.takeover != &w.economic_takeover_execution)
      return common_failure("EconomicWorldTakeoverSnapshotMismatch");
  } else {
    if (capacity.unselected_takeover_world != &w)
      return common_failure("EconomicWorldTakeoverSnapshotMismatch");
    valid = validate_unselected_economic_production_takeover_sources(capacity);
    if (!valid.ok)
      return valid;
  }
  auto shop_versions =
      selected_native_private_shop_operation_versions(w.catalog->identity);
  if (!shop_versions.ok)
    return {false, {}, std::move(shop_versions.error)};
  const auto *shop = household.foundation().shops();
  if (bool(shop_versions.value.shops) != bool(w.catalog->private_shops) ||
      bool(shop_versions.value.shops) != bool(shop))
    return common_failure("EconomicWorldPrivateShopSelectionMismatch");
  if (shop) {
    if (&shop->common != &w.common || &shop->economy != &w.local_economy ||
        &shop->state != &w.private_shops ||
        &shop->catalog != w.catalog->private_shops.get() ||
        &shop->economy_catalog != w.catalog->local_economy.get() ||
        shop->phase != f.phase)
      return common_failure("EconomicWorldPrivateShopSnapshotMismatch");
  } else {
    // 没有商铺不制造责任provider；原整World否定门同时拒政治/钱账/家户残留。
    valid = validate_unselected_private_shop_sources(w);
    if (!valid.ok)
      return valid;
  }
  auto at = w.actual_phase_moment();
  if (!at.ok)
    return {false, {}, std::move(at.error)};
  auto same = compare_world_moments(at.value, p.actual_at());
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  if (same.value)
    return common_failure("EconomicWorldScopeMomentExpired");
  if (fifo.production_inputs()->labor_history().original.military !=
      late.military.get())
    return common_failure("EconomicWorldPopulationMilitaryOwnerMismatch");
  if (w.population_military_enabled() != bool(late.military))
    return missing("EconomicWorldOriginalPopulationMilitaryRequired");
  if (late.military) {
    const auto &m = *late.military;
    if (&m.common != &w.common || &m.economy != &w.local_economy ||
        &m.economy_catalog != w.catalog->local_economy.get() ||
        &m.state != &w.population_military ||
        &m.catalog != w.catalog->population_military.get() ||
        &m.organization != &w.organization ||
        &m.organization_catalog != w.catalog->organization.get() ||
        &m.military != &w.military ||
        &m.military_catalog != w.catalog->military.get())
      return common_failure("EconomicWorldPopulationMilitarySnapshotMismatch");
  } else {
    // 原军事owner补足未选人口军务的五表/保留来源门；不能以四张空表冒证明。
    valid = absent_military_population(w);
    if (!valid.ok)
      return valid;
  }
  valid = validate_estate_employment_layout(
      w.estate_employment_enabled(), w.catalog->identity.module_versions);
  if (!valid.ok)
    return valid;
  if (w.estate_employment_enabled() != bool(late.employment))
    return missing("EconomicWorldOriginalEmploymentRequired");
  if (late.employment) {
    const auto &e = *late.employment;
    if (&e.common != &w.common || &e.economy != &w.local_economy ||
        &e.economy_catalog != w.catalog->local_economy.get() ||
        &e.state != &w.estate_employment ||
        &e.catalog != w.catalog->estate_employment.get() || e.phase != f.phase)
      return common_failure("EconomicWorldEmploymentSnapshotMismatch");
  } else {
    valid = validate_estate_employment_world(w);
    if (!valid.ok)
      return valid;
    for (const auto &row : w.common.state().facts.rows())
      if (row.second.kind == "estate_employment_offered" ||
          row.second.kind == "estate_employment_decided" ||
          row.second.kind == "estate_employment_service")
        return common_failure("EconomicWorldUnselectedEmploymentSource",
                              row.first);
  }
  auto selected =
      selected_native_economic_obligation_versions(w.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  valid = validate_named_birth_population_selected_layout(
      w.local_economy, selected.value.local_economy);
  if (!valid.ok)
    return valid;
  if (named_birth_population_selected(selected.value.local_economy) !=
      bool(late.born))
    return missing("EconomicWorldOriginalBornPopulationRequired");
  if (late.born) {
    const auto &b = *late.born;
    if (&b.common != &w.common || &b.economy != &w.local_economy ||
        &b.catalog != w.catalog->local_economy.get() ||
        &b.clock != &political.opening.clock ||
        b.selected_local_economy != selected.value.local_economy)
      return common_failure("EconomicWorldBornPopulationSnapshotMismatch");
    valid = validate_born_population_source_bindings(b);
    if (!valid.ok)
      return valid;
  }
  if (candidate) {
    if (!candidate->household_purchases || !candidate->action_elapsed)
      return missing("EconomicWorldOriginalHouseholdWriteRequired");
    auto original_household = household.service().write_binding(
        candidate->common, *candidate->household_purchases,
        *candidate->action_elapsed);
    if (!original_household.ok)
      return {false, {}, std::move(original_household.error)};
    const auto *production = fifo.production();
    if (!production || &production->state != &*w.production_cycles ||
        &production->inputs != &fifo.production_inputs()->inputs() ||
        &production->clock != &political.opening.clock ||
        production->original_payments != &payments.read() || !late.market ||
        late.market->original_economic_material_read() != &fifo.materials())
      return missing("EconomicWorldOriginalWriteSourcesRequired");
  } else if (late.market)
    return common_failure("EconomicWorldReadHasWriteMarket");
  return common_success();
}
} // namespace
struct NativeEconomicWorldScope::Impl final : CheckpointAllocatedObject {
  const WorldBundle &world;
  WorldBundle *candidate;
  std::shared_ptr<const NativePoliticalWorldScope> political;
  std::shared_ptr<const NativeEconomicPaymentWorldScope> payments;
  std::shared_ptr<const NativeEconomicFifoWorldScope> fifo;
  std::shared_ptr<const NativeHouseholdWorldScope> household;
  NativeEconomicWorldLateSources late;
  OrganizationAccess organizations;
  std::optional<PoliticalTakeoverAccess> politics;
  std::optional<PoliticalOccupationFiscalTaxpayerAccess> taxpayer;
  std::optional<EconomicTakeoverReadContext> takeover;
  std::optional<PrivateShopResponsibilitySourceContext> shops;
  EconomicExactValidationContext validation;
  std::optional<EconomicTakeoverPhaseContext> takeover_write;
  std::optional<ProductionDisruptionContext> disruption;
  struct Read final : EconomicExactReadLease {
    const Impl &x;
    explicit Read(const Impl &x) : x(x) {}
    EconomicExactReadBinding bindings() const override {
      return {*x.world.catalog->local_economy,
              x.organizations,
              x.validation,
              x.household->service().read_binding(),
              x.late.military.get(),
              x.takeover ? &*x.takeover : nullptr,
              x.takeover ? &x.world.economic_takeover_execution : nullptr,
              x.late.employment.get(),
              x.shops ? &*x.shops : nullptr,
              &x.fifo->production_inputs()->labor_history()};
    }
  } read;
  struct Write final : EconomicExactWriteLease {
    Impl &x;
    explicit Write(Impl &x) : x(x) {}
    EconomicExactWriteBinding bindings() override {
      auto &w = *x.candidate;
      const auto &service = x.household->service();
      return {w.common,
              w.local_economy,
              *w.catalog->local_economy,
              x.organizations,
              w.services,
              *x.fifo->production(),
              x.validation.calendar,
              {w.common, *w.household_purchases, *w.action_elapsed,
               service.household(), service.market()},
              x.fifo->fifo().context().household_management,
              x.disruption ? &*x.disruption : nullptr,
              x.takeover_write ? &*x.takeover_write : nullptr,
              x.late.employment.get(),
              x.late.market.get(),
              x.shops ? &*x.shops : nullptr,
              &x.validation};
    }
  };
  std::optional<Write> write;
  Impl(const WorldBundle &w, std::shared_ptr<const NativePoliticalWorldScope> p,
       std::shared_ptr<const NativeEconomicPaymentWorldScope> pay,
       std::shared_ptr<const NativeEconomicFifoWorldScope> f,
       std::shared_ptr<const NativeHouseholdWorldScope> h,
       NativeEconomicWorldLateSources l, WorldBundle *c, Integer selected)
      : world(w), candidate(c), political(std::move(p)),
        payments(std::move(pay)), fifo(std::move(f)), household(std::move(h)),
        late(std::move(l)),
        organizations(w.common, w.organization, *w.catalog->organization,
                      w.local_economy),
        validation{political->political_read_lease().bindings().opening,
                   payments->read(), *fifo->fifo().context().work, selected,
                   late.born.get()},
        read(*this) {
    // 这些context只在原选择真实存在时构造，寿命均晚于独立P1/FIFO前序。
    if (const auto *original_takeover = political->takeover_context()) {
      politics.emplace(*original_takeover);
      if (w.occupation_fiscal_enabled() ||
          w.fiscal_taxpayer_continuity_enabled())
        taxpayer.emplace(*original_takeover);
      takeover.emplace(EconomicTakeoverReadContext{
          w.common, w.local_economy, *w.catalog->local_economy,
          w.fiscal_settlement_enabled() ? &w.fiscal_settlement : nullptr,
          w.fiscal_settlement_enabled() ? w.catalog->fiscal_settlement.get()
                                        : nullptr,
          w.public_relief_enabled() ? &w.public_relief : nullptr, *politics,
          &w.organization, w.catalog->organization.get(),
          fifo->fifo().context().phase,
          w.occupation_fiscal_enabled() ? &w.occupation_fiscal_settlement
                                        : nullptr,
          taxpayer ? &*taxpayer : nullptr});
    }
    if (w.catalog->private_shops)
      shops.emplace(PrivateShopResponsibilitySourceContext{
          w.common, w.local_economy, *w.catalog->local_economy, w.private_shops,
          *w.catalog->private_shops, fifo->fifo().context().phase,
          w.catalog->private_shop_daily_rent.get(),
          w.private_shop_daily_rent_enabled() ? &w.private_shop_daily_rent
                                              : nullptr});
    if (candidate) {
      if (takeover)
        takeover_write.emplace(EconomicTakeoverPhaseContext{
            *takeover, candidate->economic_takeover_execution});
      if (candidate->catalog->production_disruptions)
        disruption.emplace(ProductionDisruptionContext{
            *candidate->catalog->production_disruptions,
            candidate->production_disruptions});
      write.emplace(*this);
    }
  }
};
NativeEconomicWorldScope::NativeEconomicWorldScope(std::unique_ptr<Impl> x)
    : impl_(std::move(x)) {}
NativeEconomicWorldScope::~NativeEconomicWorldScope() = default;
Outcome<std::unique_ptr<NativeEconomicWorldScope>>
NativeEconomicWorldScope::create(
    const WorldBundle &w, std::shared_ptr<const NativePoliticalWorldScope> p,
    std::shared_ptr<const NativeEconomicPaymentWorldScope> pay,
    std::shared_ptr<const NativeEconomicFifoWorldScope> f,
    std::shared_ptr<const NativeHouseholdWorldScope> h,
    NativeEconomicWorldLateSources l, WorldBundle *c) {
  if (!p || !pay || !f || !h)
    return {false, {}, missing("EconomicWorldOriginalOwnersRequired").error};
  auto valid = roots(w, *p, *pay, *f, *h, l, c);
  if (!valid.ok)
    return {false, {}, std::move(valid.error)};
  auto selected =
      selected_native_economic_obligation_versions(w.catalog->identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  return {true,
          std::unique_ptr<NativeEconomicWorldScope>(
              new NativeEconomicWorldScope(std::make_unique<Impl>(
                  w, std::move(p), std::move(pay), std::move(f), std::move(h),
                  std::move(l), c, selected.value.local_economy))),
          {}};
}
bool NativeEconomicWorldScope::bound_to(const WorldBundle &w) const noexcept {
  return &impl_->world == &w;
}
Status NativeEconomicWorldScope::validate_bindings() const {
  const auto &x = *impl_;
  return roots(x.world, *x.political, *x.payments, *x.fifo, *x.household,
               x.late, x.candidate);
}
const EconomicExactReadLease &NativeEconomicWorldScope::read() const noexcept {
  return impl_->read;
}
EconomicExactWriteLease *NativeEconomicWorldScope::write() const noexcept {
  return impl_->write ? &*impl_->write : nullptr;
}
} // namespace hanmo::resident
