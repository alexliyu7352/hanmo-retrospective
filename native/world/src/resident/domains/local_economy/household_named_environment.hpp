// Original NamedBuyer inputs. Condition and position remain in their owning
// domains; only their actual adopted references enter the existing Fixed
// transition.
#pragma once
#include "household_purchase_contract.hpp"
#include "resident/domains/actor_foundation/character_expression.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/travel/actor_spatial_history.hpp"
#include "resident/domains/travel/household_transport_presence.hpp"
namespace hanmo::resident {
struct HouseholdNamedEnvironmentContext {
  const CommonStore &common;
  const CommonCatalog &shared;
  const HouseholdPurchaseCatalog &catalog;
  const HouseholdPurchaseState &state;
  const ActionElapsedStore &elapsed;
  const CharacterConditionSources &body;
  const FacilityClaimsState &claims;
  const FacilityClaimSourceRead &facilities;
  const SpatialCarrierState &carriers;
  const MovementExactState &movement;
  const TransportRegistrationState &registrations;
  const SpatialCarrierSources &spatial;
  const CurrentMomentRead &clock;
  const MilitarySpatialHandoffState *military_handoffs = nullptr;
  const PrivateShopHouseholdCustodyContext *private_market_custody = nullptr;
};
// Called only by the real acceptance/resume writer after its original event has
// been appended. The returned value is stored in FixedServiceTransition inputs.
Outcome<std::vector<FixedServiceInputRef>>
capture_named_household_adoption_inputs(
    const HouseholdNamedEnvironmentContext &, const HouseholdPurchase &,
    const HouseholdServiceEvent2 &);
Status validate_named_household_adoption_inputs(
    const HouseholdNamedEnvironmentContext &, const HouseholdPurchase &,
    const HouseholdServiceEvent2 &, const std::vector<FixedServiceInputRef> &);
Status validate_named_household_current_participation(
    const HouseholdNamedEnvironmentContext &, const NamedHouseholdBuyer &,
    const HouseholdPurchasePlan &, const WorldMoment &,
    const StableId &already_owned_action = {});
Outcome<std::vector<SourceRef>> read_named_household_actual_participation(
    const HouseholdNamedEnvironmentContext &, const HouseholdPurchase &,
    const MomentSpan &);
Outcome<HouseholdPurchaseCoverage>
read_named_household_actual_coverage(const HouseholdNamedEnvironmentContext &,
                                     const HouseholdPurchase &,
                                     const MomentSpan &);
Status validate_named_household_actual_segment(
    const HouseholdNamedEnvironmentContext &, const HouseholdPurchase &,
    const HouseholdServiceSegment &);
// S4-only actual cause. Market/right/transport causes are dispatched by the
// complete environment to their own original readers, never turned into
// success.
Outcome<FacilityClaimCloseReason> read_named_household_condition_interruption(
    const HouseholdNamedEnvironmentContext &, const HouseholdPurchase &,
    const SourceRef &, const WorldMoment &, bool terminate);
} // namespace hanmo::resident
