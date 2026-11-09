#include "resident/kernel/catalog_section_observation.hpp"
#include "political_budget_policy.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
bool same_unit(const BudgetUnit &a, const BudgetUnit &b) {
  if (a.index() != b.index())
    return false;
  const auto *item = std::get_if<BudgetItemQuantity>(&a);
  return !item || item->item_id == std::get<BudgetItemQuantity>(b).item_id;
}
bool valid_unit(const BudgetUnit &u) {
  const auto *item = std::get_if<BudgetItemQuantity>(&u);
  return std::holds_alternative<BudgetMoneyQian>(u) ||
         (item && !item->item_id.empty());
}
} // namespace
const char *public_budget_policy_purpose_name(PublicBudgetPolicyPurpose p) {
  switch (p) {
  case PublicBudgetPolicyPurpose::aid:
    return "aid";
  case PublicBudgetPolicyPurpose::work:
    return "work";
  }
  return "unknown";
}
Status validate_public_budget_policy_shape(const PublicBudgetPolicy &p) {
  auto fail = [&](const char *code) {
    return common_failure(code, p.policy_id);
  };
  if (p.policy_id.empty() || p.revision < 1 || p.scope.id.empty())
    return fail("InvalidPublicBudgetPolicyIdentity");
  if (p.scope.kind != PublicBudgetPolicyScope::Kind::organization &&
      p.scope.kind != PublicBudgetPolicyScope::Kind::jurisdiction)
    return fail("InvalidPublicBudgetPolicyScope");
  if (p.period.from.half_day_tick < 0 || p.period.to.half_day_tick < 0)
    return fail("InvalidPublicBudgetPolicyPeriod");
  auto encoded_from = encode_world_moment(p.period.from);
  if (!encoded_from.ok)
    return {false, {}, encoded_from.error};
  auto encoded_until = encode_world_moment(p.period.to);
  if (!encoded_until.ok)
    return {false, {}, encoded_until.error};
  auto order = compare_world_moments(p.period.from, p.period.to);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value >= 0)
    return fail("InvalidPublicBudgetPolicyPeriod");
  std::set<PublicBudgetPolicyPurpose> purposes;
  for (const auto purpose : p.purposes) {
    if (purpose != PublicBudgetPolicyPurpose::aid &&
        purpose != PublicBudgetPolicyPurpose::work)
      return fail("UnsupportedPublicBudgetPolicyPurpose");
    if (!purposes.insert(purpose).second)
      return fail("DuplicatePublicBudgetPolicyPurpose");
  }
  if (purposes.empty() || p.containers.empty() || p.total_caps.empty())
    return fail("IncompletePublicBudgetPolicy");
  std::set<std::pair<ResourceKey::Kind, StableId>> containers;
  for (const auto &r : p.containers) {
    if ((r.kind != ResourceKey::Kind::wallet &&
         r.kind != ResourceKey::Kind::inventory) ||
        r.id.empty())
      return fail("InvalidPublicBudgetPolicyContainer");
    if (!containers.emplace(r.kind, r.id).second)
      return fail("DuplicatePublicBudgetPolicyContainer");
  }
  for (std::size_t i = 0; i < p.total_caps.size(); ++i) {
    const auto &limit = p.total_caps[i];
    if (!valid_unit(limit.unit) || limit.cap < 0)
      return fail("InvalidPublicBudgetPolicyCap");
    for (std::size_t j = 0; j < i; ++j)
      if (same_unit(limit.unit, p.total_caps[j].unit))
        return fail("DuplicatePublicBudgetPolicyUnit");
    const auto kind = std::holds_alternative<BudgetMoneyQian>(limit.unit)
                          ? ResourceKey::Kind::wallet
                          : ResourceKey::Kind::inventory;
    if (std::none_of(p.containers.begin(), p.containers.end(),
                     [&](const auto &r) { return r.kind == kind; }))
      return fail("PublicBudgetPolicyUnitContainerMissing");
  }
  for (std::size_t i = 0; i < p.purpose_caps.size(); ++i) {
    const auto &limit = p.purpose_caps[i];
    if (!purposes.contains(limit.purpose) || !valid_unit(limit.unit) ||
        limit.cap < 0)
      return fail("InvalidPublicBudgetPolicyPurposeCap");
    const auto total = std::find_if(
        p.total_caps.begin(), p.total_caps.end(),
        [&](const auto &r) { return same_unit(r.unit, limit.unit); });
    if (total == p.total_caps.end() || limit.cap > total->cap)
      return fail("PublicBudgetPolicyPurposeExceedsTotal");
    for (std::size_t j = 0; j < i; ++j)
      if (limit.purpose == p.purpose_caps[j].purpose &&
          same_unit(limit.unit, p.purpose_caps[j].unit))
        return fail("DuplicatePublicBudgetPolicyPurposeCap");
  }
  // 每个作者列出的用途、量纲都有自己的明确上限，不拿总上限填缺值。
  for (const auto purpose : p.purposes)
    for (const auto &total : p.total_caps)
      if (std::none_of(
              p.purpose_caps.begin(), p.purpose_caps.end(), [&](const auto &r) {
                return r.purpose == purpose && same_unit(r.unit, total.unit);
              }))
        return fail("PublicBudgetPolicyPurposeCapMissing");
  return common_success();
}
Status validate_public_budget_policy_catalog_shape(
    const PublicBudgetPolicyCatalog &c) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 87, "PublicBudgetPolicyCatalog", "validate_public_budget_policy_catalog_shape", &c);

  std::set<std::pair<StableId, Integer>> identities;
  for (const auto &p : c.policies) {
    auto valid = validate_public_budget_policy_shape(p);
    if (!valid.ok)
      return valid;
    if (!identities.emplace(p.policy_id, p.revision).second)
      return common_failure("DuplicatePublicBudgetPolicyRevision", p.policy_id);
  }
  return common_success();
}
const PublicBudgetPolicy *
find_public_budget_policy(const PublicBudgetPolicyCatalog &c,
                          const StableId &id, Integer revision) {
  const PublicBudgetPolicy *found = nullptr;
  for (const auto &p : c.policies)
    if (p.policy_id == id && p.revision == revision) {
      if (found)
        return nullptr;
      found = &p;
    }
  return found;
}
} // namespace hanmo::resident
