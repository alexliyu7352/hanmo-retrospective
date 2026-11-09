#include "household_aid_steward_history_v1.hpp"
#include "family_model.hpp"
#include "resident/domains/organization/organization_role_service_prefix.hpp"
#include "resident/domains/organization/organization_service_effects_codec.hpp"
#include "resident/domains/politics/office_reception/political_office_reception_profile.hpp"
namespace hanmo::resident {
namespace {
class StewardPrefix final : public CommonActualFactPrefixSink {
  const HouseholdAidStewardHistorySourcesV1 &sources_;
  const RoleDefinition &role_;
  CommonActualFactPrefixSink *retained_;
  const WorldMoment &at_;

public:
  bool enabled;
  OrganizationRoleDutyBeforePrefix duty;
  const OrganizationFact *enabling_parent = nullptr;
  const OrganizationServiceEffect *enabling_effect = nullptr;
  StewardPrefix(const HouseholdAidStewardHistorySourcesV1 &s,
                const RoleDefinition &role, OrganizationRoleDutyBeforePrefix d,
                CommonActualFactPrefixSink *retained, const WorldMoment &at)
      : sources_(s), role_(role), retained_(retained), at_(at),
        enabled(*role.initial_enabled), duty(d) {}
  Status actual_prefix_fact(std::size_t position,
                            const FactRecord &head) override {
    if (head.domain == "organization" &&
        (head.kind == "organization_member_response" ||
         head.kind == "organization_member_services_synchronized" ||
         head.kind == "organization_office_changed")) {
      const auto *parent = sources_.organization.facts.find(head.id);
      if (!parent || parent->id != head.id || parent->kind != head.kind)
        return common_failure("HouseholdAidOriginalOrganizationParentRequired",
                              head.id);
      if (parent->tick < 0 || parent->tick > at_.half_day_tick)
        return common_failure("HouseholdAidFutureOrganizationRoleSource",
                              head.id);
      if (sources_.organization_catalog.exact_fact_clock_enabled !=
          parent->occurred_at.has_value())
        return common_failure(
            "HouseholdAidOrganizationRoleClockSelectionMismatch", head.id);
      if (parent->occurred_at) {
        if (parent->occurred_at->half_day_tick != parent->tick)
          return common_failure(
              "HouseholdAidOrganizationRoleClockProjectionMismatch", head.id);
        auto order = compare_world_moments(*parent->occurred_at, at_);
        if (!order.ok)
          return {false, {}, order.error};
        if (order.value > 0)
          return common_failure("HouseholdAidFutureOrganizationRoleSource",
                                head.id);
      }
      auto reduced = organization_detail::
          reduce_organization_role_service_effects_before_prefix(
              *parent, role_.id, role_.initial_actor, duty, enabled,
              enabling_parent, enabling_effect);
      if (!reduced.ok)
        return reduced;
    } else if (head.domain == "family" && head.kind == "family_succession") {
      const auto *saved = sources_.family.facts.find(head.id);
      const auto *choice = saved ? std::get_if<FamilyChoice>(saved) : nullptr;
      if (!choice || choice->kind != head.kind || !choice->assets)
        return common_failure("HouseholdAidOriginalFamilyChoiceRequired",
                              head.id);
      // A legacy integer boundary is checked only as its original projection;
      // it is not promoted to a fabricated exact zero-offset WorldMoment.
      if (choice->tick < 0 || choice->tick > at_.half_day_tick)
        return common_failure("HouseholdAidFutureFamilyRoleSource", head.id);
      std::size_t listed = 0;
      for (const auto &id : choice->assets->disabled_roles)
        if (id == role_.id)
          ++listed;
      const bool expected =
          enabled && choice->guardian_actor_id == role_.initial_actor;
      if (listed > 1 || (listed == 1) != expected)
        return common_failure("HouseholdAidFamilyDisableCoverageMismatch",
                              head.id);
      if (listed) {
        enabled = false;
        enabling_parent = nullptr;
        enabling_effect = nullptr;
      }
    } else if (head.domain == political_office_reception_domain) {
      // The original reception catalog accepts organization roles only. Never
      // reinterpret one of its actual reassignment writers as a private role.
      if (!sources_.office_receptions)
        return common_failure("HouseholdAidReceptionSourceRequired", head.id);
      auto valid = validate_political_office_reception_fact_header(
          sources_.common, *sources_.office_receptions, head);
      if (!valid.ok)
        return valid;
      const auto *assignment =
          sources_.office_receptions->assignments.find(head.id);
      if (!assignment || assignment->role_id == role_.id ||
          assignment->tick < 0 || assignment->tick > at_.half_day_tick)
        return common_failure("HouseholdAidPrivateRoleReassignmentRejected",
                              head.id);
    }
    return retained_ ? retained_->actual_prefix_fact(position, head)
                     : common_success();
  }
};
template <class Prefix>
Outcome<std::optional<HouseholdAidStewardReadV1>>
read(const HouseholdAidStewardHistorySourcesV1 &s,
     const SourceRef &steward_source, const StableId &actor,
     const StableId &household, const MomentOwnerSource &proposal,
     const WorldMoment &at, const Prefix &prefix,
     CommonActualFactPrefixSink *retained) {
  using Result = Outcome<std::optional<HouseholdAidStewardReadV1>>;
  const auto reject = [](const char *code,
                         const StableId &id = StableId{}) -> Result {
    return {false, {}, common_failure(code, id).error};
  };
  if (proposal.source_schema != 2 || proposal.source_ref.domain != "politics" ||
      proposal.source_ref.kind != "treaty_aid_resources_proposed" ||
      proposal.source_ref.id.empty())
    return reject("HouseholdAidSpecificProposal2Required");
  if (at.half_day_tick < 0 || at.half_day_tick > s.common.tick())
    return reject("HouseholdAidActualConsentMomentRequired");
  auto moment = compare_world_moments(at, at);
  if (!moment.ok)
    return {false, {}, moment.error};
  auto cut = read_actual_common_fact_prefix(s.common, prefix);
  if (!cut.ok)
    return {false, {}, cut.error};
  auto proposed =
      read_actual_common_fact_position(s.common, proposal.source_ref);
  if (!proposed.ok)
    return {false, {}, proposed.error};
  auto accepted = read_actual_common_fact_position(s.common, steward_source);
  if (!accepted.ok)
    return {false, {}, accepted.error};
  if (proposed.value >= cut.value || accepted.value >= cut.value)
    return reject("HouseholdAidSourcesMustPrecedeConsent");
  auto source = read_original_household_steward_source(
      s.common, s.family, s.family_catalog, s.shared, steward_source,
      s.initial_age);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &fact = *source.value;
  if (fact.actor_id != actor || fact.household_group_id != household)
    return reject("HouseholdAidStewardIdentityMismatch", steward_source.id);
  if (at.half_day_tick < fact.effective_from_tick ||
      (fact.through_tick && at.half_day_tick > *fact.through_tick))
    return {true, std::nullopt, {}};
  const auto *role = s.shared.roles.find(fact.source_role_id);
  if (!role || !role->initial_enabled || role->organization_id ||
      role->initial_actor != actor)
    return reject("HouseholdAidAuthoredPrivateRoleRequired",
                  fact.source_role_id);
  if (!s.organization_catalog.service_effects_enabled)
    return reject("HouseholdAidOriginalRoleEffectsRequired", role->id);
  auto effects = validate_organization_service_effects(s.organization, 1);
  if (!effects.ok)
    return {false, {}, effects.error};
  auto duty = organization_detail::initialize_organization_role_duty_prefix(
      s.organization, s.organization_catalog, role->id, actor);
  if (!duty.ok)
    return {false, {}, duty.error};
  StewardPrefix reader(s, *role, duty.value, retained, at);
  auto visited = visit_actual_common_fact_prefix(s.common, prefix, reader);
  if (!visited.ok)
    return {false, {}, visited.error};
  if (!reader.enabled)
    return {true, std::nullopt, {}};
  return {true,
          HouseholdAidStewardReadV1{
              source.value, role,
              s.common.state().facts.find(proposal.source_ref.id),
              reader.enabling_parent, reader.enabling_effect},
          {}};
}
} // namespace
Outcome<std::optional<HouseholdAidStewardReadV1>>
read_household_aid_steward_before_prefix_v1(
    const HouseholdAidStewardHistorySourcesV1 &s, const SourceRef &source,
    const StableId &actor, const StableId &household,
    const MomentOwnerSource &proposal, const WorldMoment &at,
    const CommonActualFactPrefix &prefix,
    CommonActualFactPrefixSink *retained) {
  return read(s, source, actor, household, proposal, at, prefix, retained);
}
Outcome<std::optional<HouseholdAidStewardReadV1>>
read_household_aid_steward_before_prefix_v1(
    const HouseholdAidStewardHistorySourcesV1 &s, const SourceRef &source,
    const StableId &actor, const StableId &household,
    const MomentOwnerSource &proposal, const WorldMoment &at,
    const CurrentCommonFactPrefix &prefix,
    CommonActualFactPrefixSink *retained) {
  return read(s, source, actor, household, proposal, at, prefix, retained);
}
} // namespace hanmo::resident
