#include "resident/kernel/catalog_section_observation.hpp"
#include "private_shop_household_custody.hpp"
#include "resident/domains/travel/cargo_handling.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include <set>
namespace hanmo::resident {
namespace {
// 内部传播原规则错误，不将缺失原源转成默认成功。
struct Failure {
  RuleError error;
};
// 取成功原读值；失败原样交给本层边界。
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
// 保留无返回值原验证的错误。
void take(Status v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
}
// 以具名规则错误拒绝当前原源。
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
// 使用唯一有理世界时间比较，不换算成本地时钟。
int cmp(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
// 政策身份须编号、版本与目录摘要全相等。
bool rule(const ActorFoundationRuleRef &a, const ActorFoundationRuleRef &b) {
  return a.id == b.id && a.version == b.version &&
         a.catalog_digest == b.catalog_digest;
}
// 只借真实 Common 原事实位置证明先后。
std::size_t order(const CommonStore &c, const SourceRef &s) {
  return take(read_actual_common_fact_position(c, s));
}
// 将内部错误返回原 Outcome，不提交任何候选。
template <class T, class F> Outcome<T> guard(F f) {
  try {
    return {true, f(), {}};
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
// 将内部验证错误保留为失败状态。
template <class F> Status status(F f) {
  try {
    f();
    return common_success();
  } catch (const Failure &e) {
    return {false, {}, e.error};
  }
}
// 同图核查仅接已明定 Shop2/3；原 operation source1/capability1 不改。
void context(const PrivateShopHouseholdCustodyContext &x) {
  const auto &o = x.market.operation;
  const auto &t = x.market.inventory_titles;
  if ((o.shops.catalog.version != 2 && o.shops.catalog.version != 3) ||
      o.shops.state.version != o.shops.catalog.version ||
      o.sources.schema_version != 1 || o.capabilities.schema_version != 1 ||
      o.shops.exact_operations != &o || x.state.version != 1 ||
      x.catalog.version != 1 ||
      x.catalog.catalog_digest !=
          o.shops.common.state().metadata.content_digest ||
      &t.common != &o.shops.common || &t.catalog != &o.shops.common_catalog ||
      t.foundation != &o.body.state ||
      t.foundation_catalog != &o.body.catalog ||
      t.family_catalog != &o.shops.family_catalog || !t.clock ||
      cmp(take(t.clock->read_current_moment()),
          take(o.clock.read_current_moment())))
    fail("PrivateShopCustodySameWorldRequired");
}
// 解析确切政策并反读真实装卸规则，拒绝未知铺位或伪版本。
const PrivateShopHouseholdCustodyPolicy &
policy(const PrivateShopHouseholdCustodyContext &x,
       const ActorFoundationRuleRef &ref) {
  const auto *p = x.catalog.policies.find(ref.id);
  if (!p || !rule(p->policy_ref, ref) || ref.id.empty() || ref.version != 1 ||
      ref.catalog_digest != x.catalog.catalog_digest ||
      p->premises_id.empty() ||
      p->inventory_selection !=
          PrivateShopCustodyInventorySelection::proprietor_personal_inventory)
    fail("PrivateShopExplicitCustodyPolicyRequired", ref.id);
  if (!x.market.operation.shops.catalog.premises.find(p->premises_id))
    fail("PrivateShopCustodyOriginalPremisesRequired", p->premises_id);
  const auto *r = x.travel.cargo_handling_rules.find(p->handling_rule_id);
  if (!r || r->version != p->handling_rule_version)
    fail("PrivateShopOriginalHandlingRuleRequired", p->handling_rule_id);
  take(validate_cargo_handling_rule(*r));
  return *p;
}
// 对实际托管逐边反读，不调用家庭全根，避免零售验证回环。
const PrivateShopHouseholdCustodyAcceptedSource &
read(const PrivateShopHouseholdCustodyContext &x, const MomentOwnerSource &s) {
  context(x);
  if (s.source_schema != 1 || s.source_ref.domain != "local_economy" ||
      s.source_ref.kind != "private_shop_household_custody_accepted")
    fail("PrivateShopCustodySourceRequired", s.source_ref.id);
  const auto *r = x.state.accepted.find(s.source_ref.id);
  if (!r || r->id != s.source_ref.id)
    fail("OwnedPrivateShopCustodySourceRequired", s.source_ref.id);
  const auto &p = policy(x, r->explicit_policy_ref);
  const auto &o = x.market.operation;
  const auto &c = o.shops.common;
  const auto *op = o.shops.state.operations.find(r->operation_id);
  const auto *a = take(read_original_private_shop_operation_acceptance(
      o, {private_shop_operation_started_source(r->operation_id), 2}));
  const auto *shop =
      take(read_original_private_shop_opening(o.shops, r->original_shop_epoch));
  if (!op || op->shop_id != shop->id || !op->staff_contract_id.empty() ||
      op->actor_id != r->proprietor_actor_id ||
      shop->terms.proprietor_actor_id != r->proprietor_actor_id ||
      shop->terms.premises_id != p.premises_id ||
      !same_source_ref(a->original_shop_epoch_ref, r->original_shop_epoch) ||
      cmp(r->accepted_at, a->accepted_at) ||
      order(c, private_shop_operation_started_source(op->id)) >=
          order(c, s.source_ref))
    fail("PrivateShopCustodyOriginalOperationMismatch", r->id);
  const auto *actor = c.actor(r->proprietor_actor_id);
  const auto &service = r->accepted_service;
  if (!actor || service.custody_inventory_id != actor->inventory_id ||
      service.custody_inventory_id == shop->inventory_id)
    fail("PrivateShopCustodyActualPersonalInventoryRequired", r->id);
  NativePrivateInventoryTitleRead titles(x.market.inventory_titles);
  auto title = take(titles.read_private_inventory_title_before_source(
      service.custody_inventory_id, s.source_ref));
  if (title.legal_owner.kind != Owner::Kind::actor ||
      title.legal_owner.id != actor->id || title.legal_owner.extra)
    fail("PrivateShopCustodyOriginalTitleMismatch", r->id);
  const MarketServiceDefinition *d = nullptr;
  for (const auto &candidate : o.markets.market_service_definitions)
    if (rule(candidate.definition_ref, a->market_definition_ref)) {
      if (d)
        fail("DuplicatePrivateShopCustodyMarketDefinition", r->id);
      d = &candidate;
    }
  // 装卸规则是显式目录引用，不冒充 Common 事实。
  const SourceRef handling{"travel", "cargo_handling_rule", p.handling_rule_id};
  if (!d || service.id != r->id ||
      service.market_ref != shop->terms.market_shop_id ||
      service.place_id != shop->terms.place_id ||
      service.service_slot != d->slot_ref ||
      !same_source_ref(service.custody_terms_ref, s.source_ref) ||
      !same_source_ref(service.service_usage_right_ref,
                       r->original_shop_epoch) ||
      !same_source_ref(service.handling_rule_ref, handling))
    fail("PrivateShopCustodyDerivedServiceMismatch", r->id);
  for (const auto &v : x.state.accepted.rows())
    if (v.second.operation_id == r->operation_id && v.first != r->id)
      fail("DuplicatePrivateShopCustodyConsent", r->operation_id);
  return *r;
}
// 一营业只允许一份真实同意；缺失和重复均拒。
const PrivateShopHouseholdCustodyAcceptedSource &
for_operation(const PrivateShopHouseholdCustodyContext &x, const StableId &id) {
  const PrivateShopHouseholdCustodyAcceptedSource *r = nullptr;
  for (const auto &v : x.state.accepted.rows())
    if (v.second.operation_id == id) {
      if (r)
        fail("DuplicatePrivateShopCustodyConsent", id);
      r = &v.second;
    }
  if (!r)
    fail("ExplicitPrivateShopCustodyAcceptanceRequired", id);
  return read(x, {private_shop_household_custody_source(r->id), 1});
}
} // namespace
SourceRef private_shop_household_custody_source(const StableId &id) {
  return {"local_economy", "private_shop_household_custody_accepted", id};
}
Status validate_private_shop_household_custody_catalog(
    const PrivateShopHouseholdCustodyContext &x) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 73, "PrivateShopHouseholdCustodyContext", "validate_private_shop_household_custody_catalog", &x);

  return status([&] {
    context(x);
    for (const auto &v : x.catalog.policies.rows()) {
      if (v.first != v.second.policy_ref.id)
        fail("PrivateShopCustodyPolicyKeyMismatch", v.first);
      policy(x, v.second.policy_ref);
    }
  });
}
Outcome<const PrivateShopHouseholdCustodyAcceptedSource *>
read_private_shop_household_custody(const PrivateShopHouseholdCustodyContext &x,
                                    const MomentOwnerSource &s) {
  return guard<const PrivateShopHouseholdCustodyAcceptedSource *>(
      [&] { return &read(x, s); });
}
Status validate_private_shop_household_custody_sources(
    const PrivateShopHouseholdCustodyContext &x) {
  return status([&] {
    take(validate_private_shop_household_custody_catalog(x));
    for (const auto &v : x.state.accepted.rows()) {
      if (v.first != v.second.id)
        fail("PrivateShopCustodySourceKeyMismatch", v.first);
      read(x, {private_shop_household_custody_source(v.first), 1});
    }
    for (const auto &v : x.market.operation.shops.common.state().facts.rows())
      if (v.second.domain == "local_economy" &&
          v.second.kind == "private_shop_household_custody_accepted" &&
          !x.state.accepted.find(v.first))
        fail("OrphanPrivateShopCustodySource", v.first);
  });
}
Status preview_private_shop_household_custody_choice(
    const PrivateShopHouseholdCustodyContext &x, const StableId &shop_id,
    const ActorFoundationRuleRef &explicit_policy, const StableId &actor_id) {
  return status([&] {
    context(x);
    const auto &p = policy(x, explicit_policy);
    const auto &o = x.market.operation;
    const auto &c = o.shops.common;
    const auto *shop = take(read_original_private_shop_opening(
        o.shops, {private_shop_module, private_shop_open_kind, shop_id}));
    const auto *actor = c.actor(actor_id);
    auto position = take(c.resolved_location(actor_id));
    const auto *place = std::get_if<SettlementLocation>(&position);
    if (shop->status != "open" || shop->terms.proprietor_actor_id != actor_id ||
        shop->terms.premises_id != p.premises_id || !actor || !actor->alive ||
        !actor->available || !actor->action_id.empty() || !place ||
        place->place_id != shop->terms.place_id ||
        actor->inventory_id.empty() ||
        actor->inventory_id == shop->inventory_id)
      fail("PrivateShopCustodyCurrentProprietorRequired", shop_id);
    NativePrivateInventoryTitleRead titles(x.market.inventory_titles);
    const auto title =
        take(titles.read_current_private_inventory_title(actor->inventory_id));
    if (title.legal_owner.kind != Owner::Kind::actor ||
        title.legal_owner.id != actor_id || title.legal_owner.extra)
      fail("PrivateShopCustodyCurrentTitleMismatch", actor->inventory_id);
    const auto *inventory = c.inventory(actor->inventory_id);
    if (!inventory)
      fail("PrivateShopCustodyCapacityRequired", actor->inventory_id);
    // 此处不产生营业、接受事实或标识；原营业预览仍核租金、库存和服务可用性。
  });
}
Outcome<StableId> accept_private_shop_household_custody_in_candidate(
    CommonStore &c, PrivateShopHouseholdCustodyState &state,
    const PrivateShopHouseholdCustodyContext &x, const StableId &operation,
    const ActorFoundationRuleRef &explicit_policy, const StableId &actor) {
  return guard<StableId>([&] {
    context(x);
    if (&c != &x.market.operation.shops.common || &state != &x.state)
      fail("PrivateShopCustodyCandidateMismatch", operation);
    const auto &p = policy(x, explicit_policy);
    const auto *op = x.market.operation.shops.state.operations.find(operation);
    if (!op || op->actor_id != actor)
      fail("PrivateShopCustodyActualActorRequired", operation);
    const auto basis = take(read_current_private_household_market_basis(
        x.market, {op->shop_id, operation}));
    const auto at = take(x.market.operation.clock.read_current_moment());
    if (basis.shop->terms.premises_id != p.premises_id ||
        basis.shop->terms.proprietor_actor_id != actor ||
        cmp(at, basis.original_operation_acceptance->accepted_at))
      fail("PrivateShopCustodySameOperationAcceptanceRequired", operation);
    for (const auto &v : state.accepted.rows())
      if (v.second.operation_id == operation) {
        const auto &old =
            read(x, {private_shop_household_custody_source(v.first), 1});
        if (old.proprietor_actor_id != actor ||
            !rule(old.explicit_policy_ref, explicit_policy))
          fail("PrivateShopCustodyAcceptanceConflict", operation);
        return old.id;
      }
    const auto id = take(c.next_id("private_shop_household_custody"));
    PrivateShopHouseholdCustodyAcceptedSource row;
    row.id = id;
    row.operation_id = operation;
    row.proprietor_actor_id = actor;
    row.original_shop_epoch = basis.original_usage_right;
    row.explicit_policy_ref = explicit_policy;
    row.accepted_at = at;
    row.accepted_service = {
        id,
        basis.shop->terms.market_shop_id,
        basis.shop->terms.place_id,
        basis.proprietor_personal_inventory.inventory_id,
        basis.original_market_definition->slot_ref,
        private_shop_household_custody_source(id),
        basis.original_usage_right,
        {"travel", "cargo_handling_rule", p.handling_rule_id}};
    if (!state.accepted.insert(id, std::move(row)))
      fail("DuplicatePrivateShopCustodyId", id);
    take(c.append_fact(
        {id, "private_shop_household_custody_accepted", "local_economy"}));
    read(x, {private_shop_household_custody_source(id), 1});
    return id;
  });
}
Outcome<const HouseholdMarketServiceDefinition *>
read_private_shop_household_service_current(
    const PrivateShopHouseholdCustodyContext &x,
    const PrivateHouseholdRetailSeller &s) {
  return guard<const HouseholdMarketServiceDefinition *>([&] {
    auto basis = take(read_current_private_household_market_basis(x.market, s));
    const auto &r = for_operation(x, s.operation_id);
    if (r.proprietor_actor_id != basis.shop->terms.proprietor_actor_id)
      fail("PrivateShopCustodySellerMismatch", s.shop_id);
    return &r.accepted_service;
  });
}
Outcome<const HouseholdMarketServiceDefinition *>
read_private_shop_household_service_accepted(
    const PrivateShopHouseholdCustodyContext &x,
    const HouseholdPurchaseAcceptedTerms &a) {
  return guard<const HouseholdMarketServiceDefinition *>([&] {
    const auto *s = std::get_if<PrivateHouseholdRetailSeller>(&a.seller);
    if (!s)
      fail("PrivateHouseholdSellerRequired", a.source.id);
    take(read_private_household_market_basis_before_purchase(
        x.market, x.market.purchases, a));
    const auto &r = for_operation(x, s->operation_id);
    if (order(x.market.operation.shops.common,
              private_shop_household_custody_source(r.id)) >=
        order(x.market.operation.shops.common, a.source))
      fail("PrivateShopCustodyBeforePurchaseRequired", a.source.id);
    return &r.accepted_service;
  });
}
} // namespace hanmo::resident
