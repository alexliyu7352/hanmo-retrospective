// 原私务角色后果重放；告知身份/受托权/身体来源各守原 owner。
#include "household_purchase_disclosure_history.hpp"
#include "family_model.hpp"
#include "resident/domains/actor_foundation/character_availability_read.hpp"
#include "resident/domains/interaction/household_purchase_report.hpp"
#include "resident/domains/organization/organization_role_service_prefix.hpp"
#include "resident/domains/organization/organization_service_effects_codec.hpp"
#include "resident/domains/politics/office_reception/political_office_reception_profile.hpp"
namespace hanmo::resident {
namespace {
class PurchaseDisclosurePrefix final : public CommonActualFactPrefixSink {
  const HouseholdPurchaseDisclosureHistorySources &sources_;
  const RoleDefinition &role_;
  CommonActualFactPrefixSink *retained_;

public:
  bool enabled;
  OrganizationRoleDutyBeforePrefix duty;
  const OrganizationFact *enabling_parent = nullptr;
  const OrganizationServiceEffect *enabling_effect = nullptr;
  PurchaseDisclosurePrefix(const HouseholdPurchaseDisclosureHistorySources &s,
                           const RoleDefinition &role,
                           OrganizationRoleDutyBeforePrefix d,
                           CommonActualFactPrefixSink *retained)
      : sources_(s), role_(role), retained_(retained),
        enabled(*role.initial_enabled), duty(d) {}
  Status actual_prefix_fact(std::size_t position,
                            const FactRecord &head) override {
    if (head.domain == "organization" &&
        (head.kind == "organization_member_response" ||
         head.kind == "organization_member_services_synchronized" ||
         head.kind == "organization_office_changed")) {
      const auto *parent = sources_.organization.facts.find(head.id);
      if (!parent || parent->id != head.id || parent->kind != head.kind)
        return common_failure(
            "HouseholdDisclosureOriginalOrganizationParentRequired", head.id);
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
        return common_failure("HouseholdDisclosureOriginalFamilyChoiceRequired",
                              head.id);
      std::size_t listed = 0;
      for (const auto &id : choice->assets->disabled_roles)
        if (id == role_.id)
          ++listed;
      const bool expected =
          enabled && choice->guardian_actor_id == role_.initial_actor;
      if (listed > 1 || (listed == 1) != expected)
        return common_failure(
            "HouseholdDisclosureFamilyDisableCoverageMismatch", head.id);
      if (listed) {
        enabled = false;
        enabling_parent = nullptr;
        enabling_effect = nullptr;
      }
    } else if (head.domain == political_office_reception_domain) {
      // The original reception catalog accepts organization roles only. Never
      // reinterpret one of its actual reassignment writers as a private role.
      if (!sources_.office_receptions)
        return common_failure("HouseholdDisclosureReceptionSourceRequired",
                              head.id);
      auto valid = validate_political_office_reception_fact_header(
          sources_.condition.common, *sources_.office_receptions, head);
      if (!valid.ok)
        return valid;
      const auto *assignment =
          sources_.office_receptions->assignments.find(head.id);
      if (!assignment || assignment->role_id == role_.id)
        return common_failure(
            "HouseholdDisclosurePrivateRoleReassignmentRejected", head.id);
    }
    return retained_ ? retained_->actual_prefix_fact(position, head)
                     : common_success();
  }
};

} // namespace
Outcome<HouseholdPurchaseDisclosureControllerRead>
read_household_purchase_disclosure_controller_before_fact(
    const HouseholdPurchaseDisclosureHistorySources &s, const SourceRef &source,
    Integer schema, CommonActualFactPrefixSink *retained,
    CharacterAvailabilityProvenanceSink *retained_presence) {
  using Result = Outcome<HouseholdPurchaseDisclosureControllerRead>;
  const auto reject = [](const char *code,
                         const StableId &id = StableId{}) -> Result {
    return {false, {}, common_failure(code, id).error};
  };
  const auto &body = s.condition;
  const auto &common = body.common;
  if (body.shared != &s.shared || body.family_catalog != &s.family_catalog ||
      body.interaction != &s.interaction)
    return reject("HouseholdDisclosureSameWorldSourcesRequired", source.id);
  // 原 raw 口只核 Face3 实际行/头/前缀/父源顺序，不能改为 full 回读。
  auto actual = read_original_household_purchase_report_source(
      common, s.interaction, source, schema);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &row = *actual.value;
  const auto &proof = row.controller_authority;
  if (proof.actor_id != row.controller_actor_id ||
      proof.operation != household_purchase_operation)
    return reject("HouseholdDisclosureControllerProofMismatch", source.id);
  auto now = s.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same_now = compare_world_moments(now.value, body.actual_now);
  if (!same_now.ok)
    return {false, {}, same_now.error};
  if (same_now.value != 0)
    return reject("HouseholdDisclosureSameWorldMomentRequired", source.id);
  auto future = compare_world_moments(row.disclosed_at, now.value);
  if (!future.ok)
    return {false, {}, future.error};
  if (future.value > 0)
    return reject("HouseholdDisclosureFromFuture", source.id);
  auto valid =
      s.controls.validate_accepted(proof, row.disclosed_at.half_day_tick);
  if (!valid.ok)
    return {false, {}, valid.error};
  // 同值的外部 proof 不能替代本 World 的真实 Family 原件。
  const auto *saved = s.family.facts.find(proof.fact_id);
  const auto *mandate =
      saved ? std::get_if<HouseholdControlFact>(saved) : nullptr;
  if (!mandate || mandate->kind != "family_household_control" ||
      !mandate->actor_accepted || mandate->actor_id != proof.actor_id ||
      mandate->household_group_id != proof.household_group_id ||
      mandate->source_role_id != proof.source_role_id ||
      mandate->operation != proof.operation ||
      mandate->effective_from_tick != proof.effective_from_tick ||
      mandate->through_tick != proof.through_tick)
    return reject("HouseholdDisclosureOriginalMandateRequired", proof.fact_id);
  auto prefix = read_actual_common_fact_prefix_before(common, source);
  if (!prefix.ok)
    return {false, {}, prefix.error};
  auto cut = read_actual_common_fact_prefix(common, prefix.value);
  if (!cut.ok)
    return {false, {}, cut.error};
  auto accepted = read_actual_common_fact_position(
      common,
      SourceRefBuildView{"family", "family_household_control", proof.fact_id});
  if (!accepted.ok)
    return {false, {}, accepted.error};
  if (accepted.value >= cut.value)
    return reject("HouseholdDisclosureMandateMustPrecedeSource", proof.fact_id);
  const auto *role = s.shared.roles.find(proof.source_role_id);
  if (!role || !role->initial_enabled || role->organization_id ||
      role->initial_actor != proof.actor_id)
    return reject("HouseholdDisclosureAuthoredPrivateRoleRequired",
                  proof.source_role_id);
  if (!s.organization_catalog.service_effects_enabled)
    return reject("HouseholdDisclosureOriginalRoleEffectsRequired", role->id);
  valid = validate_organization_service_effects(s.organization, 1);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto duty = organization_detail::initialize_organization_role_duty_prefix(
      s.organization, s.organization_catalog, role->id, proof.actor_id);
  if (!duty.ok)
    return {false, {}, duty.error};
  PurchaseDisclosurePrefix reader(s, *role, duty.value, retained);
  valid = visit_actual_common_fact_prefix(common, prefix.value, reader);
  if (!valid.ok)
    return {false, {}, valid.error};
  // 严格原 reader 包含完整 Life4 / availability 覆盖，不降为 provenance 弱口。
  // FrozenBodyScalars 本来没有 available；绝不从伤势/HP 阈值猜可用性。
  auto presence = read_character_availability_before_fact_v2(
      body, proof.actor_id, source, row.disclosed_at, s.clock);
  if (!presence.ok)
    return {false, {}, presence.error};
  if (retained_presence) {
    auto visited = visit_character_availability_before_fact_provenance_v2(
        body, proof.actor_id, source, row.disclosed_at, s.clock,
        *retained_presence);
    if (!visited.ok)
      return {false, {}, visited.error};
    const auto &a = presence.value;
    const auto &b = visited.value;
    if (a.original_origin != b.original_origin || a.present != b.present ||
        a.alive != b.alive || a.available != b.available ||
        a.latest_availability_transition != b.latest_availability_transition ||
        a.actual_death != b.actual_death)
      return reject("HouseholdDisclosurePresenceChangedDuringVisit", source.id);
  }
  const auto &p = presence.value;
  return {true,
          {role, reader.enabled, p.present, p.alive, p.available,
           p.original_origin, p.latest_availability_transition, p.actual_death,
           reader.enabling_parent, reader.enabling_effect},
          {}};
}
} // namespace hanmo::resident
