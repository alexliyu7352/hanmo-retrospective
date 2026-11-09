// 组织命令边界只解有限参数并核真实发令者；预览不分配ID或推进世界。
#include "dynamic_integration.hpp"
#include "named_membership.hpp"
#include "organization_contract.hpp"
#include "organization_grant_route.hpp"
#include "resident/domains/organization/named_membership_rules.hpp"
namespace hanmo::resident {
namespace {
ActingAuthority read_authority(const BoundaryValue &value) {
  using namespace boundary;
  fields(value, {"organization_id", "office_id", "authority_epoch", "actor_id"},
         {}, "authority");
  return {get_string(value, "organization_id", "authority"),
          get_string(value, "office_id", "authority"),
          get_int(value, "authority_epoch", "authority"),
          get_string(value, "actor_id", "authority")};
}
SubjectRef read_subject(const BoundaryValue &value) {
  using namespace boundary;
  fields(value, {"kind", "id"}, {}, "subject");
  const auto kind = get_string(value, "kind", "subject");
  SubjectKind tag;
  if (kind == "actor")
    tag = SubjectKind::actor;
  else if (kind == "organization")
    tag = SubjectKind::organization;
  else if (kind == "cohort")
    tag = SubjectKind::cohort;
  else
    fail("InvalidSubjectKind", "subject/kind");
  return {tag, get_string(value, "id", "subject")};
}
Status actor_matches(const StableId &actor, const StableId &declared) {
  return actor == declared ? common_success()
                           : common_failure("CommandActorMismatch");
}
} // namespace
bool dynamic_command_kind(const StableId &kind) {
  return organization_grant_command_kind(kind) ||
         named_membership_command_kind(kind) ||
         kind == "organization_accept_offer" ||
         kind == "organization_member_response" ||
         kind == "organization_recruit" || kind == "organization_activity";
}
Outcome<CommandPayload> decode_dynamic_command(const StableId &kind,
                                               const BoundaryValue &params) {
  try {
    using namespace boundary;
    if (organization_grant_command_kind(kind)) {
      auto decoded = decode_organization_grant_command(kind, params);
      if (!decoded.ok)
        return {false, {}, decoded.error};
      return {true, {kind, std::move(decoded.value)}, {}};
    }
    if (named_membership_command_kind(kind))
      return decode_named_membership_command(kind, params);
    if (kind == "organization_accept_offer") {
      fields(params,
             {"authority", "executor_actor_id", "payee_wallet_id", "task"}, {},
             "params");
      OrganizationContractCommand command;
      command.authority = read_authority(field(params, "authority", "params"));
      command.executor_actor_id =
          get_string(params, "executor_actor_id", "params");
      command.payee_wallet_id = get_string(params, "payee_wallet_id", "params");
      const auto &task = field(params, "task", "params");
      fields(task, {"offer_ref", "need_revision", "quantity", "context"},
             {"offer_id"}, "params/task");
      auto decoded = decode_task_command("accept_offer", task);
      if (!decoded.ok)
        return {false, {}, decoded.error};
      command.task = std::move(decoded.value);
      return {true, {kind, std::move(command)}, {}};
    }
    if (kind == "organization_member_response") {
      fields(params, {"actor_id", "organization_id", "source_fact_id", "kind"},
             {}, "params");
      OrganizationMemberCommand command;
      command.actor_id = get_string(params, "actor_id", "params");
      command.organization_id = get_string(params, "organization_id", "params");
      command.source_fact_id = get_string(params, "source_fact_id", "params");
      const auto action = get_string(params, "kind", "params");
      if (action == "request_payment")
        command.kind = MemberCommandKind::request_payment;
      else if (action == "wait_payment")
        command.kind = MemberCommandKind::wait_payment;
      else if (action == "decline_new_work")
        command.kind = MemberCommandKind::decline_new_work;
      else if (action == "lawful_leave")
        command.kind = MemberCommandKind::lawful_leave;
      else
        fail("InvalidMemberResponse", "params/kind");
      return {true, {kind, std::move(command)}, {}};
    }
    if (kind == "organization_recruit") {
      fields(params,
             {"authority", "source_cohort_id", "target_cohort_id",
              "activity_terms_id", "count", "labor_count",
              "release_allocation_ids"},
             {}, "params");
      OrganizationRecruitCommand command;
      command.authority = read_authority(field(params, "authority", "params"));
      command.source_cohort_id =
          get_string(params, "source_cohort_id", "params");
      command.target_cohort_id =
          get_string(params, "target_cohort_id", "params");
      command.activity_terms_id =
          get_string(params, "activity_terms_id", "params");
      command.count = get_int(params, "count", "params");
      command.labor_count = get_int(params, "labor_count", "params");
      command.release_allocation_ids =
          strings(field(params, "release_allocation_ids", "params"),
                  "params/release_allocation_ids");
      return {true, {kind, std::move(command)}, {}};
    }
    if (kind == "organization_activity") {
      fields(params,
             {"authority", "terms_id", "participation", "commitment_ids"}, {},
             "params");
      OrganizationActivityCommand command;
      command.authority = read_authority(field(params, "authority", "params"));
      command.terms_id = get_string(params, "terms_id", "params");
      command.commitment_ids = strings(
          field(params, "commitment_ids", "params"), "params/commitment_ids");
      for (const auto &value : array(field(params, "participation", "params"),
                                     "params/participation")) {
        fields(value, {"subject", "role", "binding_kind", "binding_id"}, {},
               "participation");
        Participation part;
        part.subject = read_subject(field(value, "subject", "participation"));
        part.role = get_string(value, "role", "participation");
        part.binding_id = get_string(value, "binding_id", "participation");
        const auto binding = get_string(value, "binding_kind", "participation");
        if (binding == "administrative_reference")
          part.binding_kind = ParticipationKind::administrative_reference;
        else if (binding == "personal_action")
          part.binding_kind = ParticipationKind::personal_action;
        else if (binding == "cohort_allocation")
          part.binding_kind = ParticipationKind::cohort_allocation;
        else
          fail("InvalidParticipationKind", "participation/binding_kind");
        command.participation.push_back(std::move(part));
      }
      return {true, {kind, std::move(command)}, {}};
    }
    return {false, {}, common_failure("UnsupportedAction", kind).error};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Status preview_dynamic_command(const WorldBundle &world, const StableId &actor,
                               const CommandPayload &command) {
  if (!world.dynamic_enabled())
    return common_failure("DynamicProfileRequired");
  if (const auto *value = std::get_if<GrantChangeCommand>(&command.data))
    return preview_organization_grant_command(world, actor, command.kind,
                                              *value);
  if (organization_grant_command_kind(command.kind))
    return common_failure("CommandPayloadKindMismatch");
  if (const auto *value =
          std::get_if<OrganizationContractCommand>(&command.data)) {
    auto quote = preview_organization_contract(world, actor, *value);
    return {quote.ok, {}, quote.error};
  }
  if (const auto *value =
          std::get_if<OrganizationMemberCommand>(&command.data)) {
    auto checked = actor_matches(actor, value->actor_id);
    if (!checked.ok)
      return checked;
    if (world.named_membership_enabled() && !value->source_fact_id.empty() &&
        !membership_fact_known(world.common, world.social, actor,
                               value->source_fact_id))
      return common_failure("MembershipFactNotKnown", value->source_fact_id);
    if (world.named_membership_enabled())
      return preview_named_membership_member_command(
          world.common, world.organization, *world.catalog->organization,
          world.local_economy, world.services, *value, world.common.tick());
    return preview_organization_member_command(
        world.common, world.organization, *world.catalog->organization,
        world.local_economy, world.services, *value, world.common.tick());
  }
  if (const auto *value =
          std::get_if<OrganizationRecruitCommand>(&command.data)) {
    auto checked = actor_matches(actor, value->authority.actor_id);
    if (!checked.ok)
      return checked;
    return preview_organization_recruit_command(
        world.common, world.organization, *world.catalog->organization,
        world.local_economy, world.services, *value, world.common.tick());
  }
  if (const auto *value =
          std::get_if<OrganizationActivityCommand>(&command.data)) {
    auto checked = actor_matches(actor, value->authority.actor_id);
    if (!checked.ok)
      return checked;
    return preview_organization_activity_command(
        world.common, world.organization, *world.catalog->organization,
        world.local_economy, world.services, *value, world.common.tick(),
        &world.services);
  }
  return common_failure("UnsupportedAction", command.kind);
}
Outcome<OrganizationEffects>
apply_dynamic_command(WorldBundle &world, const StableId &actor,
                      const CommandPayload &command) {
  auto checked = preview_dynamic_command(world, actor, command);
  if (!checked.ok)
    return {false, {}, checked.error};
  if (const auto *value = std::get_if<GrantChangeCommand>(&command.data))
    return apply_organization_grant_command(world, actor, command.kind, *value);
  if (const auto *value = std::get_if<OrganizationMemberCommand>(&command.data))
    if (world.named_membership_enabled())
      return apply_named_membership_member_command(
          world.common, world.organization, *world.catalog->organization,
          world.local_economy, *world.catalog->local_economy, world.services,
          world.services, *value, world.common.tick());
  if (const auto *value = std::get_if<OrganizationMemberCommand>(&command.data))
    return apply_organization_member_command(
        world.common, world.organization, *world.catalog->organization,
        world.local_economy, world.services, *value, world.common.tick());
  if (const auto *value =
          std::get_if<OrganizationRecruitCommand>(&command.data))
    return apply_organization_recruit_command(
        world.common, world.organization, *world.catalog->organization,
        world.local_economy, world.services, *value, world.common.tick());
  if (const auto *value =
          std::get_if<OrganizationActivityCommand>(&command.data))
    return apply_organization_activity_command(
        world.common, world.organization, *world.catalog->organization,
        world.local_economy, world.services, *value, world.common.tick(),
        &world.services);
  return {false, {}, common_failure("UnsupportedAction", command.kind).error};
}
} // namespace hanmo::resident
