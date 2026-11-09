#include "household_service_lease.hpp"
#include "resident/kernel/native_private_shop_operations.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
Outcome<HouseholdRetailAcceptance>
NativeHouseholdServiceLease::RetailAcceptanceLink::
    read_household_retail_acceptance(const StableId &id) const {
  if (!contracts_)
    return {false,
            {},
            common_failure("NativeHouseholdAcceptanceLinkUnbound", {},
                           ErrorKind::internal)
                .error};
  return contracts_->read_household_retail_acceptance(id);
}
Status NativeHouseholdServiceLease::RetailAcceptanceLink::
    validate_household_retail_settlement(
        const HouseholdRetailSettlement &settlement) const {
  if (!contracts_)
    return common_failure("NativeHouseholdAcceptanceLinkUnbound", {},
                          ErrorKind::internal);
  return contracts_->validate_household_retail_settlement(settlement);
}
NativeHouseholdServiceLease::NativeHouseholdServiceLease(
    NativeHouseholdServiceDependencies d)
    : dependencies_(std::move(d)),
      private_originals_(
          dependencies_.market_original.private_operations
              ? std::optional<
                    NativePrivateMarketServiceOriginalRead>{std::in_place,
                                                            *dependencies_
                                                                 .market_original
                                                                 .private_operations,
                                                            dependencies_
                                                                .market_original
                                                                .conditions,
                                                            dependencies_
                                                                .market_original
                                                                .identity}
              : std::nullopt),
      market_original_context_{
          dependencies_.market_original.identity,
          dependencies_.market_original.common,
          dependencies_.market_original.catalog,
          dependencies_.market_original.purchases,
          dependencies_.market_original.assets,
          dependencies_.market_original.shops_catalog,
          dependencies_.market_original.installed,
          dependencies_.market_original.condition_catalog,
          dependencies_.market_original.conditions,
          dependencies_.market_original.public_authority,
          private_originals_ ? &*private_originals_ : nullptr,
          dependencies_.market_original.current,
          dependencies_.market_original.arithmetic,
          dependencies_.market_original.unselected_shops_world},
      market_originals_(market_original_context_),
      market_{dependencies_.named.common, dependencies_.named.state,
              dependencies_.named.claims, dependencies_.staffing,
              market_originals_,          dependencies_.all_holders,
              dependencies_.now},
      market_capacity_(market_), named_{dependencies_.named.common,
                                        dependencies_.named.shared,
                                        dependencies_.named.catalog,
                                        dependencies_.named.state,
                                        dependencies_.named.elapsed,
                                        dependencies_.named.body,
                                        dependencies_.named.claims,
                                        market_capacity_,
                                        dependencies_.named.carriers,
                                        dependencies_.named.movement,
                                        dependencies_.named.registrations,
                                        dependencies_.named.spatial,
                                        dependencies_.named.clock,
                                        dependencies_.named.military_handoffs,
                                        dependencies_.private_market_custody},
      // Construct the actual retail interface before converting it to a base
      // reference. Its final target is installed privately after contracts_
      // exists.
      retail_acceptance_{}, retail_{dependencies_.named.common,
                                    dependencies_.named.shared,
                                    dependencies_.retail.shops,
                                    dependencies_.retail.shops_catalog,
                                    dependencies_.retail.markets,
                                    dependencies_.retail.catalog,
                                    dependencies_.retail.state,
                                    dependencies_.now,
                                    retail_acceptance_,
                                    dependencies_.retail.market_history,
                                    dependencies_.retail.market_capture},
      environment_context_{named_,
                           dependencies_.cohort,
                           dependencies_.controls,
                           dependencies_.claim_catalog,
                           retail_,
                           dependencies_.scale,
                           dependencies_.transport_expenses,
                           dependencies_.handling,
                           dependencies_.travel,
                           dependencies_.group_executions,
                           market_,
                           dependencies_.inventory_history,
                           dependencies_.selected_household_version,
                           dependencies_.mobile_custody,
                           dependencies_.private_market_custody},
      environment_(environment_context_),
      household_{named_.common,
                 named_.shared,
                 dependencies_.cohort.economy,
                 dependencies_.cohort.population,
                 named_.catalog,
                 named_.state,
                 dependencies_.controls,
                 environment_,
                 named_.claims,
                 dependencies_.claim_catalog,
                 market_capacity_,
                 retail_,
                 named_.elapsed,
                 dependencies_.scale,
                 dependencies_.now,
                 dependencies_.transport_expenses,
                 &named_.registrations,
                 dependencies_.selected_household_version,
                 dependencies_.mobile_custody,
                 dependencies_.private_market_custody},
      contracts_(household_), holders_(household_) {
  retail_acceptance_.bind(contracts_);
}
Status NativeHouseholdServiceLease::validate_bindings() const {
  if (named_.private_market_custody != dependencies_.private_market_custody ||
      dependencies_.cohort.private_market_custody !=
          dependencies_.private_market_custody ||
      environment_context_.private_market_custody !=
          dependencies_.private_market_custody ||
      household_.private_market_custody != dependencies_.private_market_custody)
    return common_failure("NativeHouseholdCustodyReaderGraphMismatch");
  const auto &d = dependencies_;
  const auto &n = named_;
  const auto &c = d.cohort;
  const auto &m = market_original_context_;
  const auto *s = d.staffing;
  const auto *shop = d.market_original.private_operations;
  const auto shop_selection =
      selected_native_private_shop_operation_versions(m.identity);
  if (!shop_selection.ok)
    return {false, {}, shop_selection.error};
  if (bool(shop_selection.value.shops) != bool(shop) ||
      (shop && (!shop_selection.value.exact_operations() ||
                shop->sources.schema_version != 1 ||
                shop->capabilities.schema_version != 1)))
    return common_failure(
        "NativeHouseholdPrivateMarketOriginalSelectionRequired");
  if (bool(shop) != bool(private_originals_) ||
      bool(shop) != bool(retail_.shops_catalog))
    return common_failure("NativeHouseholdPrivateMarketSelectionMismatch");
  if (!shop) {
    const auto *w = d.market_original.unselected_shops_world;
    if (!w || !w->catalog || &w->private_shops != &retail_.shops ||
        w->catalog->household_retail_settlements.get() != &retail_.catalog ||
        !w->household_retail_settlements ||
        &*w->household_retail_settlements != &retail_.state ||
        w->catalog->growth.get() != &retail_.markets)
      return common_failure("NativeHouseholdPrivateAbsenceSnapshotMismatch");
  }
  if (bool(s) != bool(m.public_authority))
    return common_failure("NativeHouseholdPublicMarketSelectionMismatch", {},
                          ErrorKind::unsupported_capability);
  if (!s) {
    const auto unselected = validate_unselected_public_household_market(
        n.common, n.catalog, n.state, c.population, d.retail.catalog);
    if (!unselected.ok)
      return unselected;
  }
  if (&retail_.acceptances !=
          static_cast<const HouseholdRetailAcceptanceRead *>(
              &retail_acceptance_) ||
      !retail_acceptance_.bound_to(contracts_))
    return common_failure("NativeHouseholdAcceptanceLinkMismatch", {},
                          ErrorKind::internal);
  if (&n.common != &c.common || &n.state != &c.state ||
      &n.catalog != &c.catalog || &n.carriers != &c.carriers ||
      &n.movement != &c.movement || &n.registrations != &c.registrations ||
      &n.spatial != &c.spatial || &n.clock != &c.clock ||
      (s && (&s->common != &n.common || &s->economy != &c.economy ||
             &s->population != &c.population || &s->purchases != &n.state ||
             &s->controls != &d.controls)) ||
      (shop && (&shop->shops.common != &n.common ||
                &shop->shops.state != &retail_.shops ||
                &shop->shops.catalog != retail_.shops_catalog ||
                shop->shops.retail_catalog != &retail_.catalog ||
                shop->shops.retail != &retail_.state ||
                shop->shops.exact_operations != shop ||
                &shop->elapsed != &n.elapsed || &shop->body != &n.body ||
                &shop->clock != &n.clock || &shop->scale != &d.scale)) ||
      m.private_business !=
          (private_originals_ ? &*private_originals_ : nullptr) ||
      &m.common != &n.common || &m.catalog != &n.catalog ||
      &m.purchases != &n.state || m.shops_catalog != retail_.shops_catalog ||
      &retail_.common != &n.common || &retail_.common_catalog != &n.shared ||
      &d.inventory_history.mobile.common != &n.common ||
      &d.inventory_history.mobile.carriers != &n.carriers ||
      &d.inventory_history.mobile.movement != &n.movement ||
      &d.inventory_history.mobile.registrations != &n.registrations ||
      &d.inventory_history.mobile.clock != &n.clock ||
      &d.inventory_history.mobile.spatial_sources != &n.spatial ||
      &d.inventory_history.mobile.common_catalog != &n.shared ||
      &d.inventory_history.group_executions != &d.group_executions)
    return common_failure("NativeHouseholdServiceSnapshotMismatch", {},
                          ErrorKind::internal);
  auto physical = validate_market_service_original_physical_sources(m);
  if (!physical.ok)
    return physical;
  if (bool(c.economy_catalog.production_management) != bool(m.assets) ||
      bool(c.economy.production_management) != bool(m.assets) ||
      (m.assets && &*c.economy_catalog.production_management != m.assets))
    return common_failure("NativeHouseholdProductionSelectionMismatch");
  if (!m.assets)
    for (const auto &row : c.economy.production_receipts.rows())
      if (row.second.production_context)
        return common_failure("NativeHouseholdUnselectedProductionContext",
                              row.first);
  if (!c.economy_catalog.enabled || d.scale.units_per_half_day <= 0 ||
      (d.selected_household_version != 2 &&
       d.selected_household_version != 3) ||
      (d.selected_household_version == 3 && !d.mobile_custody))
    return common_failure("NativeHouseholdSelectedOriginalsRequired", {},
                          ErrorKind::unsupported_capability);
  if (c.population_history &&
      (&c.population_history->common != &n.common ||
       &c.population_history->economy != &c.economy ||
       &c.population_history->population != &c.population ||
       &c.population_history->transport != &n.registrations ||
       &c.population_history->clock != &n.clock))
    return common_failure("NativeHouseholdPopulationHistoryMismatch", {},
                          ErrorKind::internal);
  const auto now = n.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  const auto spatial = n.spatial.current_moment();
  if (!spatial.ok)
    return {false, {}, spatial.error};
  for (const auto *at :
       {&now.value, &spatial.value, s ? &s->now : nullptr, &m.current}) {
    if (!at)
      continue;
    const auto same = compare_world_moments(*at, d.now);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value)
      return common_failure("NativeHouseholdServiceClockMismatch", {},
                            ErrorKind::internal);
  }
  return common_success();
}
Status NativeHouseholdServiceLease::validate_sources() const {
  auto bound = validate_bindings();
  if (!bound.ok)
    return bound;
  if (private_originals_) {
    bound =
        private_originals_->validate_capacity_source_coverage(household_.state);
    if (!bound.ok)
      return bound;
  }
  return validate_household_service_runtime_sources(read_binding(),
                                                    dependencies_.now);
}
Outcome<HouseholdServiceWriteBinding>
NativeHouseholdServiceLease::write_binding(CommonStore &c,
                                           HouseholdPurchaseState &s,
                                           ActionElapsedStore &e) const {
  auto bound = validate_bindings();
  if (!bound.ok)
    return {false, {c, s, e, household_, market_}, bound.error};
  if (&c != &household_.common || &s != &household_.state ||
      &e != &household_.elapsed)
    return {false,
            {c, s, e, household_, market_},
            common_failure("NativeHouseholdWriteCandidateMismatch", {},
                           ErrorKind::internal)
                .error};
  return {true, {c, s, e, household_, market_}, {}};
}
} // namespace hanmo::resident
