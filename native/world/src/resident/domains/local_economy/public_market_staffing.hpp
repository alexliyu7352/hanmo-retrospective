#pragma once
#include "population_transport.hpp"
#include "public_market_staffing_model.hpp"
#include "resident/domains/organization/organization_model.hpp"
namespace hanmo::resident {
struct HouseholdPurchaseState;
struct HouseholdPurchaseCatalog;
struct HouseholdRetailCatalog;
struct PublicMarketStaffQualification {
  CivilianPopulationRef population_source;
  SourceRef location_source;
  std::vector<SourceRef> eligibility_sources;
  Integer eligible_workers = 0;
};
// 这些是原S6/S3/S1已应用结果的窄读口，不授setter或caller自报人数。
class PublicMarketStaffingOriginalRead {
public:
  virtual ~PublicMarketStaffingOriginalRead() = default;
  virtual Status validate_staffing_terms(const PublicMarketStaffingTerms &,
                                         const WorldMoment &,
                                         bool new_acceptance) const = 0;
  virtual Status validate_staffing_office(const PublicMarketStaffingTerms &,
                                          const ActingAuthority &,
                                          const WorldMoment &) const = 0;
  virtual Status validate_staffing_offer_authority(
      const PublicMarketStaffingOffer &) const = 0;
  virtual Status validate_staffing_contact(const SourceRef &,
                                           const PublicMarketStaffingTerms &,
                                           const StableId &consenting_actor,
                                           const WorldMoment &) const = 0;
  virtual Status
  validate_staffing_compensation(const PublicMarketStaffingTerms &,
                                 const MomentSpan &) const = 0;
  virtual Outcome<PublicMarketStaffQualification> read_staffing_qualification(
      const PublicMarketStaffingTerms &, const CivilianPopulationRef &,
      const SourceRef &location_source,
      const std::vector<SourceRef> &qualification_sources,
      const MomentSpan &) const = 0;
  virtual Outcome<OrganizationFact>
  read_verified_public_market_staffing_approval(const SourceRef &) const = 0;
  virtual Outcome<OrganizationFact>
  read_verified_public_market_staffing_boundary(const SourceRef &) const = 0;
  virtual Status
  validate_staffing_labor_change(const PublicMarketLaborOrigin &,
                                 const PublicMarketLaborTransition &) const = 0;
  virtual Outcome<PublicMarketLaborChange>
  read_applied_staffing_labor_change(const PublicMarketLaborOrigin &,
                                     const SourceRef &,
                                     const WorldMoment &) const = 0;
};
class PublicMarketStaffingOrganizationWrite {
public:
  virtual ~PublicMarketStaffingOrganizationWrite() = default;
  virtual Outcome<SourceRef>
  record_public_market_staffing_offer_authority_in_candidate(
      const SourceRef &actual_offer, const PublicMarketStaffingTerms &,
      const ActingAuthority &) = 0;
  virtual Outcome<OrganizationEffects>
  approve_public_market_staffing_in_candidate(const SourceRef &offer,
                                              const SourceRef &consent,
                                              const StableId &actual_activity,
                                              const StableId &actual_allocation,
                                              const ActingAuthority &) = 0;
};
class PublicMarketStaffingEffectsWrite {
public:
  virtual ~PublicMarketStaffingEffectsWrite() = default;
  virtual Status settle_public_market_staffing_before_change(
      const PublicMarketStaffingBinding &, const WorldMoment &,
      const SourceRef &actual_cause) = 0;
  virtual Status apply_public_market_staffing_capacity_change(
      const PublicMarketStaffingBinding &, const SourceRef &actual_event) = 0;
};
struct PublicMarketStaffingContext {
  const CommonStore &common;
  const LocalEconomyState &economy;
  const PopulationTransportState &population;
  const HouseholdPurchaseState &purchases;
  const HouseholdProvisionControlRead &controls;
  const PublicMarketStaffingOriginalRead &sources;
  WorldMoment now;
};
struct PublicMarketStaffingQuote {
  WorldIdentity identity;
  PublicMarketStaffingTerms terms;
  SourceRef local_contact_source;
  ActingAuthority acting;
  WorldMoment expires_at;
};
Outcome<PublicMarketStaffingQuote> preview_public_market_staffing(
    const PublicMarketStaffingContext &, const PublicMarketStaffingTerms &,
    const ActingAuthority &, const SourceRef &contact,
    const WorldMoment &expires);
Outcome<StableId> offer_public_market_staffing_in_candidate(
    CommonStore &, HouseholdPurchaseState &,
    const PublicMarketStaffingContext &,
    PublicMarketStaffingOrganizationWrite &, const PublicMarketStaffingQuote &);
Outcome<StableId> consent_public_market_staffing_in_candidate(
    CommonStore &, HouseholdPurchaseState &,
    const PublicMarketStaffingContext &, const StableId &offer,
    Integer expected_terms_revision, const StableId &actor,
    const SourceRef &actual_contact);
Outcome<StableId> accept_public_market_staffing_in_candidate(
    CommonStore &, LocalEconomyState &, PopulationTransportState &,
    HouseholdPurchaseState &, const PublicMarketStaffingContext &,
    PublicMarketStaffingOrganizationWrite &, PublicMarketStaffingEffectsWrite &,
    const StableId &offer, const StableId &consent, const ActingAuthority &);
Outcome<StableId> consent_public_market_staffing_release_in_candidate(
    CommonStore &, HouseholdPurchaseState &,
    const PublicMarketStaffingContext &, const StableId &binding,
    Integer release_tick, Integer terms_revision, const StableId &actor,
    const SourceRef &actual_contact);
// Original accepted allocation only; does not project future qualification
// spans.
Outcome<const PublicMarketLaborOrigin *>
read_public_market_staffing_labor_origin(const PublicMarketStaffingContext &,
                                         const StableId &binding);
Outcome<PublicMarketStaffingLaborHistoryView>
read_public_market_staffing_labor_history(const PublicMarketStaffingContext &,
                                          const StableId &binding,
                                          const MomentSpan &);
Outcome<StableId> apply_public_market_labor_change_in_candidate(
    CommonStore &, PopulationTransportState &, HouseholdPurchaseState &,
    const PublicMarketStaffingContext &, PublicMarketStaffingEffectsWrite &,
    const SourceRef &origin, const SourceRef &actual_cause);
Status on_public_market_staffing_boundary_in_candidate(
    CommonStore &, LocalEconomyState &, PopulationTransportState &,
    HouseholdPurchaseState &, const PublicMarketStaffingContext &,
    PublicMarketStaffingEffectsWrite &, const StableId &activity,
    const SourceRef &actual_boundary);
Status validate_public_market_staffing(const PublicMarketStaffingContext &);
// 仅核真实选型与反向覆盖；作者公市、已发生公市行或原Common头存在时，
// 不得省略公市上下文。此口不授人员能力、经营许可或当前付款权。
Status validate_unselected_public_household_market(
    const CommonStore &, const HouseholdPurchaseCatalog &,
    const HouseholdPurchaseState &, const PopulationTransportState &,
    const HouseholdRetailCatalog &);
} // namespace hanmo::resident
