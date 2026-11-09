#include "political_budget_contract.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
bool same_unit(const BudgetUnit &a, const BudgetUnit &b) {
  if (a.index() != b.index())
    return false;
  const auto *x = std::get_if<BudgetItemQuantity>(&a);
  return !x || x->item_id == std::get<BudgetItemQuantity>(b).item_id;
}
bool same_period(const BudgetPeriod &a, const BudgetPeriod &b) {
  return a.period_from_tick == b.period_from_tick &&
         a.period_until_tick == b.period_until_tick;
}
const PublicBudgetLimit *limit_for(const BudgetAuthorizationSource &s,
                                   const BudgetUnit &u) {
  for (const auto &x : s.total_caps)
    if (same_unit(x.unit, u))
      return &x;
  return nullptr;
}
bool add(Integer &a, Integer b) {
  if (b < 0 || a > std::numeric_limits<Integer>::max() - b)
    return false;
  a += b;
  return true;
}
BudgetAuthorizationRef ref(const BudgetAuthorizationSource &s) {
  return {s.budget_id, s.budget_revision, s.approval_source};
}
Outcome<BudgetAllocationView> fail(const StableId &code, const StableId &id) {
  return {false, {}, common_failure(code, id).error};
}
} // namespace
const BudgetAuthorizationSource *
find_budget_authorization(const PoliticalBudgetState &state, const StableId &id,
                          Integer revision) {
  const BudgetAuthorizationSource *found = nullptr;
  for (const auto &s : state.authorization_history)
    if (s.budget_id == id && s.budget_revision == revision) {
      if (found)
        return nullptr;
      found = &s;
    }
  return found;
}
const BudgetAuthorizationSource *
current_budget_authorization(const PoliticalBudgetState &state,
                             const StableId &id) {
  const BudgetAuthorizationSource *found = nullptr;
  for (const auto &s : state.authorization_history)
    if (s.budget_id == id &&
        (!found || s.budget_revision > found->budget_revision))
      found = &s;
  return found;
}
Status validate_budget_authorization_shape(const BudgetAuthorizationSource &s) {
  return validate_budget_authorization_shape(s, 1);
}
Status validate_budget_authorization_shape(const BudgetAuthorizationSource &s,
                                           Integer selected_version) {
  if (selected_version != 1 && selected_version != 2)
    return common_failure("UnsupportedPoliticalBudgetsSchema",
                          std::to_string(selected_version));
  if (s.budget_id.empty() || s.budget_revision < 1 || s.policy_id.empty() ||
      s.policy_revision < 0 || s.principal_org_id.empty() ||
      s.scope.id.empty() || s.period.period_from_tick < 0 ||
      s.period.period_until_tick <= s.period.period_from_tick ||
      !valid_source_ref(s.approval_source) ||
      s.approval_source.domain != "politics" ||
      !same_source_ref(s.approval_source,
                       s.authority_basis_ref.issued_source) ||
      s.authority_basis_ref.source_schema != 1 || s.issuer_actor_id.empty() ||
      s.container_refs.empty() || s.total_caps.empty())
    return common_failure("InvalidBudgetAuthorizationSource", s.budget_id);
  if (s.purposes.empty())
    return common_failure("BudgetPurposeRequired", s.budget_id);
  std::set<Integer> purposes;
  for (const auto purpose : s.purposes) {
    const auto tag = static_cast<Integer>(purpose);
    auto valid = validate_budget_execution_purpose(purpose, selected_version);
    if (!valid.ok)
      return valid;
    if (!purposes.insert(tag).second)
      return common_failure("InvalidBudgetPurpose", s.budget_id);
  }
  for (const auto &category : s.category_caps)
    if (selected_version == 1 && !purposes.count(static_cast<Integer>(category.purpose)))
      return common_failure("BudgetCategoryNotAuthorized", s.budget_id);
  std::set<StableId> containers;
  for (const auto &id : s.container_refs)
    if (id.empty() || !containers.insert(id).second)
      return common_failure("InvalidBudgetSourceContainer", s.budget_id);
  for (std::size_t i = 0; i < s.total_caps.size(); ++i) {
    const auto &x = s.total_caps[i];
    if (x.cap < 0 || x.civil_floor < 0 || x.garrison_floor < 0)
      return common_failure("InvalidBudgetCapacity", s.budget_id);
    if (const auto *item = std::get_if<BudgetItemQuantity>(&x.unit);
        item && item->item_id.empty())
      return common_failure("InvalidBudgetUnit", s.budget_id);
    for (std::size_t j = 0; j < i; ++j)
      if (same_unit(x.unit, s.total_caps[j].unit))
        return common_failure("DuplicateBudgetUnit", s.budget_id);
  }
  for (std::size_t i = 0; i < s.category_caps.size(); ++i) {
    const auto &x = s.category_caps[i];
    auto purpose_valid=validate_budget_execution_purpose(x.purpose,selected_version);
    if(!purpose_valid.ok)return purpose_valid;
    const auto *total = limit_for(s, x.unit);
    if (!total || x.cap < 0 || x.cap > total->cap)
      return common_failure("InvalidBudgetCategoryCapacity", s.budget_id);
    for (std::size_t j = 0; j < i; ++j)
      if (x.purpose == s.category_caps[j].purpose &&
          same_unit(x.unit, s.category_caps[j].unit))
        return common_failure("DuplicateBudgetCategory", s.budget_id);
  }
  if (s.parent_budget_ref &&
      (s.parent_budget_ref->authorization_id.empty() ||
       s.parent_budget_ref->authorization_id == s.budget_id ||
       s.parent_budget_ref->authorization_revision < 1 ||
       !valid_source_ref(s.parent_budget_ref->approval_source)))
    return common_failure("InvalidBudgetParent", s.budget_id);
  return common_success();
}
Outcome<BudgetAllocationView>
read_budget_allocation(const PoliticalBudgetState &state, const StableId &id,
                       Integer revision, const BudgetUnit &unit) {
  const auto *source = find_budget_authorization(state, id, revision);
  const auto *current = current_budget_authorization(state, id);
  if (!source || source != current)
    return fail("StaleBudgetAuthorization", id);
  auto shape =
      validate_budget_authorization_shape(*source, state.schema_version);
  if (!shape.ok)
    return {false, {}, shape.error};
  const auto *cap = limit_for(*source, unit);
  if (!cap)
    return fail("BudgetUnitNotAuthorized", id);
  BudgetAllocationView out;
  out.authorized_cap = cap->cap;
  std::map<StableId, const BudgetAuthorizationSource *> latest;
  for (const auto &row : state.authorization_history) {
    auto it = latest.find(row.budget_id);
    if (it == latest.end() || it->second->budget_revision < row.budget_revision)
      latest[row.budget_id] = &row;
    else if (it->second->budget_revision == row.budget_revision)
      return fail("DuplicateBudgetRevision", row.budget_id);
  }
  for (const auto &row : latest) {
    const auto &child = *row.second;
    if (!child.parent_budget_ref ||
        child.parent_budget_ref->authorization_id != id)
      continue;
    shape = validate_budget_authorization_shape(child, state.schema_version);
    if (!shape.ok)
      return {false, {}, shape.error};
    const auto *parent = find_budget_authorization(
        state, id, child.parent_budget_ref->authorization_revision);
    if (!parent ||
        !same_source_ref(parent->approval_source,
                         child.parent_budget_ref->approval_source) ||
        !same_period(child.period, source->period))
      return fail("BudgetDelegationSourceMismatch", child.budget_id);
    if (const auto *amount = limit_for(child, unit);
        amount && !add(out.delegated_cap, amount->cap))
      return fail("BudgetDelegationOverflow", id);
  }
  if (out.delegated_cap > out.authorized_cap)
    return fail("BudgetCapacityExceeded", id);
  out.retained_cap = out.authorized_cap - out.delegated_cap;
  return {true, out, {}};
}
// /2 的每-purpose容量与总容量同样扣除真实子授权，不能让父子重复使用一份额度。
Outcome<BudgetAllocationView> read_budget_category_allocation(
    const PoliticalBudgetState &state, const StableId &id, Integer revision,
    const BudgetUnit &unit, BudgetExecutionPurpose purpose) {
  if (state.schema_version != 2)
    return fail("PoliticalBudgetsVersion2Required", id);
  auto valid = validate_budget_execution_purpose(purpose, 2);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto total = read_budget_allocation(state, id, revision, unit);
  if (!total.ok)
    return total;
  const auto *parent = find_budget_authorization(state, id, revision);
  const PublicBudgetCategoryLimit *cap = nullptr;
  for (const auto &row : parent->category_caps)
    if (row.purpose == purpose && same_unit(row.unit, unit))
      cap = &row;
  if (!cap)
    return fail("BudgetCategoryNotAuthorized", id);
  BudgetAllocationView out;
  out.authorized_cap = cap->cap;
  for (const auto &child : state.authorization_history) {
    if (current_budget_authorization(state, child.budget_id) != &child ||
        !child.parent_budget_ref ||
        child.parent_budget_ref->authorization_id != id)
      continue;
    for (const auto &row : child.category_caps)
      if (row.purpose == purpose && same_unit(row.unit, unit) &&
          !add(out.delegated_cap, row.cap))
        return fail("BudgetDelegationOverflow", id);
  }
  if (out.delegated_cap > out.authorized_cap)
    return fail("BudgetCategoryCapacityExceeded",
                budget_execution_purpose_key(purpose));
  out.retained_cap = out.authorized_cap - out.delegated_cap;
  return {true, out, {}};
}
Outcome<BudgetDelegationView>
read_budget_delegation(const PoliticalBudgetState &state,
                       const BudgetUsageRead &usage,
                       const WorldIdentity &identity, const StableId &id,
                       Integer revision, const BudgetUnit &unit) {
  auto allocation = read_budget_allocation(state, id, revision, unit);
  if (!allocation.ok)
    return {false, {}, allocation.error};
  const auto &parent = *find_budget_authorization(state, id, revision);
  BudgetDelegationView out;
  out.identity = identity;
  out.parent_budget_ref = ref(parent);
  out.unit = unit;
  out.period_from_tick = parent.period.period_from_tick;
  out.period_until_tick = parent.period.period_until_tick;
  out.authorized_cap = allocation.value.authorized_cap;
  out.delegated_cap = allocation.value.delegated_cap;
  out.retained_cap = allocation.value.retained_cap;
  for (const auto &child : state.authorization_history) {
    if (!child.parent_budget_ref ||
        child.parent_budget_ref->authorization_id != id ||
        current_budget_authorization(state, child.budget_id) != &child)
      continue;
    const auto *cap = limit_for(child, unit);
    if (!cap)
      continue;
    auto real = usage.read_budget_usage({child.budget_id, child.budget_revision,
                                         child.period, unit, identity, 0, 64});
    if (!real.ok)
      return {false, {}, real.error};
    auto own = read_budget_allocation(state, child.budget_id,
                                      child.budget_revision, unit);
    if (!own.ok)
      return {false, {}, own.error};
    Integer occupied = real.value.spent_gross;
    if (!add(occupied, real.value.committed_unpaid) ||
        !add(occupied, own.value.delegated_cap))
      return {
          false,
          {},
          common_failure("BudgetDelegationOverflow", child.budget_id).error};
    out.children.push_back(
        {ref(child), cap->cap, occupied >= cap->cap ? 0 : cap->cap - occupied});
  }
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
