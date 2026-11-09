// 逐型复制计量已删除；保留真实 StableId 长度门与 FSL 深复制/可抛交换的拥有关系。
#include "world_copy_plan.hpp"
#include "hanmo/resident/model_contract.hpp"
#include "resident/domains/growth/growth_fixed_service_model.hpp"
#include <utility>
namespace hanmo::resident {
std::size_t checkpoint_produced_stable_id_size(const ProducedStableIdBuildView &value) {
  if (value.prefix.size() == SIZE_MAX ||
      value.decimal.size() > SIZE_MAX - value.prefix.size() - 1)
    throw std::bad_alloc();
  return value.prefix.size() + 1 + value.decimal.size();
}

// 真实租约先于成员复制；右值仍可能因 CanonicalFraction 复制而抛出。
LifeFixedServiceInputs::LifeFixedServiceInputs(
    const LifeFixedServiceInputs &other)
    : copy_memory(CheckpointCopyLease::reserve()),
      actors(other.actors), locked_rule_refs(other.locked_rule_refs),
      accepted_permission_refs(other.accepted_permission_refs),
      facility_use_refs(other.facility_use_refs),
      resource_basis_refs(other.resource_basis_refs), specific(other.specific),
      field_order(other.field_order) {
}
LifeFixedServiceInputs::LifeFixedServiceInputs(LifeFixedServiceInputs &&other)
    : copy_memory(other.copy_memory), actors(std::move(other.actors)),
      locked_rule_refs(std::move(other.locked_rule_refs)),
      accepted_permission_refs(std::move(other.accepted_permission_refs)),
      facility_use_refs(std::move(other.facility_use_refs)),
      resource_basis_refs(std::move(other.resource_basis_refs)),
      specific(std::move(other.specific)),
      field_order(std::move(other.field_order)) {
  other.copy_memory = {};
}
LifeFixedServiceInputs &
LifeFixedServiceInputs::operator=(const LifeFixedServiceInputs &other) {
  if (this != &other) {
    LifeFixedServiceInputs fresh(other);
    swap(fresh);
  }
  return *this;
}
LifeFixedServiceInputs &
LifeFixedServiceInputs::operator=(LifeFixedServiceInputs &&other) {
  if (this != &other) {
    LifeFixedServiceInputs fresh(std::move(other));
    swap(fresh);
  }
  return *this;
}
// 交换失败时两端仍持同一组原租约；候选回滚销毁前不释放仍活的字段拥有关系。
void LifeFixedServiceInputs::swap(LifeFixedServiceInputs &other) {
  if (this == &other)
    return;
  auto left = copy_memory, right = other.copy_memory;
  auto both = CheckpointCopyLease::hold_both(left, right);
  copy_memory = both;
  other.copy_memory = both;
  using std::swap;
  swap(actors, other.actors);
  swap(locked_rule_refs, other.locked_rule_refs);
  swap(accepted_permission_refs, other.accepted_permission_refs);
  swap(facility_use_refs, other.facility_use_refs);
  swap(resource_basis_refs, other.resource_basis_refs);
  swap(specific, other.specific);
  swap(field_order, other.field_order);
  copy_memory = std::move(right);
  other.copy_memory = std::move(left);
}
// 真实租约先于成员复制；右值仍可能因 CanonicalFraction 复制而抛出。
LifeFixedServiceBoundary::LifeFixedServiceBoundary(
    const LifeFixedServiceBoundary &other)
    : copy_memory(CheckpointCopyLease::reserve()),
      boundary_id(other.boundary_id), source_schema(other.source_schema),
      action_ref(other.action_ref), action_kind(other.action_kind),
      owner_ref(other.owner_ref), boundary_ordinal(other.boundary_ordinal),
      previous_boundary_ref(other.previous_boundary_ref), kind(other.kind),
      occurred_at(other.occurred_at), cause(other.cause),
      input_snapshot(other.input_snapshot),
      original_outcome_ref(other.original_outcome_ref),
      field_order(other.field_order) {
}
LifeFixedServiceBoundary::LifeFixedServiceBoundary(
    LifeFixedServiceBoundary &&other)
    : copy_memory(other.copy_memory), boundary_id(std::move(other.boundary_id)),
      source_schema(std::move(other.source_schema)),
      action_ref(std::move(other.action_ref)),
      action_kind(std::move(other.action_kind)),
      owner_ref(std::move(other.owner_ref)),
      boundary_ordinal(std::move(other.boundary_ordinal)),
      previous_boundary_ref(std::move(other.previous_boundary_ref)),
      kind(std::move(other.kind)), occurred_at(std::move(other.occurred_at)),
      cause(std::move(other.cause)),
      input_snapshot(std::move(other.input_snapshot)),
      original_outcome_ref(std::move(other.original_outcome_ref)),
      field_order(std::move(other.field_order)) {
  other.copy_memory = {};
}
LifeFixedServiceBoundary &
LifeFixedServiceBoundary::operator=(const LifeFixedServiceBoundary &other) {
  if (this != &other) {
    LifeFixedServiceBoundary fresh(other);
    swap(fresh);
  }
  return *this;
}
LifeFixedServiceBoundary &
LifeFixedServiceBoundary::operator=(LifeFixedServiceBoundary &&other) {
  if (this != &other) {
    LifeFixedServiceBoundary fresh(std::move(other));
    swap(fresh);
  }
  return *this;
}
// 交换失败时两端仍持同一组原租约；候选回滚销毁前不释放仍活的字段拥有关系。
void LifeFixedServiceBoundary::swap(LifeFixedServiceBoundary &other) {
  if (this == &other)
    return;
  auto left = copy_memory, right = other.copy_memory;
  auto both = CheckpointCopyLease::hold_both(left, right);
  copy_memory = both;
  other.copy_memory = both;
  using std::swap;
  swap(boundary_id, other.boundary_id);
  swap(source_schema, other.source_schema);
  swap(action_ref, other.action_ref);
  swap(action_kind, other.action_kind);
  swap(owner_ref, other.owner_ref);
  swap(boundary_ordinal, other.boundary_ordinal);
  swap(previous_boundary_ref, other.previous_boundary_ref);
  swap(kind, other.kind);
  swap(occurred_at, other.occurred_at);
  swap(cause, other.cause);
  swap(input_snapshot, other.input_snapshot);
  swap(original_outcome_ref, other.original_outcome_ref);
  swap(field_order, other.field_order);
  copy_memory = std::move(right);
  other.copy_memory = std::move(left);
}
} // namespace hanmo::resident
