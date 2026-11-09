// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "market_service_original.hpp"
#include "household_purchase_contract.hpp"
#include "resident/domains/commerce/private_shop_absence.hpp"
#include "resident/kernel/native_economic_facility_roots.hpp"
#include "resident/kernel/native_private_shop_operations.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace market_original {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const StableId &code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
void take(Status r) {
  if (!r.ok)
    throw Failure{std::move(r.error)};
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
// 仅解引用真实公市权源；未选分支不得以空源冒充公市准入。
const PrivateMarketServiceOriginalRead &
private_business(const MarketServiceOriginalContext &x) {
  if (!x.private_business)
    fail("PrivateMarketOriginalBusinessRequired");
  return *x.private_business;
}
const PublicMarketAuthorityRead &
public_authority(const MarketServiceOriginalContext &x) {
  if (!x.public_authority)
    throw Failure{common_failure("PublicMarketAuthorityReadRequired", {},
                                 ErrorKind::unsupported_capability)
                      .error};
  return *x.public_authority;
}
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
bool rule(const ActorFoundationRuleRef &a, const ActorFoundationRuleRef &b) {
  return std::tie(a.id, a.version, a.catalog_digest) ==
         std::tie(b.id, b.version, b.catalog_digest);
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
bool spans(const std::vector<MomentSpan> &a, const std::vector<MomentSpan> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t n = 0; n < a.size(); ++n)
    if (cmp(a[n].from, b[n].from) || cmp(a[n].to, b[n].to))
      return false;
  return true;
}
bool definition(const MarketServiceDefinition &a,
                const MarketServiceDefinition &b) {
  return rule(a.definition_ref, b.definition_ref) && a.slot_ref == b.slot_ref &&
         a.place_ref == b.place_ref &&
         same_source_ref(a.asset_source_ref, b.asset_source_ref) &&
         a.physical_service_slots == b.physical_service_slots &&
         spans(a.opening_intervals, b.opening_intervals) &&
         take(compare_fractions(a.max_claim_half_days.value,
                                b.max_claim_half_days.value)) == 0 &&
         a.staffing_rule == b.staffing_rule &&
         rule(a.usage_rule_ref, b.usage_rule_ref);
}
std::size_t order(const CommonStore &c, const SourceRef &r) {
  const auto *f = c.state().facts.find(r.id);
  if (!f || f->kind != r.kind || f->domain != r.domain)
    fail("MarketOriginalSourceMissing", r.id);
  const auto &ids = c.state().facts.order();
  return static_cast<std::size_t>(std::find(ids.begin(), ids.end(), r.id) -
                                  ids.begin());
}
const MarketServiceDefinition &locked(const MarketServiceOriginalContext &x,
                                      const MarketServiceDefinition &d) {
  take(validate_market_service_original_physical_sources(x));
  const MarketServiceDefinition *found = nullptr;
  for (const auto &row : x.catalog.market_service_definitions)
    if (rule(row.definition_ref, d.definition_ref)) {
      if (found)
        fail("DuplicateLockedMarketDefinition", d.definition_ref.id);
      found = &row;
    }
  if (!found || !definition(*found, d) ||
      d.definition_ref.catalog_digest !=
          x.common.state().metadata.content_digest ||
      d.slot_ref.slot_kind != "market_service" || d.physical_service_slots <= 0)
    fail("MarketServiceContentLockMismatch", d.definition_ref.id);
  if (d.slot_ref.provider_domain == "local_economy")
    public_authority(x);
  return *found;
}
StableId asset(const MarketServiceOriginalContext &x,
               const MarketServiceDefinition &d) {
  locked(x, d);
  if (d.slot_ref.provider_domain == "local_economy" &&
      d.asset_source_ref.domain == "local_economy" &&
      d.asset_source_ref.kind == "initial_production_asset") {
    if (!x.assets)
      fail("MarketOriginalProductionAssetsRequired", d.asset_source_ref.id);
    const ProductionAsset *found = nullptr;
    for (const auto &row : x.assets->assets.rows())
      if (row.second.source_key == d.asset_source_ref.id) {
        if (found)
          fail("DuplicateMarketAssetSource", d.asset_source_ref.id);
        found = &row.second;
      }
    if (!found || found->kind != ProductionAssetKind::facility ||
        found->site_id != d.place_ref)
      fail("MarketAssetInitializationMissing", d.asset_source_ref.id);
    return found->id;
  }
  if (d.slot_ref.provider_domain == "commerce" &&
      d.asset_source_ref.domain == "commerce" &&
      d.asset_source_ref.kind == "initial_shop_premises") {
    const auto *p = x.shops_catalog
                        ? x.shops_catalog->premises.find(d.asset_source_ref.id)
                        : nullptr;
    if (!p || p->place_id != d.place_ref)
      fail("MarketPremisesInitializationMissing", d.asset_source_ref.id);
    return p->id;
  }
  if (d.asset_source_ref.kind == "economic_facility_installed") {
    if (!x.installed)
      fail("MarketOriginalInstalledFacilitiesRequired", d.asset_source_ref.id);
    for (const auto &f : x.installed->facilities)
      if (const auto *i =
              std::get_if<InstalledPhysicalFacility>(&f.initialization_basis))
        if (i->installation_evidence.installation_id == d.asset_source_ref.id &&
            f.asset_owner_domain == d.asset_source_ref.domain) {
          const auto &e = i->installation_evidence;
          if (f.place_ref != d.place_ref ||
              e.world_id != x.common.state().identity.world_id ||
              e.catalog_identity.content_digest !=
                  x.common.state().metadata.content_digest ||
              cmp(e.installed_at, x.current) > 0)
            fail("MarketInstalledAssetMismatch", d.asset_source_ref.id);
          return f.original_asset_ref;
        }
  }
  fail("UnsupportedMarketAssetInitialization", d.asset_source_ref.id);
}
MarketServiceConditionBasis
condition(const MarketServiceOriginalContext &x,
          const MarketServiceDefinition &d, const WorldMoment &at,
          const SourceRef &adoption,
          const std::optional<SourceRef> &actual_capacity_cut) {
  auto id = asset(x, d);
  if (!x.condition_catalog)
    return MarketNotConditioned{d.definition_ref};
  const ProductionFacilityConditionSeed *seed = nullptr;
  for (const auto &row : x.condition_catalog->seeds.rows())
    if (row.second.original_asset_ref == id) {
      if (seed)
        fail("DuplicateMarketCondition", id);
      seed = &row.second;
    }
  if (!seed)
    return MarketNotConditioned{d.definition_ref};
  Integer revision = 1;
  const bool owned_body = adoption.domain == "actor_foundation" &&
                          adoption.kind == "condition_receipt";
  std::size_t adopted = 0;
  if (owned_body) {
    auto original =
        take(private_business(x).read_private_market_change_source(adoption));
    if (original.source_schema != 2)
      fail("MarketOwnedConditionSourceRequired", adoption.id);
    if (actual_capacity_cut)
      adopted = order(x.common, *actual_capacity_cut);
    else {
      if (cmp(at, x.current))
        fail("MarketActualCapacityCutRequired", adoption.id);
      adopted = x.common.state().facts.order().size();
    }
  } else
    adopted = order(x.common, adoption);
  for (const auto &row : x.conditions->changes.rows())
    if (row.second.facility_ref == id) {
      auto before = cmp(row.second.occurred_at, at);
      if (before < 0 ||
          (before == 0 && order(x.common, {d.slot_ref.provider_domain,
                                           "facility_condition_change",
                                           row.first}) <= adopted))
        revision = row.second.new_revision;
    }
  auto actual = take(read_production_facility_condition_revision(
      *x.conditions, *x.condition_catalog, *x.installed, seed->condition_ref,
      revision));
  return MarketOriginalCondition{seed->condition_ref, revision,
                                 actual.last_change_ref};
}
// 逐项核对原始资产、工时与条件依据；任一来源不一致即拒绝等价。
bool basis(const MarketServiceOriginalInputs &a,
           const MarketServiceCapacitySource &b) {
  if (a.asset.index() != b.asset_basis.index() ||
      a.condition.index() != b.condition_basis.index() ||
      a.staffing.size() != b.staffing_bindings.size())
    return false;
  bool same_asset = std::visit(
      [&](const auto &l) {
        using T = std::decay_t<decltype(l)>;
        const auto &r = std::get<T>(b.asset_basis);
        if constexpr (std::is_same_v<T, PublicMarketAssetBasis>)
          return l.original_asset_ref == r.original_asset_ref &&
                 l.original_organization_ref == r.original_organization_ref &&
                 same_source_ref(l.original_asset_right_ref,
                                 r.original_asset_right_ref);
        else
          return l.original_shop_ref == r.original_shop_ref &&
                 same_source_ref(l.original_shop_epoch_ref,
                                 r.original_shop_epoch_ref) &&
                 l.original_premises_ref == r.original_premises_ref &&
                 same_source_ref(l.original_lease_or_title_ref,
                                 r.original_lease_or_title_ref);
      },
      a.asset);
  if (!same_asset ||
      !rule(a.opening.original_opening_rule_ref,
            b.opening_basis.original_opening_rule_ref) ||
      !same_source_ref(a.opening.accepted_opening_source_ref,
                       b.opening_basis.accepted_opening_source_ref) ||
      !spans(a.opening.committed_intervals,
             b.opening_basis.committed_intervals) ||
      !rule(a.usage.usage_rule_ref, b.usage_right_basis.usage_rule_ref) ||
      !same_source_ref(a.usage.original_right_ref,
                       b.usage_right_basis.original_right_ref) ||
      !same_source_ref(
          a.usage.original_acceptance_or_approval_ref,
          b.usage_right_basis.original_acceptance_or_approval_ref) ||
      !spans(a.usage.valid_intervals, b.usage_right_basis.valid_intervals))
    return false;
  for (std::size_t n = 0; n < a.staffing.size(); ++n) {
    if (a.staffing[n].index() != b.staffing_bindings[n].index())
      return false;
    if (const auto *p =
            std::get_if<PublicMarketStaffingSource>(&a.staffing[n])) {
      if (p->staffing_binding_ref !=
          std::get<PublicMarketStaffingSource>(b.staffing_bindings[n])
              .staffing_binding_ref)
        return false;
    } else {
      const auto &private_source =
          std::get<PrivateMarketOperationSource>(a.staffing[n]);
      const auto &q =
          std::get<PrivateMarketOperationSource>(b.staffing_bindings[n]);
      if (private_source.operation_ref != q.operation_ref ||
          bool(private_source.original_staff_participation_ref) !=
              bool(q.original_staff_participation_ref) ||
          (private_source.original_staff_participation_ref &&
           !same_source_ref(*private_source.original_staff_participation_ref,
                            *q.original_staff_participation_ref)))
        return false;
    }
  }
  return std::visit(
      [&](const auto &l) {
        using T = std::decay_t<decltype(l)>;
        const auto &r = std::get<T>(b.condition_basis);
        if constexpr (std::is_same_v<T, MarketNotConditioned>)
          return rule(l.asset_definition_ref, r.asset_definition_ref);
        else
          return l.condition_ref == r.condition_ref &&
                 l.condition_version == r.condition_version &&
                 same_source_ref(l.original_change_source_ref,
                                 r.original_change_source_ref);
      },
      a.condition);
}
} // namespace market_original
using namespace market_original;
// 条件根只能由原 09 有限选择启用；没有选择时仍检查真实 Common 残留。
Status validate_market_service_condition_selection(
    const CatalogIdentity &identity, const CommonStore &common,
    const ProductionFacilityConditionsState *conditions) {
  return guard<Done>([&] {
    const auto &metadata = common.state().metadata;
    if (identity.content_id != metadata.content_id ||
        identity.content_version != metadata.content_version ||
        identity.content_digest != metadata.content_digest ||
        identity.ruleset_id != metadata.ruleset_id)
      fail("MarketOriginalCatalogIdentityMismatch");
    const auto selected =
        take(selected_native_economic_facility_versions(identity));
    if (bool(selected.conditions) != bool(conditions))
      fail("MarketOriginalConditionSelectionMismatch");
    if (!selected.conditions)
      for (const auto &row : common.state().facts.rows())
        if (row.second.kind == "facility_condition_change")
          fail("MarketUnselectedConditionFact", row.first);
    return Done{};
  });
}
Status validate_market_service_original_physical_sources(
    const MarketServiceOriginalContext &x) {
  return guard<Done>([&] {
    const auto shop_selection =
        take(selected_native_private_shop_operation_versions(x.identity));
    if (bool(shop_selection.shops) != bool(x.shops_catalog) ||
        (x.shops_catalog && x.shops_catalog->version != shop_selection.shops))
      fail("PrivateMarketOriginalCatalogSelectionMismatch");
    if (bool(x.shops_catalog) != bool(x.private_business))
      fail("PrivateMarketOriginalSelectionMismatch");
    if (!x.private_business) {
      const auto *w = x.unselected_shops_world;
      if (!w || !w->catalog || &w->common != &x.common ||
          &w->catalog->identity != &x.identity ||
          w->catalog->household_purchases.get() != &x.catalog ||
          !w->household_purchases || &*w->household_purchases != &x.purchases)
        fail("PrivateMarketOriginalAbsenceWorldRequired");
      take(validate_unselected_private_shop_sources(*w));
    }

    take(validate_market_service_condition_selection(x.identity, x.common,
                                                     x.conditions));
    const auto selected =
        take(selected_native_economic_facility_versions(x.identity));
    bool assets = false;
    for (const auto &row : x.identity.module_versions)
      if (row.first == "production_management") {
        if (assets || row.second != "1")
          fail("MarketOriginalProductionModuleMismatch", row.first);
        assets = true;
      }
    if (assets != bool(x.assets) || (x.assets && x.assets->version != 1) ||
        bool(selected.facilities) != bool(x.installed) ||
        bool(selected.conditions) != bool(x.condition_catalog) ||
        (selected.facilities && !assets) ||
        (x.installed && x.installed->schema_version != selected.facilities))
      fail("MarketOriginalPhysicalSelectionMismatch");
    const auto selected_source = [&](const StableId &domain,
                                     const StableId &kind, const StableId &id) {
      if (!assets && domain == "local_economy" &&
          kind == "initial_production_asset")
        fail("MarketUnselectedProductionAsset", id);
      if (!selected.facilities && (kind == "economic_facility_installed" ||
                                   kind == "economic_facility_capacity" ||
                                   kind == "facility_retention"))
        fail("MarketUnselectedPhysicalSource", id);
      if (!selected.conditions && kind == "facility_condition_change")
        fail("MarketUnselectedConditionSource", id);
    };
    const auto source = [&](const SourceRef &r) {
      selected_source(r.domain, r.kind, r.id);
    };
    const auto definition = [&](const MarketServiceDefinition &d) {
      source(d.asset_source_ref);
      if (!assets && d.slot_ref.provider_domain == "local_economy")
        fail("MarketUnselectedPublicAsset", d.definition_ref.id);
    };
    for (const auto &d : x.catalog.market_service_definitions)
      definition(d);
    for (const auto &d : x.purchases.market_service_definitions)
      definition(d);
    for (const auto &row : x.catalog.market_services.rows()) {
      source(row.second.custody_terms_ref);
      source(row.second.service_usage_right_ref);
      source(row.second.handling_rule_ref);
    }
    for (const auto *rows :
         {&x.purchases.market_service_sources.public_sources,
          &x.purchases.market_service_sources.private_sources})
      for (const auto &s : *rows) {
        if (!selected.conditions &&
            (std::holds_alternative<MarketOriginalCondition>(
                 s.condition_basis) ||
             std::holds_alternative<MarketServiceFacilityChanged>(
                 s.change_cause)))
          fail("MarketUnselectedConditionBasis", s.source_id);
        std::visit(
            [&](const auto &asset) {
              using T = std::decay_t<decltype(asset)>;
              if constexpr (std::is_same_v<T, PublicMarketAssetBasis>) {
                if (!assets)
                  fail("MarketUnselectedPublicAsset", s.source_id);
                source(asset.original_asset_right_ref);
              } else {
                source(asset.original_shop_epoch_ref);
                source(asset.original_lease_or_title_ref);
              }
            },
            s.asset_basis);
        if (s.previous_source_ref)
          source(*s.previous_source_ref);
        source(s.opening_basis.accepted_opening_source_ref);
        source(s.usage_right_basis.original_right_ref);
        source(s.usage_right_basis.original_acceptance_or_approval_ref);
        std::visit(
            [&](const auto &change) {
              using T = std::decay_t<decltype(change)>;
              if constexpr (std::is_same_v<T, MarketServiceInitialized>) {
                source(change.original_asset_initialization_ref);
                source(change.original_opening_acceptance_ref);
              } else if constexpr (std::is_same_v<T,
                                                  MarketServiceOpeningChanged>)
                source(change.original_opening_change_ref);
              else if constexpr (std::is_same_v<T,
                                                MarketServiceStaffingChanged>)
                source(change.original_staffing_event_ref);
              else if constexpr (std::is_same_v<T,
                                                MarketServiceFacilityChanged>)
                source(change.original_condition_result_ref);
              else
                source(change.original_right_change_ref);
            },
            s.change_cause);
        if (const auto *condition =
                std::get_if<MarketOriginalCondition>(&s.condition_basis))
          source(condition->original_change_source_ref);
        for (const auto &binding : s.staffing_bindings)
          if (const auto *private_staff =
                  std::get_if<PrivateMarketOperationSource>(&binding))
            if (private_staff->original_staff_participation_ref)
              source(*private_staff->original_staff_participation_ref);
      }
    // 家户终局与分段保存的原原因同样不能隐藏未选的条件源。
    for (const auto &row : x.purchases.named_service_events.rows())
      if (row.second.cause_ref)
        source(*row.second.cause_ref);
    for (const auto &row : x.purchases.phase_changes.rows())
      source(row.second.actual_cause);
    for (const auto &row : x.purchases.service_results.rows())
      source(row.second.coverage_source);
    for (const auto &row : x.purchases.handovers.rows())
      source(row.second.actual_delivery_source);
    for (const auto &s : x.purchases.service_segments) {
      source(s.actual_coverage_source);
      for (const auto &r : s.exertion_source_refs)
        source(r);
    }
    for (const auto &row : x.common.state().facts.rows()) {
      const auto &head = row.second;
      if (!assets && head.domain == "local_economy" &&
          head.kind.rfind("production_configuration_", 0) == 0)
        fail("MarketUnselectedProductionFact", row.first);
      selected_source(head.domain, head.kind, head.id);
    }
    return Done{};
  });
}
Outcome<FacilitySourceRef>
NativeMarketServiceOriginalRead::read_original_market_change_source(
    const FacilitySlotRef &slot,
    const MarketServiceChangeCause &changed) const {
  return guard<FacilitySourceRef>([&] {
    take(validate_market_service_original_physical_sources(x_));
    const auto original = cause(changed);
    if (slot.provider_domain == "commerce")
      return take(
          private_business(x_).read_private_market_change_source(original));
    take(read_original_market_change_at(slot, changed));
    return FacilitySourceRef{original, 1};
  });
}
Outcome<MarketServiceChangeCause>
NativeMarketServiceOriginalRead::resolve_original_market_service_change(
    const MarketServiceDefinition &d, const FacilitySourceRef &r) const {
  return guard<MarketServiceChangeCause>([&] {
    locked(x_, d);
    if (d.slot_ref.provider_domain != "commerce" && r.source_schema != 1)
      fail("MarketChangeSchemaMismatch", r.source_ref.id);
    MarketServiceChangeCause result;
    if (d.slot_ref.provider_domain == "commerce")
      result =
          take(private_business(x_).read_private_market_service_change(d, r));
    else if (r.source_ref.domain == "local_economy" &&
             r.source_ref.kind == "public_market_staffing")
      result = MarketServiceStaffingChanged{r.source_ref};
    else if (r.source_ref.kind == "facility_condition_change")
      result = MarketServiceFacilityChanged{r.source_ref};
    else
      result =
          take(public_authority(x_).read_public_market_service_change(d, r));
    if (!same_source_ref(cause(result), r.source_ref))
      fail("MarketChangeOriginalSourceMismatch", r.source_ref.id);
    take(read_original_market_change_at(d.slot_ref, result));
    return result;
  });
}
Outcome<WorldMoment>
NativeMarketServiceOriginalRead::read_original_market_change_at(
    const FacilitySlotRef &slot,
    const MarketServiceChangeCause &changed) const {
  return guard<WorldMoment>([&] {
    take(validate_market_service_original_physical_sources(x_));
    const auto original = cause(changed);
    if (slot.provider_domain == "commerce")
      return take(
          private_business(x_).read_private_market_change_moment(original));
    if (slot.provider_domain != "local_economy" ||
        slot.slot_kind != "market_service")
      fail("UnsupportedMarketProvider", slot.facility_id);
    const auto &authority = public_authority(x_);
    if (const auto *staff =
            std::get_if<MarketServiceStaffingChanged>(&changed)) {
      const auto *event = x_.purchases.public_staffing_events.find(
          staff->original_staffing_event_ref.id);
      if (!event || original.domain != "local_economy" ||
          original.kind != "public_market_staffing")
        fail("PublicMarketStaffingChangeMissing", original.id);
      order(x_.common, original);
      const auto *b =
          x_.purchases.public_staffing_bindings.find(event->binding_ref);
      const auto *o =
          b ? x_.purchases.public_staffing_offers.find(b->offer_ref) : nullptr;
      bool owns = false;
      if (o)
        for (const auto &d : x_.catalog.market_service_definitions)
          if (d.slot_ref == slot &&
              rule(d.definition_ref, o->terms.market_definition_ref))
            owns = true;
      if (!owns)
        fail("PublicMarketStaffingSlotMismatch", original.id);
      return event->occurred_at;
    }
    if (std::holds_alternative<MarketServiceFacilityChanged>(changed)) {
      const auto *c =
          x_.conditions ? x_.conditions->changes.find(original.id) : nullptr;
      if (!c || original.kind != "facility_condition_change")
        fail("MarketFacilityChangeMissing", original.id);
      bool owns = false;
      for (const auto &d : x_.catalog.market_service_definitions)
        if (d.slot_ref == slot && asset(x_, d) == c->facility_ref)
          owns = true;
      if (!owns)
        fail("MarketFacilityChangeSlotMismatch", original.id);
      order(x_.common, original);
      return c->occurred_at;
    }
    return take(authority.read_public_market_authority_change_moment(original));
  });
}
static Outcome<MarketServiceOriginalInputs> read_market_original_inputs(
    const MarketServiceOriginalContext &x, const MarketServiceDefinition &d,
    const MarketServiceChangeCause &changed,
    const std::optional<SourceRef> &actual_capacity_cut) {
  return guard<MarketServiceOriginalInputs>([&] {
    NativeMarketServiceOriginalRead owner(x);
    locked(x, d);
    auto at = take(owner.read_original_market_change_at(d.slot_ref, changed));
    const auto original = cause(changed);
    auto original_asset = asset(x, d);
    MarketServiceOriginalInputs result;
    if (d.slot_ref.provider_domain == "local_economy") {
      auto authority =
          take(public_authority(x).read_public_market_authority_at_adoption(
              d, original, at));
      take(public_authority(x).validate_public_market_authority_history(
          d, authority, original, at));
      if (!x.assets)
        fail("MarketOriginalProductionAssetsRequired", original_asset);
      const auto *a = x.assets->assets.find(original_asset);
      if (!a || authority.asset_basis.original_asset_ref != a->id ||
          a->owner.kind != Owner::Kind::organization ||
          a->owner.id != authority.asset_basis.original_organization_ref)
        fail("PublicMarketAssetAuthorityMismatch", original_asset);
      result.asset = authority.asset_basis;
      result.opening = authority.opening_basis;
      result.usage = authority.usage_right_basis;
      auto adopted = order(x.common, original);
      std::vector<StableId> ids;
      for (const auto &row : x.purchases.public_staffing_bindings.rows()) {
        const auto &b = row.second;
        const auto *o = x.purchases.public_staffing_offers.find(b.offer_ref);
        if (!o || !rule(o->terms.market_definition_ref, d.definition_ref))
          continue;
        const auto *event =
            x.purchases.public_staffing_events.find(b.accepted_event_ref.id);
        if (!event)
          fail("MarketStaffingAcceptanceMissing", b.binding_id);
        if (cmp(event->occurred_at, at) < 0 ||
            (cmp(event->occurred_at, at) == 0 &&
             order(x.common, b.accepted_event_ref) <= adopted))
          ids.push_back(b.binding_id);
      }
      std::sort(ids.begin(), ids.end());
      for (const auto &id : ids)
        result.staffing.push_back(PublicMarketStaffingSource{id});
    } else {
      auto original_private =
          take(private_business(x).read_private_market_basis(d, changed));
      if (original_private.asset.original_premises_ref != original_asset)
        fail("PrivateMarketPremisesMismatch", original_asset);
      result.asset = original_private.asset;
      result.opening = original_private.opening;
      result.usage = original_private.usage;
      for (const auto &binding : original_private.staffing)
        result.staffing.push_back(binding);
    }
    result.condition = condition(x, d, at, original, actual_capacity_cut);
    if (const auto *init = std::get_if<MarketServiceInitialized>(&changed))
      if (!same_source_ref(init->original_asset_initialization_ref,
                           d.asset_source_ref) ||
          !same_source_ref(init->original_opening_acceptance_ref,
                           result.opening.accepted_opening_source_ref))
        fail("MarketInitializationBasisMismatch", d.definition_ref.id);
    return result;
  });
}
Outcome<MarketServiceOriginalInputs>
NativeMarketServiceOriginalRead::read_original_market_service_inputs(
    const MarketServiceDefinition &d, const MarketServiceChangeCause &c) const {
  return read_market_original_inputs(x_, d, c, std::nullopt);
}
Status NativeMarketServiceOriginalRead::validate_original_market_service_inputs(
    const MarketServiceDefinition &d,
    const MarketServiceCapacitySource &s) const {
  return guard<Done>([&] {
    if (d.slot_ref != s.slot_ref || !rule(d.definition_ref, s.definition_ref))
      fail("MarketDefinitionBasisMismatch", s.source_id);
    auto original = take(read_market_original_inputs(
        x_, d, s.change_cause,
        s.source_id.empty() ? std::nullopt
                            : std::optional<SourceRef>{
                                  {s.slot_ref.provider_domain,
                                   "market_service_capacity", s.source_id}}));
    if (!basis(original, s))
      fail("MarketOriginalBasisMismatch", s.source_id);
    return Done{};
  });
}
Outcome<std::vector<MarketServiceWorkerSegment>>
NativeMarketServiceOriginalRead::read_private_market_service_workers(
    const PrivateMarketOperationSource &p, const MomentSpan &span) const {
  if (!x_.private_business)
    return {false,
            {},
            common_failure("PrivateMarketOriginalBusinessRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  return x_.private_business->read_private_market_workers(p, span);
}
Outcome<std::vector<MarketServiceConditionSegment>>
NativeMarketServiceOriginalRead::read_market_service_condition(
    const MarketServiceDefinition &d, const MarketServiceConditionBasis &b,
    const MomentSpan &window) const {
  return guard<std::vector<MarketServiceConditionSegment>>([&] {
    auto original_asset = asset(x_, d);
    if (cmp(window.from, window.to) >= 0)
      fail("InvalidMarketConditionWindow");
    if (const auto *unconditioned = std::get_if<MarketNotConditioned>(&b)) {
      if (!rule(unconditioned->asset_definition_ref, d.definition_ref))
        fail("MarketConditionDefinitionMismatch");
      if (x_.condition_catalog)
        for (const auto &seed : x_.condition_catalog->seeds.rows())
          if (seed.second.original_asset_ref == original_asset)
            fail("MarketConditionCannotBeOmitted", original_asset);
      return std::vector<MarketServiceConditionSegment>{
          {window, d.physical_service_slots, d.asset_source_ref}};
    }
    const auto &original = std::get<MarketOriginalCondition>(b);
    if (!x_.condition_catalog || !x_.conditions || !x_.installed)
      fail("MarketOriginalConditionsRequired", original.condition_ref);
    const auto *seed = x_.condition_catalog->seeds.find(original.condition_ref);
    if (!seed || seed->original_asset_ref != original_asset)
      fail("MarketConditionAssetMismatch", original_asset);
    auto condition = take(read_production_facility_condition_revision(
        *x_.conditions, *x_.condition_catalog, *x_.installed,
        original.condition_ref, original.condition_version));
    if (!same_source_ref(condition.last_change_ref,
                         original.original_change_source_ref))
      fail("MarketConditionRevisionMismatch", original_asset);
    const FacilityBaseSlotCapacity *slot = nullptr;
    for (const auto &s : seed->base_capacity_by_slot)
      if (s.slot_kind == "market_service" &&
          s.capacity_unit_id == market_service_unit) {
        if (slot)
          fail("DuplicateMarketConditionSlot", original_asset);
        slot = &s;
      }
    if (!slot || slot->units != d.physical_service_slots)
      fail("MarketConditionPhysicalConfigurationMissing", original_asset);
    auto units = take(
        facility_condition_capacity(slot->units, condition.integrity,
                                    condition.max_integrity, x_.arithmetic));
    return std::vector<MarketServiceConditionSegment>{
        {window, units, original.original_change_source_ref}};
  });
}
Outcome<bool> NativeMarketServiceOriginalRead::market_claim_conflict_visible(
    const ReservationRef &observer, const FacilityClaim2 &claim) const {
  return guard<bool>([&] {
    if (observer.kind != ReservationRef::Kind::domain_activity ||
        observer.domain_id != "local_economy" ||
        (observer.activity_kind != "household_purchase" &&
         observer.activity_kind != "household_purchase_plan"))
      fail("UnsupportedMarketConflictObserver", observer.id);
    const HouseholdPurchasePlan *plan = nullptr;
    if (observer.activity_kind == "household_purchase_plan")
      plan = x_.purchases.plans.find(observer.id);
    else {
      const auto *p = x_.purchases.purchases.find(observer.id);
      const auto *a =
          p ? x_.purchases.accepted_terms.find(p->accepted_terms_ref.id)
            : nullptr;
      if (a)
        plan = &a->plan;
    }
    if (!plan)
      fail("MarketConflictObserverMissing", observer.id);
    const auto *other = x_.purchases.purchases.find(claim.holder.id);
    const auto *a =
        other ? x_.purchases.accepted_terms.find(other->accepted_terms_ref.id)
              : nullptr;
    return a && a->plan.household_id == plan->household_id;
  });
}
Status adopt_locked_market_service_definitions_in_candidate(
    HouseholdPurchaseState &state, const HouseholdPurchaseCatalog &catalog,
    const CommonStore &common) {
  return guard<Done>([&] {
    if (!state.market_service_definitions.empty())
      fail("MarketDefinitionsAlreadyAdopted");
    std::set<std::tuple<StableId, StableId, StableId>> slots;
    for (const auto &d : catalog.market_service_definitions) {
      if (d.definition_ref.catalog_digest !=
              common.state().metadata.content_digest ||
          d.definition_ref.version <= 0 || d.physical_service_slots <= 0 ||
          !slots
               .emplace(d.slot_ref.provider_domain, d.slot_ref.facility_id,
                        d.slot_ref.slot_kind)
               .second)
        fail("InvalidMarketDefinitionAdoption", d.definition_ref.id);
    }
    state.market_service_definitions = catalog.market_service_definitions;
    return Done{};
  });
}
} // namespace hanmo::resident
