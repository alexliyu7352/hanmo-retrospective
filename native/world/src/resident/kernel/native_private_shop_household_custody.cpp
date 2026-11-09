#include "resident/kernel/catalog_section_observation.hpp"
#include "native_private_shop_household_custody.hpp"
#include "exact_complete_world.hpp"
#include "native_private_shop_custody_source_pins.hpp"
#include "native_private_shop_operations.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/domains/local_economy/household_foundation_lease.hpp"
#include "resident/domains/local_economy/private_shop_household_custody_fields.hpp"
#include "resident/domains/travel/cargo_handling.hpp"
#include "resident/storage/common_codec.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <optional>
namespace hanmo::resident {
namespace {
constexpr const char *module = "private_shop_household_custody_sources";
constexpr const char *input_name = "private_shop_household_custody";
// 只读显式作者字段；内容摘要来自已锁定目录，不允许作者提交未来事实或容器。
Outcome<std::optional<PrivateShopHouseholdCustodyCatalog>>
read_catalog_input(const BoundaryValue &scenario,
                   const CatalogIdentity &identity) {
  try {
    using namespace boundary;
    const BoundaryValue *input = nullptr;
    for (const auto &field : object(scenario, "scenario")) {
      if (field.first != input_name)
        continue;
      if (input)
        fail("DuplicatePrivateShopCustodySelection", input_name);
      input = &field.second;
    }
    if (!input)
      return {true, std::nullopt, {}};
    fields(*input, {"profile", "policies"}, {}, input_name);
    if (object(*input, input_name).size() != 2 ||
        get_string(*input, "profile", input_name) !=
            "original_private_shop_household_custody_v1")
      unsupported("UnsupportedPrivateShopCustodySelection", input_name);
    if (identity.content_digest.empty())
      fail("PrivateShopCustodyLockedIdentityRequired", input_name);
    std::optional<ScopedCheckpointJsonDecode> scope;
    if (ScopedCheckpointBoundaryDecode::current())
      scope.emplace(CheckpointCatalogJsonKind::private_shop_household_custody);
    PrivateShopHouseholdCustodyCatalog out;
    out.catalog_digest = identity.content_digest;
    for (const auto &row : array(field(*input, "policies", input_name),
                                 "private_shop_household_custody/policies")) {
      fields(row,
             {"id", "version", "premises_id", "inventory_selection",
              "handling_rule_id", "handling_rule_version"},
             {}, input_name);
      if (object(row, input_name).size() != 6)
        fail("DuplicatePrivateShopCustodyPolicyField", input_name);
      PrivateShopHouseholdCustodyPolicy policy;
      policy.policy_ref = {get_string(row, "id", input_name),
                           get_int(row, "version", input_name),
                           identity.content_digest};
      policy.premises_id = get_string(row, "premises_id", input_name);
      policy.handling_rule_id = get_string(row, "handling_rule_id", input_name);
      policy.handling_rule_version =
          get_int(row, "handling_rule_version", input_name);
      if (policy.policy_ref.id.empty() || policy.policy_ref.version != 1 ||
          policy.premises_id.empty() || policy.handling_rule_id.empty() ||
          policy.handling_rule_version != 1 ||
          get_string(row, "inventory_selection", input_name) !=
              "proprietor_personal_inventory")
        unsupported("UnsupportedPrivateShopCustodyPolicy",
                    policy.policy_ref.id);
      const auto key = policy.policy_ref.id;
      if (!out.policies.insert(key, std::move(policy)))
        fail("DuplicatePrivateShopCustodyPolicy", key);
    }
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
} // namespace

// 未选择时返回零；重复、未知版本与不成对的原营业源一律拒绝。
Outcome<Integer> selected_native_private_shop_household_custody_version(
    const CatalogIdentity &identity) {
  Integer version = 0;
  for (const auto &entry : identity.module_versions) {
    if (entry.first != module)
      continue;
    if (version || entry.second != "1")
      return {false,
              {},
              common_failure("PrivateShopCustodyModuleMismatch", entry.first)
                  .error};
    version = 1;
  }
  auto shops = selected_native_private_shop_operation_versions(identity);
  if (!shops.ok)
    return {false, {}, shops.error};
  if (version && !shops.value.exact_operations())
    return {
        false,
        {},
        common_failure("PrivateShopCustodyOriginalOperationRequired").error};
  return {true, version, {}};
}

Status append_native_private_shop_household_custody_module(
    const BoundaryValue &scenario, CatalogIdentity &identity) {
  for (const auto &entry : identity.module_versions)
    if (entry.first == module)
      return common_failure("DuplicatePrivateShopCustodyModule", module);
  auto input = read_catalog_input(scenario, identity);
  if (!input.ok)
    return {false, {}, input.error};
  if (input.value)
    identity.module_versions.emplace_back(module, "1");
  auto selected =
      selected_native_private_shop_household_custody_version(identity);
  return selected.ok ? common_success() : Status{false, {}, selected.error};
}

// 冷目录重新读同一作者输入，并与已准入 module 精确成对；不从状态推断选型。
Status prepare_native_private_shop_household_custody_catalog(
    const BoundaryValue &scenario, PreparedCatalog &catalog) {
  if (catalog.private_shop_household_custody)
    return common_failure("PrivateShopCustodyCatalogAlreadyPrepared");
  auto selected =
      selected_native_private_shop_household_custody_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  auto input = read_catalog_input(scenario, catalog.identity);
  if (!input.ok)
    return {false, {}, input.error};
  if (bool(input.value) != bool(selected.value))
    return common_failure("PrivateShopCustodyAuthorSelectionMismatch");
  if (input.value)
    catalog.private_shop_household_custody =
        checkpoint_make_shared<const PrivateShopHouseholdCustodyCatalog>(
            std::move(*input.value));
  return validate_native_private_shop_household_custody_catalog(catalog);
}

// 目录只认实际铺位与原装卸规则，费用、产权及明确接受留给各自真正来源。
Status validate_native_private_shop_household_custody_catalog(
    const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 151, "PreparedCatalog", "validate_native_private_shop_household_custody_catalog", &catalog);

  auto selected =
      selected_native_private_shop_household_custody_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (bool(selected.value) != bool(catalog.private_shop_household_custody))
    return common_failure("PrivateShopCustodyCatalogSelectionMismatch");
  if (!selected.value)
    return common_success();
  auto operation = validate_native_private_shop_operation_catalog(catalog);
  if (!operation.ok)
    return operation;
  if (!catalog.travel || !catalog.household_retail_settlements ||
      !catalog.private_shops || !catalog.household_purchases ||
      (catalog.selected_household_version != 2 &&
       catalog.selected_household_version != 3))
    return common_failure("PrivateShopCustodyOriginalCatalogsRequired", {},
                          ErrorKind::unsupported_capability);
  const auto &owned = *catalog.private_shop_household_custody;
  if (owned.version != 1 ||
      owned.catalog_digest != catalog.identity.content_digest)
    return common_failure("PrivateShopCustodyCatalogIdentityMismatch");
  for (const auto &entry : owned.policies.rows()) {
    const auto &p = entry.second;
    if (entry.first != p.policy_ref.id || p.policy_ref.id.empty() ||
        p.policy_ref.version != 1 ||
        p.policy_ref.catalog_digest != owned.catalog_digest ||
        p.inventory_selection != PrivateShopCustodyInventorySelection::
                                     proprietor_personal_inventory ||
        !catalog.private_shops->premises.find(p.premises_id))
      return common_failure("PrivateShopCustodyOriginalPolicyRequired",
                            entry.first);
    const auto *rule =
        catalog.travel->cargo_handling_rules.find(p.handling_rule_id);
    if (!rule || rule->version != p.handling_rule_version ||
        p.handling_rule_version != 1)
      return common_failure("PrivateShopCustodyOriginalHandlingRuleRequired",
                            p.handling_rule_id);
    auto valid = validate_cargo_handling_rule(*rule);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}

// 根存在与显式选择双向核对，未选时连孤立事实也不能隐藏在 Common 中。
Status validate_native_private_shop_household_custody_layout(
    const WorldBundle &world) {
  if (!world.catalog)
    return common_failure("PrivateShopCustodyWorldCatalogRequired", {},
                          ErrorKind::internal);
  auto checked =
      validate_native_private_shop_household_custody_catalog(*world.catalog);
  if (!checked.ok)
    return checked;
  if (bool(world.private_shop_household_custody) !=
          bool(world.catalog->private_shop_household_custody) ||
      (world.private_shop_household_custody &&
       world.private_shop_household_custody->version != 1))
    return common_failure("PrivateShopCustodySelectedRootMismatch");
  if (!world.private_shop_household_custody)
    for (const auto &entry : world.common.state().facts.rows())
      if (entry.second.domain == "local_economy" &&
          entry.second.kind == "private_shop_household_custody_accepted")
        return common_failure("UnselectedPrivateShopCustodySource",
                              entry.first);
  return common_success();
}

// 只有外层完整 owner 已建立后才作来源冷验证；构造器绝不反取此工厂。
Status
validate_native_private_shop_household_custody_world(const WorldBundle &world) {
  auto checked = validate_native_private_shop_household_custody_layout(world);
  if (!checked.ok || !world.private_shop_household_custody)
    return checked;
  if (!world.read_stamps || !world.world_timeline)
    return common_failure("PrivateShopCustodyOriginalClockRequired", {},
                          ErrorKind::internal);
  auto now = world.actual_phase_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  // phase 先建后析构；Complete 及所有借用者必须在其之前销毁。
  std::unique_ptr<PhaseCurrentReadScope> phase;
  if (!world.has_active_read_phase()) {


    auto memory = CheckpointMemoryCharge(0);
    const StableId name{"private_shop_custody_validation"},
        provider{"local_economy"};
    auto entered = world.enter_phase_read_context(
        name, provider, {now.value, now.value}, now.value);
    if (!entered.ok)
      return {false, {}, std::move(entered.error)};
    phase = std::move(entered.value);
  }
  auto owner = world.complete_exact_time_bindings();
  if (!owner.ok)
    return {false, {}, std::move(owner.error)};
  if (!owner.value || !owner.value->reads().original_core)
    return common_failure("PrivateShopCustodyCompleteOwnerRequired", {},
                          ErrorKind::internal);
  const auto *household = owner.value->reads().household_foundation;
  const auto *source = household ? household->private_custody() : nullptr;
  if (!source || &source->state != &*world.private_shop_household_custody ||
      &source->catalog != world.catalog->private_shop_household_custody.get() ||
      &source->market.operation.shops.common != &world.common ||
      &source->travel != world.catalog->travel.get())
    return common_failure("PrivateShopCustodySameWorldSourceRequired", {},
                          ErrorKind::internal);
  auto pins = collect_native_private_shop_custody_source_pins(
      world, *owner.value->reads().original_core, *household);
  return pins.ok ? common_success() : Status{false, {}, std::move(pins.error)};
}
} // namespace hanmo::resident
