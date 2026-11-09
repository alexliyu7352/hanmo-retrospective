#include "household_market_resolver.hpp"
#include "household_facility_sources.hpp"
#include "named_household_service.hpp"
#include "cohort_household_service.hpp"
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> fail(const char *code, const StableId &id = {}) {
  return {false, {}, common_failure(code, id).error};
}
bool same_time(const WorldMoment &a, const WorldMoment &b) {
  auto c = compare_world_moments(a, b);
  return c.ok && c.value == 0;
}
} // namespace
Outcome<FacilityHolderKind>
HouseholdFacilitySourceRead::read_facility_holder_kind(
    const ReservationRef &holder) const {
  if (holder.kind != ReservationRef::Kind::domain_activity ||
      holder.domain_id != "local_economy" ||
      holder.activity_kind != "household_purchase" ||
      (!x_.state.purchases.find(holder.id) && !x_.state.plans.find(holder.id)))
    return fail<FacilityHolderKind>("UnknownHouseholdFacilityHolder",
                                    holder.id);
  return {true, {holder.kind, "local_economy", "household_purchase"}, {}};
}
Status HouseholdFacilitySourceRead::validate_facility_holder_selection(
    const FacilityHolderProfileSelection &s) const {
  if (s.holder.kind != ReservationRef::Kind::domain_activity ||
      s.holder.owner_domain != "local_economy" ||
      s.holder.activity_kind != "household_purchase" ||
      s.profile_id != "household_purchase_facility" || s.version != 2)
    return common_failure("UnsupportedHouseholdFacilityProfile", s.profile_id);
  return common_success();
}
Outcome<std::vector<FacilityClaimDependency>>
HouseholdFacilitySourceRead::enumerate_holder_claim_dependencies(
    const FacilitySlotRef &slot) const {
  std::vector<FacilityClaimDependency> out;
  for (const auto &row : x_.state.purchases.rows()) {
    const auto &p = row.second;
    const auto *a = x_.state.accepted_terms.find(p.accepted_terms_ref.id);
    if (!a)
      return fail<std::vector<FacilityClaimDependency>>(
          "HouseholdFacilityAcceptanceRequired", p.purchase_id);
    auto resolved = read_household_market_service_accepted(
        x_.common, x_.catalog, x_.state, x_.private_market_custody, *a);
    if (!resolved.ok)
      return {false, {}, resolved.error};
    const auto *m = resolved.value;
    if (!a || !m)
      return fail<std::vector<FacilityClaimDependency>>(
          "HouseholdFacilityAcceptanceMissing", p.purchase_id);
    if (m->service_slot == slot) {
      if (std::holds_alternative<NamedHouseholdBuyer>(p.executor)) {
        for (const auto &ep : x_.state.named_service_claim_episodes)
          if (ep.purchase_id == p.purchase_id) {
            out.push_back({ep.claim_id, ep.admission_receipt_ref});
            if (ep.close_receipt_ref)
              out.push_back({ep.claim_id, ep.close_receipt_ref});
          }
        continue;
      }
      auto attempts = read_cohort_household_service_attempts(x_, p.purchase_id);
      if (!attempts.ok)
        return {false, {}, attempts.error};
      for (const auto &attempt : attempts.value) {
        out.push_back({attempt.claim_episode.original_claim.claim_id,
                       attempt.claim_episode.admission_receipt_ref});
        if (attempt.claim_episode.close_receipt_ref)
          out.push_back({attempt.claim_episode.original_claim.claim_id,
                         attempt.claim_episode.close_receipt_ref});
      }
    }
  }
  return {true, std::move(out), {}};
}
Status HouseholdFacilitySourceRead::validate_holder_claim_admission(
    const FacilityClaim2 &claim,
    const FacilityClaimMutationReceipt &receipt) const {
  return validate_household_purchase_claim_source(x_, claim, receipt, true);
}
Status HouseholdFacilitySourceRead::validate_holder_claim_close(
    const FacilityClaim2 &claim, FacilityClaimCloseReason reason,
    const FacilityClaimMutationReceipt &receipt) const {
  const auto *p = x_.state.purchases.find(claim.holder.id);
  if (p && std::holds_alternative<NamedHouseholdBuyer>(p->executor)) {
    bool found = false;
    for (const auto &close : receipt.closed_claims)
      if (close.claim_id == claim.claim_id && close.reason == reason)
        found = true;
    if (!found)
      return common_failure("NamedHouseholdCloseReasonMismatch",
                            claim.claim_id);
    return validate_named_household_service_claim(x_, claim, receipt, false);
  }
  if (reason != FacilityClaimCloseReason::completed &&
      reason != FacilityClaimCloseReason::suspended)
    return common_failure("HouseholdServiceInterruptionSourceRequired",
                          claim.claim_id);
  return validate_cohort_household_service_claim(x_, claim, receipt, false);
}
Status HouseholdFacilitySourceRead::validate_holder_mutation_source(
    const FacilitySlotRef &slot,
    const FacilityClaimMutationReceipt &receipt) const {
  if (receipt.source_schema == 2) {
    auto event =
        read_named_household_service_event(x_, receipt.source_result_ref);
    if (!event.ok)
      return {false, {}, event.error};
    const NamedServiceClaimEpisode2 *ep = nullptr;
    for (const auto &candidate : x_.state.named_service_claim_episodes)
      if (candidate.purchase_id == event.value->purchase_id &&
          candidate.ordinal == event.value->episode_ordinal) {
        if (ep)
          return common_failure("DuplicateNamedHouseholdEpisode",
                                candidate.claim_id);
        ep = &candidate;
      }
    if (!ep || ep->admission_receipt_ref.slot_ref != slot ||
        !same_time(event.value->occurred_at, receipt.occurred_at))
      return common_failure("NamedHouseholdMutationSourceMismatch",
                            receipt.source_result_ref.id);
    if (event.value->kind == HouseholdServiceEventKind::accepted ||
        event.value->kind == HouseholdServiceEventKind::resumed) {
      if (receipt.admitted_claims.size() != 1)
        return common_failure("NamedHouseholdAdmissionReceiptMismatch");
      return validate_holder_claim_admission(receipt.admitted_claims.front(),
                                             receipt);
    }
    auto original =
        read_facility_claim_episode(x_.claims, ep->claim_id, x_.now);
    if (!original.ok)
      return {false, {}, original.error};
    if (receipt.closed_claims.size() != 1)
      return common_failure("NamedHouseholdCloseReceiptMismatch");
    return validate_holder_claim_close(original.value.original_claim,
                                       receipt.closed_claims.front().reason,
                                       receipt);
  }
  if (receipt.source_schema != 1 ||
      receipt.source_result_ref.domain != "local_economy")
    return common_failure("HouseholdFacilityMutationSourceMismatch",
                          receipt.source_result_ref.id);
  if (receipt.admitted_claims.size() == 1 && receipt.closed_claims.empty()) {
    if (receipt.admitted_claims.front().slot_ref != slot)
      return common_failure("HouseholdFacilityAdmissionSlotMismatch");
    return validate_holder_claim_admission(receipt.admitted_claims.front(),
                                           receipt);
  }
  if (receipt.admitted_claims.empty() && receipt.closed_claims.size() == 1) {
    auto original = read_facility_claim_episode(
        x_.claims, receipt.closed_claims.front().claim_id, x_.now);
    if (!original.ok)
      return {false, {}, original.error};
    if (original.value.original_claim.slot_ref != slot)
      return common_failure("HouseholdFacilityCloseSlotMismatch");
    return validate_holder_claim_close(original.value.original_claim,
                                       receipt.closed_claims.front().reason,
                                       receipt);
  }
  return common_failure("UnsupportedHouseholdFacilityMutation",
                        receipt.source_result_ref.id);
}
Outcome<bool> HouseholdFacilitySourceRead::has_actual_facility_service_started(
    const FacilityClaim2 &claim, const WorldMoment &at) const {
  const auto *purchase = x_.state.purchases.find(claim.holder.id);
  bool owned = purchase && purchase->service_claim_ref == claim.claim_id;
  if (purchase &&
      std::holds_alternative<CohortHouseholdBuyer>(purchase->executor)) {
    auto attempts =
        read_cohort_household_service_attempts(x_, purchase->purchase_id);
    if (!attempts.ok)
      return {false, {}, attempts.error};
    for (const auto &attempt : attempts.value)
      owned |= attempt.claim_episode.original_claim.claim_id == claim.claim_id;
  }
  if (purchase &&
      std::holds_alternative<NamedHouseholdBuyer>(purchase->executor))
    for (const auto &ep : x_.state.named_service_claim_episodes)
      if (ep.purchase_id == purchase->purchase_id &&
          ep.claim_id == claim.claim_id)
        owned = true;
  if (!owned)
    return fail<bool>("HouseholdFacilityHolderMissing", claim.claim_id);
  for (const auto &segment : x_.state.service_segments)
    if (segment.purchase_id == purchase->purchase_id) {
      auto starts = compare_world_moments(claim.from, segment.window.from);
      auto ends = compare_world_moments(segment.window.to, claim.until);
      if (!starts.ok)
        return {false, {}, starts.error};
      if (!ends.ok)
        return {false, {}, ends.error};
      if (starts.value > 0 || ends.value > 0)
        continue;
      auto positive =
          compare_world_moments(segment.window.from, segment.window.to);
      auto began = compare_world_moments(segment.window.from, at);
      if (!positive.ok)
        return {false, {}, positive.error};
      if (!began.ok)
        return {false, {}, began.error};
      if (positive.value < 0 && began.value < 0) {
        auto actual = x_.environment.validate_purchase_service_segment(
            *purchase, segment);
        if (!actual.ok)
          return {false, {}, actual.error};
        return {true, true, {}};
      }
    }
  return {true, false, {}};
}

} // namespace hanmo::resident
