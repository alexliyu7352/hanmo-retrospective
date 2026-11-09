#include "private_shop_absence.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "private_shop_payload.hpp"
#include "resident/domains/local_economy/market_service_fields.hpp"
#include "resident/domains/local_economy/obligations/obligation_sources_owned_fields.hpp"
#include "resident/kernel/native_private_shop_operations.hpp"
#include "resident/kernel/political_shop_registry_director_integration.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <string_view>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void absent(const StableId &id = {}) {
  throw Failure{common_failure("UnselectedPrivateShopSource", id).error};
}
bool kind(std::string_view k) {
  return k == "initial_shop_premises" || k.starts_with("private_shop_") ||
         k.starts_with("PrivateShop") || k.starts_with("task_shop_order_") ||
         k == "political_shop_registry_initialized" ||
         k == "political_wanted_registered" ||
         k == "political_wanted_released" ||
         k == "political_shop_clearance_recorded" ||
         k == "political_shop_permit_issued" ||
         k == "political_shop_permit_revoked" ||
         k == "political_shop_registry_accepted" ||
         k == "political_request_shop_permission" ||
         k == "political_register_wanted" || k == "political_release_wanted" ||
         k == "political_revoke_shop_permit" ||
         k == "political_accept_shop_registry";
}
bool domain(std::string_view d) {
  return d == "private_shops" || d == "private_shop_operation_sources" ||
         d == "private_shop_operation_capabilities" ||
         d == "private_shop_daily_rent" ||
         d == "private_shop_command_results" ||
         d == "private_shop_liquidations" || d == "political_shop_licensing" ||
         d == "political_shop_licensing_receipts" ||
         d == "political_shop_registry_intents" ||
         d == "political_shop_registry_successions" ||
         d.starts_with("task_shop_order");
}
void source(const SourceRef &r) {
  if (kind(r.kind) || domain(r.domain))
    absent(r.id);
}
void reservation(const ReservationRef &r) {
  if (kind(r.activity_kind) || domain(r.domain_id))
    absent(r.id);
}
template <class T> struct Variant : std::false_type {};
template <class... T> struct Variant<std::variant<T...>> : std::true_type {};
template <class T> struct Optional : std::false_type {};
template <class T> struct Optional<std::optional<T>> : std::true_type {};
// 只沿原字段表借读来源，既不序列化临时树，也不增加另一份持久字段顺序。
struct OriginalSources {
  template <class T> void operator()(const char *, T &value) { read(value); }
  template <class T> void read(const T &value) {
    if constexpr (std::is_same_v<T, SourceRef>)
      source(value);
    else if constexpr (std::is_same_v<T, MomentOwnerSource>)
      source(value.source_ref);
    else if constexpr (std::is_same_v<T, DomainReadVersion>) {
      if (kind(value.kind) || domain(value.domain))
        absent(value.id);
    } else if constexpr (std::is_same_v<T, PrivateMarketAssetBasis>)
      absent(value.original_shop_ref);
    else if constexpr (std::is_same_v<T, PrivateMarketOperationSource>)
      absent(value.operation_ref);
    else if constexpr (std::is_same_v<T, OriginalShopOrderRefund>)
      absent(value.original_refund.source_ref.id);
    else if constexpr (std::is_same_v<T, OriginalShopCancellationFee>)
      absent(value.original_charge.source_ref.id);
    else if constexpr (std::is_same_v<T, StableId> || std::is_arithmetic_v<T> ||
                       std::is_enum_v<T>)
      return;
    else if constexpr (Optional<T>::value) {
      if (value)
        read(*value);
    } else if constexpr (Variant<T>::value)
      std::visit([&](const auto &row) { read(row); }, value);
    else if constexpr (requires {
                         value.begin();
                         value.end();
                       }) {
      for (const auto &row : value)
        read(row);
    } else if constexpr (requires {
                           obligation_source_owned_fields::fields(
                               *this, const_cast<T &>(value));
                         })
      obligation_source_owned_fields::fields(*this, const_cast<T &>(value));
    else if constexpr (requires {
                         public_market_record(*this, const_cast<T &>(value));
                       })
      public_market_record(*this, const_cast<T &>(value));
  }
};
void market_definition(const MarketServiceDefinition &d,
                       OriginalSources &scan) {
  if (d.asset_source_ref.kind == "private_shop_premises" ||
      d.asset_source_ref.domain == "commerce")
    absent(d.definition_ref.id);
  scan.read(d);
}
} // namespace
Status validate_unselected_private_shop_sources(const WorldBundle &w) {
  try {
    if (!w.catalog)
      return common_failure("PrivateShopAbsenceCatalogRequired");
    const auto &p = *w.catalog;
    const auto &c = w.common.state();
    if (!p.common || !p.local_economy)
      return common_failure("PrivateShopAbsenceOriginalRootsRequired");
    if (c.metadata.content_id != p.identity.content_id ||
        c.metadata.content_version != p.identity.content_version ||
        c.metadata.content_digest != p.identity.content_digest ||
        c.metadata.ruleset_id != p.identity.ruleset_id)
      return common_failure("PrivateShopAbsenceIdentityMismatch");
    for (const auto &m : p.identity.module_versions)
      if (m.first.starts_with("private_shop") ||
          m.first.starts_with("task_shop_order") ||
          m.first == "political_shop_licensing" ||
          m.first == "political_shop_licensing_receipts" ||
          m.first == "political_shop_registry_successions" ||
          m.first == "political_shop_registry_intents")
        absent(m.first);
    if (p.private_shops || p.private_shop_daily_rent ||
        p.private_shop_command_results || p.private_shop_liquidations ||
        p.political_shop_licensing || p.political_shop_licensing_receipts ||
        p.political_shop_registry_successions || p.task_shop_orders_enabled ||
        p.task_shop_order_contacts_enabled ||
        (p.director && p.director->shop_registry_intents))
      absent();
    auto valid = validate_private_shop_layout(
        w.common, p.identity.module_versions, nullptr, nullptr);
    if (!valid.ok)
      return valid;
    valid = validate_native_private_shop_operation_layout(w);
    if (!valid.ok)
      return valid;
    if (w.private_shops_seed_complete ||
        !is_default_private_shop_state(w.private_shops) ||
        w.private_shop_operation_sources ||
        w.private_shop_operation_capabilities || w.tasks.shop_orders)
      absent();
    const auto &rent = w.private_shop_daily_rent;
    if (w.private_shop_daily_rent_seed_complete || rent.version != 1 ||
        rent.last_step_tick || !rent.assessments.empty() ||
        !rent.payments.empty() || !rent.resumptions.empty() ||
        !rent.player_results.empty())
      absent();
    valid = validate_private_shop_daily_rent_layout(w.common, nullptr);
    if (!valid.ok)
      return valid;
    const auto &results = w.private_shop_command_results;
    if (w.private_shop_command_results_seed_complete || results.version != 1 ||
        !results.bindings.empty() || !results.control_bindings.empty())
      absent();
    const auto &liquidations = w.private_shop_liquidations;
    if (w.private_shop_liquidations_seed_complete ||
        liquidations.version != 1 || !liquidations.mandates.empty() ||
        !liquidations.acceptances.empty() ||
        !liquidations.revocations.empty() || !liquidations.closings.empty() ||
        !liquidations.result_bindings.empty())
      absent();
    for (auto check : {validate_political_shop_licensing_world,
                       validate_political_shop_registry_world,
                       validate_political_shop_registry_director_world}) {
      valid = check(w);
      if (!valid.ok)
        return valid;
    }
    for (const auto &row : w.director.plans.rows())
      if (row.second.template_id == "shop_registry_acceptance")
        absent(row.first);
    for (const auto &row : w.director.archived_plans.rows())
      if (row.second.template_id == "shop_registry_acceptance")
        absent(row.first);
    for (const auto &row : c.facts.rows())
      if (kind(row.second.kind) || domain(row.second.domain))
        absent(row.first);
    for (const auto &row : c.actions.rows())
      if (kind(row.second.kind))
        absent(row.first);
    for (const auto &row : c.history)
      if (kind(row.kind))
        absent(row.result_id);
    for (const auto &row : c.results.rows())
      if (row.second && kind(row.second->kind))
        absent(row.first);
    for (const auto &row : c.wallets.rows())
      for (const auto &r : row.second.reserved)
        reservation(r.action_id);
    for (const auto &row : c.inventories.rows())
      for (const auto &lot : row.second.lots)
        reservation(lot.reserved_by);
    for (const auto &row : p.common->roles.rows())
      for (const auto &s : row.second.services)
        if (kind(s))
          absent(row.first);
    // 仅拒绝原writer保留前缀的动态孤容器；作者Required原件不凭名称改判来源。
    for (const auto &row : c.wallets.rows())
      if (!p.common->required_wallets.find(row.first) &&
          std::string_view(row.first).starts_with("private_shop_wallet:"))
        absent(row.first);
    for (const auto &row : c.inventories.rows())
      if (!p.common->required_inventories.find(row.first) &&
          std::string_view(row.first).starts_with("private_shop_inventory:"))
        absent(row.first);
    const auto shop_obligation = [&](const StableId &id) {
      return !p.local_economy->obligations.find(id) &&
             (std::string_view(id).starts_with("private_shop_rent:") ||
              std::string_view(id).starts_with("private_shop_wage:"));
    };
    for (const auto &row : w.local_economy.obligations.rows())
      if (shop_obligation(row.first))
        absent(row.first);
    for (const auto &row : w.local_economy.obligation_remainders.rows())
      if (shop_obligation(row.first))
        absent(row.first);
    for (const auto &row : w.local_economy.arrears.rows())
      if (shop_obligation(row.second.obligation_id))
        absent(row.first);
    for (const auto &row : w.local_economy.payments.rows())
      if (shop_obligation(row.second.obligation_id))
        absent(row.first);
    OriginalSources scan;
    if (w.economic_obligation_sources)
      scan.read(*w.economic_obligation_sources);
    const auto authority = [&](const StableId &id) {
      const auto *head = c.facts.find(id);
      if (head && (kind(head->kind) || domain(head->domain)))
        absent(id);
    };
    for (const auto &row : w.local_economy.obligations.rows())
      authority(row.second.authority_ref);
    for (const auto &row : w.local_economy.payments.rows())
      authority(row.second.authority_ref);
    if (w.organization_runtime) {
      for (const auto &row : w.organization_runtime->authority_bases.rows()) {
        const auto &b = row.second;
        if (kind(b.command_kind))
          absent(row.first);
        source(b.issued_source);
        if (b.budget_ref)
          source(b.budget_ref->approval_source);
      }
      for (const auto &row : w.organization_runtime->relations.rows())
        source(row.second.source_ref);
    }
    if (p.household_purchases) {
      for (const auto &d : p.household_purchases->market_service_definitions)
        market_definition(d, scan);
      for (const auto &row : p.household_purchases->market_services.rows()) {
        source(row.second.custody_terms_ref);
        source(row.second.service_usage_right_ref);
        source(row.second.handling_rule_ref);
      }
    }
    if (w.household_purchases) {
      const auto &h = *w.household_purchases;
      if (!h.market_service_sources.private_sources.empty())
        absent();
      for (const auto &d : h.market_service_definitions)
        market_definition(d, scan);
      scan.read(h.market_service_sources.public_sources);
      for (const auto &row : h.accepted_terms.rows())
        if (std::holds_alternative<PrivateHouseholdRetailSeller>(
                row.second.seller))
          absent(row.first);
      for (const auto &row : h.service_segments) {
        source(row.actual_coverage_source);
        for (const auto &r : row.exertion_source_refs)
          source(r);
      }
    }
    if (p.household_retail_settlements &&
        !p.household_retail_settlements->premises_ids.empty())
      absent(p.household_retail_settlements->premises_ids.front());
    if (w.household_retail_settlements)
      for (const auto &row :
           w.household_retail_settlements->settlements.rows()) {
        if (std::holds_alternative<PrivateHouseholdRetailSeller>(
                row.second.terms.seller_ref) ||
            !row.second.consumed_lots.empty() || row.second.cost_of_goods)
          absent(row.first);
        source(row.second.terms.accepted_terms_ref);
        source(row.second.terms.custody_terms_ref);
      }
    if (w.spatial_carriers) {
      for (const auto &row : w.spatial_carriers->inventory_bindings.rows())
        source(row.second.source_ref);
    }
    return common_success();
  } catch (const Failure &f) {
    return {false, {}, f.error};
  }
}
} // namespace hanmo::resident
