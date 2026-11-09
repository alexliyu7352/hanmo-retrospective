// Finite representative proof for one actual Aid proposal, not a new mandate.
#pragma once
#include "household_control.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct OrganizationState;
struct OrganizationCatalog;
struct PoliticalOfficeReceptionState;
struct OrganizationFact;
struct OrganizationServiceEffect;
struct HouseholdAidStewardHistorySourcesV1 {
  const CommonStore &common;
  const FamilyState &family;
  const FamilyCatalog &family_catalog;
  const CommonCatalog &shared;
  const OrganizationState &organization;
  const OrganizationCatalog &organization_catalog;
  const LifeAgeSources *initial_age = nullptr;
  const PoliticalOfficeReceptionState *office_receptions = nullptr;
};
struct HouseholdAidStewardReadV1 {
  const HouseholdControlFact *accepted_steward = nullptr;
  const RoleDefinition *authored_role = nullptr;
  const FactRecord *proposal_head = nullptr;
  const OrganizationFact *enabling_parent = nullptr;
  const OrganizationServiceEffect *enabling_effect = nullptr;
};
// The Politics owner authenticates proposal2 payload and the actual consent M /
// immediately preceding prefix. S5 independently proves the selected
// container's historical title. This reader proves only the same
// actor/household's original accepted Steward and private role at that real
// prefix, never today's role or actor state. No generalized financial
// permission or second consent is created. Original Organization parent
// authorization / Family whole-source validation must run on these same roots;
// this reader adds exact role consequence replay. If retained, pin the WHOLE
// actual prefix, not only its final anchor.
Outcome<std::optional<HouseholdAidStewardReadV1>>
read_household_aid_steward_before_prefix_v1(
    const HouseholdAidStewardHistorySourcesV1 &,
    const SourceRef &steward_source, const StableId &actor,
    const StableId &household, const MomentOwnerSource &proposal,
    const WorldMoment &actual_consent_at, const CommonActualFactPrefix &,
    CommonActualFactPrefixSink *retained = nullptr);
Outcome<std::optional<HouseholdAidStewardReadV1>>
read_household_aid_steward_before_prefix_v1(
    const HouseholdAidStewardHistorySourcesV1 &,
    const SourceRef &steward_source, const StableId &actor,
    const StableId &household, const MomentOwnerSource &proposal,
    const WorldMoment &actual_consent_at, const CurrentCommonFactPrefix &,
    CommonActualFactPrefixSink *retained = nullptr);
} // namespace hanmo::resident
