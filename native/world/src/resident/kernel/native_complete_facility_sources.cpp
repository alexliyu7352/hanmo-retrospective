#include "native_complete_facility_sources.hpp"
#include "native_economic_facility_roots.hpp"
#include "resident/domains/local_economy/economic_facility_provider.hpp"
#include "resident/domains/local_economy/household_service_lease.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
bool same_slot(const FacilitySlotRef &a, const FacilitySlotRef &b) {
  return a.provider_domain == b.provider_domain &&
         a.facility_id == b.facility_id && a.slot_kind == b.slot_kind;
}
bool same_source(const SourceRef &a, const SourceRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id;
}
} // namespace
NativeCompleteFacilitySources::NativeCompleteFacilitySources(
    const WorldBundle &w, const NativeHouseholdServiceLease *h,
    const NativeEconomicFacilitySourceRead *e)
    : world_(w), household_(h), economic_(e) {}
Status NativeCompleteFacilitySources::validate_bindings() const {
  auto selected = validate_native_economic_facility_layout(world_);
  if (!selected.ok)
    return selected;
  if (!household_ && !economic_)
    return common_failure("CompleteFacilityOriginalOwnerRequired");
  if (bool(world_.catalog->economic_facilities) != bool(economic_) ||
      bool(world_.catalog->household_purchases) != bool(household_) ||
      bool(world_.household_purchases) != bool(household_) ||
      bool(world_.catalog->household_retail_settlements) != bool(household_) ||
      bool(world_.household_retail_settlements) != bool(household_) ||
      (household_ ? (world_.catalog->selected_household_version != 2 &&
                     world_.catalog->selected_household_version != 3)
                  : world_.catalog->selected_household_version != 0))
    return common_failure("CompleteFacilitySelectedOwnerMismatch");
  if (household_) {
    auto bound = household_->validate_bindings();
    if (!bound.ok)
      return bound;
    const auto &h = household_->household();
    const auto &m = household_->market();
    if (!world_.household_purchases || &h.common != &world_.common ||
        &h.state != &*world_.household_purchases ||
        &h.catalog != world_.catalog->household_purchases.get() ||
        &m.common != &world_.common ||
        &m.purchases != &*world_.household_purchases ||
        !world_.facility_claims || &m.claims != &*world_.facility_claims ||
        &household_->retail().state != &*world_.household_retail_settlements ||
        &household_->retail().catalog !=
            world_.catalog->household_retail_settlements.get())
      return common_failure("CompleteMarketFacilityWorldMismatch");
    auto now = world_.actual_phase_moment();
    if (!now.ok)
      return {false, {}, std::move(now.error)};
    auto equal = compare_world_moments(m.now, now.value);
    if (!equal.ok)
      return {false, {}, std::move(equal.error)};
    if (equal.value)
      return common_failure("CompleteMarketFacilityMomentMismatch");
  }
  return common_success();
}
Outcome<const FacilityClaimSourceRead *>
NativeCompleteFacilitySources::owner(const FacilitySlotRef &s) const {
  if (s.slot_kind == "market_service")
    return household_
               ? Outcome<const FacilityClaimSourceRead *>{true,
                                                          &household_
                                                               ->capacity(),
                                                          {}}
               : Outcome<const FacilityClaimSourceRead *>{
                     false, nullptr,
                     common_failure("CompleteMarketFacilityOwnerRequired",
                                    s.facility_id)
                         .error};
  const bool physical =
      s.slot_kind == "bed_space" || s.slot_kind == "furnace_slot" ||
      s.slot_kind == "workbench_slot" || s.slot_kind == "teaching_seat";
  if (!physical || !economic_)
    return {
        false, nullptr,
        common_failure("CompleteOriginalFacilityOwnerRequired", s.facility_id)
            .error};
  return {true, economic_, {}};
}
Outcome<WorldMoment> NativeCompleteFacilitySources::initial_source_at(
    const FacilitySlotRef &s) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->initial_source_at(s);
}
Outcome<std::vector<VerifiedFacilityCapacitySegment>>
NativeCompleteFacilitySources::read_verified_capacity(
    const FacilitySourceRef &r, const FacilitySlotRef &s,
    const MomentSpan &w) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->read_verified_capacity(r, s, w);
}
Outcome<FacilityCapacityChange>
NativeCompleteFacilitySources::read_capacity_history_base(
    const FacilitySlotRef &s, const FacilityHistoryBase &b) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->read_capacity_history_base(s, b);
}
Outcome<FacilitySourceRef>
NativeCompleteFacilitySources::read_capacity_reconciliation_source(
    const FacilitySlotRef &s, const FacilityCapacityChange &c) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->read_capacity_reconciliation_source(s, c);
}
Outcome<FacilityMutationCapacityOrder>
NativeCompleteFacilitySources::compare_mutation_to_capacity_change(
    const FacilitySlotRef &s, const FacilityClaimMutationReceipt &r,
    const FacilityCapacityChange &c) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->compare_mutation_to_capacity_change(s, r, c);
}
Outcome<FacilitySourceRef>
NativeCompleteFacilitySources::capacity_source_at(const FacilitySlotRef &s,
                                                  const WorldMoment &at) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->capacity_source_at(s, at);
}
Outcome<std::vector<FacilityCapacityChange>>
NativeCompleteFacilitySources::enumerate_capacity_changes(
    const FacilitySlotRef &s, const WorldMoment &a,
    const WorldMoment &b) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->enumerate_capacity_changes(s, a, b);
}
Outcome<std::vector<FacilityClaimDependency>>
NativeCompleteFacilitySources::enumerate_claim_dependencies(
    const FacilitySlotRef &s) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->enumerate_claim_dependencies(s);
}
Status NativeCompleteFacilitySources::validate_claim_admission(
    const FacilityClaim2 &c, const FacilityClaimMutationReceipt &r) const {
  auto p = owner(c.slot_ref);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->validate_claim_admission(c, r);
}
Status NativeCompleteFacilitySources::validate_claim_close(
    const FacilityClaim2 &c, FacilityClaimCloseReason why,
    const FacilityClaimMutationReceipt &r) const {
  auto p = owner(c.slot_ref);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->validate_claim_close(c, why, r);
}
Status NativeCompleteFacilitySources::validate_mutation_source(
    const FacilitySlotRef &s, const FacilityClaimMutationReceipt &r) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->validate_mutation_source(s, r);
}
Status NativeCompleteFacilitySources::validate_pruned_prefix(
    const FacilitySlotRef &s, const VacantPrunedFacilityPrefix &p) const {
  auto original = owner(s);
  if (!original.ok)
    return {false, {}, std::move(original.error)};
  return original.value->validate_pruned_prefix(s, p);
}
Status NativeCompleteFacilitySources::validate_retention_and_physical_occupancy(
    const FacilitySlotRef &s, const MomentSpan &w,
    const std::vector<const FacilityClaim2 *> &c,
    const FacilityCapacitySource2 &r) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->validate_retention_and_physical_occupancy(s, w, c, r);
}
Outcome<Integer> NativeCompleteFacilitySources::read_additional_normal_capacity(
    const FacilitySlotRef &s, const MomentSpan &w,
    const std::vector<const FacilityClaim2 *> &c,
    const FacilityCapacitySource2 &r) const {
  auto p = owner(s);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->read_additional_normal_capacity(s, w, c, r);
}
Outcome<bool> NativeCompleteFacilitySources::is_verified_retained_claim(
    const FacilityClaim2 &c, const MomentSpan &w,
    const FacilityCapacitySource2 &r) const {
  auto p = owner(c.slot_ref);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->is_verified_retained_claim(c, w, r);
}
Outcome<bool> NativeCompleteFacilitySources::conflicting_source_visible(
    const ReservationRef &r, const FacilityClaim2 &c) const {
  auto p = owner(c.slot_ref);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  auto initial = p.value->initial_source_at(c.slot_ref);
  if (!initial.ok)
    return {false, {}, std::move(initial.error)};
  return p.value->conflicting_source_visible(r, c);
}
// 此原接口不带slot，只能从原capacity行定位并调用真实owner全验，不能按空claims猜。
Outcome<bool> NativeCompleteFacilitySources::source_change_affects_claims(
    const FacilityCapacityChange &change,
    const std::vector<const FacilityClaim2 *> &claims) const {
  const FacilitySlotRef *slot = nullptr;
  const WorldMoment *at = nullptr;
  Integer revision = 0;
  const std::optional<SourceRef> *previous = nullptr;
  const auto &ref = change.source;
  if (ref.source_schema != 1)
    return {false,
            {},
            common_failure("CompleteFacilitySourceSchemaMismatch",
                           ref.source_ref.id)
                .error};
  auto bind = [&](const auto &row) {
    if (row.source_id != ref.source_ref.id ||
        row.slot_ref.provider_domain != ref.source_ref.domain)
      return true;
    if (slot)
      return false;
    slot = &row.slot_ref;
    at = &row.occurred_at;
    revision = row.source_revision;
    previous = &row.previous_source_ref;
    return true;
  };
  if (ref.source_ref.kind == "market_service_capacity" && household_) {
    const auto &rows = household_->household().state.market_service_sources;
    for (const auto *values : {&rows.public_sources, &rows.private_sources})
      for (const auto &row : *values) {
        if (!bind(row))
          return {
              false,
              {},
              common_failure("CompleteFacilitySourceAliased", ref.source_ref.id)
                  .error};
        if (slot == &row.slot_ref) {
          auto valid = household_->capacity().validate_market_source(row);
          if (!valid.ok)
            return {false, {}, std::move(valid.error)};
        }
      }
  } else if (ref.source_ref.kind == "economic_facility_capacity" && economic_ &&
             world_.economic_facility_capacity_sources) {
    for (const auto &row :
         world_.economic_facility_capacity_sources->capacity_sources) {
      if (!bind(row))
        return {
            false,
            {},
            common_failure("CompleteFacilitySourceAliased", ref.source_ref.id)
                .error};
      if (slot == &row.slot_ref) {
        auto valid = economic_->validate_source(row);
        if (!valid.ok)
          return {false, {}, std::move(valid.error)};
      }
    }
  }
  if (!slot || revision != change.revision ||
      previous->has_value() != change.previous_source.has_value() ||
      (previous->has_value() &&
       (!same_source(**previous, change.previous_source->source_ref) ||
        change.previous_source->source_schema != 1)))
    return {false,
            {},
            common_failure("CompleteFacilityOriginalChangeRequired",
                           ref.source_ref.id)
                .error};
  auto equal = compare_world_moments(*at, change.occurred_at);
  if (!equal.ok)
    return {false, {}, std::move(equal.error)};
  if (equal.value)
    return {false,
            {},
            common_failure("CompleteFacilityChangeMomentMismatch",
                           ref.source_ref.id)
                .error};
  for (const auto *claim : claims)
    if (!claim || !same_slot(claim->slot_ref, *slot))
      return {false,
              {},
              common_failure("CompleteFacilityChangeClaimMismatch",
                             ref.source_ref.id)
                  .error};
  auto p = owner(*slot);
  if (!p.ok)
    return {false, {}, std::move(p.error)};
  return p.value->source_change_affects_claims(change, claims);
}
} // namespace hanmo::resident
