// 组织目录、任职、职责和活动的严格同候选验证；墓碑仅保历史引用。
#include "organization_validation.hpp"
#include "grant_authority.hpp"
#include "grant_terms_definition.hpp"
#include "organization_internal.hpp"
#include "organization_member_prefix.hpp"
#include "organization_role_service_prefix.hpp"
#include "organization_validation_detail.hpp"
namespace hanmo::resident {
namespace {
using namespace organization_detail;
// 行政父链和组织分支分别检查，不能靠交叉关系掩盖一个环。
Status hierarchy(const OrganizationState &state, bool administrative) {
  for (const auto &row : state.organizations.rows()) {
    std::set<StableId> seen;
    const auto *organization = &row.second;
    while (organization) {
      if (!seen.insert(organization->id).second)
        return common_failure("OrganizationHierarchyCycle", row.first);
      const auto &parent = administrative ? organization->administrative_parent
                                          : organization->branch_parent;
      if (!parent)
        break;
      organization = state.organizations.find(*parent);
      if (!organization)
        return common_failure("UnknownParentOrganization", *parent);
    }
  }
  return common_success();
}
// 所有服务职责仍引用共同 RoleRecord，不在组织状态复制教师持有人。
Status members(const CommonStore &c, const OrganizationState &s,
               const OrganizationCatalog &catalog,
               const LocalEconomyState &economy, const ActivityRead *activity,
               Integer current_tick) {
  std::set<std::pair<StableId, StableId>> identities;
  if ((!catalog.named_membership_enabled &&
       (s.members.size() != catalog.members.size() ||
        s.duties.size() != catalog.duties.size())) ||
      s.members.size() < catalog.members.size() ||
      s.duties.size() < catalog.duties.size())
    return common_failure("OrganizationMemberRegistryMismatch");
  if (catalog.named_membership_enabled) {
    for (const auto &row : catalog.members.rows())
      if (!s.members.find(row.first))
        return common_failure("MissingSeedOrganizationMember", row.first);
    for (const auto &row : catalog.duties.rows())
      if (!s.duties.find(row.first))
        return common_failure("MissingSeedOrganizationDuty", row.first);
  }
  for (const auto &row : catalog.member_policies.rows()) {
    const auto &p = row.second;
    if (row.first != p.id || p.request_after_ticks < 0 ||
        p.refuse_after_ticks < p.request_after_ticks ||
        p.leave_after_ticks < p.refuse_after_ticks || p.minimum_arrears <= 0)
      return common_failure("InvalidMemberPolicy", row.first);
  }
  auto role_duty_sources =
      validate_organization_role_duty_provenance(s, catalog);
  if (!role_duty_sources.ok)
    return role_duty_sources;
  for (const auto &row : s.duties.rows()) {
    const auto &duty = row.second;
    const auto *initial = catalog.duties.find(row.first);
    const OrgMemberState *dynamic_member = nullptr;
    if (!initial && catalog.named_membership_enabled)
      for (const auto &member : s.members.rows())
        if (member.second.episode_source &&
            has(member.second.duty_ids, duty.id)) {
          if (dynamic_member)
            return common_failure("DuplicateDynamicMembershipDuty", duty.id);
          dynamic_member = &member.second;
        }
    if (row.first != duty.id || (!initial && !dynamic_member) ||
        !s.organizations.find(duty.organization_id) ||
        !c.actor(duty.actor_id) ||
        !catalog.member_policies.find(duty.policy_id) ||
        (initial && duty.role_id.empty() &&
         (duty.actor_id != initial->actor_id ||
          duty.organization_id != initial->organization_id ||
          duty.role_id != initial->role_id)))
      return common_failure("InvalidOrganizationDuty", row.first);
    if (duty.activity_terms_id &&
        !catalog.activity_terms.find(*duty.activity_terms_id))
      return common_failure("UnknownDutyActivityTerms", row.first);
    if (!duty.office_id.empty()) {
      const auto *office = s.offices.find(duty.office_id);
      if (!office || office->organization_id != duty.organization_id)
        return common_failure("InvalidDutyOffice", row.first);
    }
    if (!duty.role_id.empty()) {
      const auto *role = c.state().roles.find(duty.role_id);
      if (!role)
        return common_failure("InvalidDutyRole", row.first);
      if (duty.active && role->actor_id != duty.actor_id)
        return common_failure("DutyRoleHolderMismatch", row.first);
    }
    for (const auto &id : duty.obligation_ids) {
      const auto *obligation = economy.obligations.find(id);
      if (!obligation ||
          obligation->principal.kind != SubjectKind::organization ||
          obligation->principal.id != duty.organization_id ||
          obligation->beneficiary.kind != SubjectKind::actor ||
          obligation->beneficiary.id != duty.actor_id)
        return common_failure("MemberObligationMismatch", id);
    }
    for (const auto &id : duty.commitment_ids)
      if (!c.action(id) && !s.activities.find(id) && !c.state().facts.find(id))
        return common_failure("UnknownDutyCommitment", id);
  }
  for (const auto &row : s.members.rows()) {
    const auto &m = row.second;
    auto identity =
        validate_organization_member_identity(row.first, m, catalog);
    if (!identity.ok)
      return identity;
    if (!s.organizations.find(m.organization_id) || !c.actor(m.actor_id) ||
        !catalog.member_policies.find(m.policy_id) ||
        m.response_since_tick < 0 || m.response_since_tick > current_tick ||
        m.next_review_tick < 0 ||
        ((!catalog.named_membership_enabled ||
          m.response != MemberResponse::departed) &&
         !identities.insert({m.organization_id, m.actor_id}).second))
      return common_failure("InvalidOrganizationMember", row.first);
    std::set<StableId> duties;
    for (const auto &id : m.duty_ids) {
      const auto *duty = s.duties.find(id);
      if (!duty || duty->actor_id != m.actor_id ||
          duty->organization_id != m.organization_id ||
          !duties.insert(id).second ||
          ((m.response == MemberResponse::departed) && duty->active))
        return common_failure("InvalidMemberDuty", id);
      if (duty->active && !duty->role_id.empty() &&
          (m.response == MemberResponse::refusing_new_duties ||
           m.response == MemberResponse::awaiting_release)) {
        const auto *role = c.state().roles.find(duty->role_id);
        if (role && role->actor_id == m.actor_id && role->enabled &&
            !has_personal_action(c, m.actor_id) &&
            (!activity || !activity->has_existing_contract_service(
                              m.actor_id, duty->role_id)))
          return common_failure("RefusingMemberServiceEnabled", id);
      }
    }
    if (!m.last_fact_id.empty()) {
      const auto *fact = s.facts.find(m.last_fact_id);
      if (!fact || fact->actor_id != m.actor_id ||
          fact->kind != "organization_member_response" ||
          fact->response != m.response ||
          (catalog.named_membership_enabled &&
           (fact->organization_id != m.organization_id ||
            fact->tick != m.response_since_tick)))
        return common_failure("InvalidMemberResponseFact", row.first);
    }
    auto owed = debt(c, s, economy, m);
    if (!owed.ok)
      return {false, {}, owed.error};
  }
  return common_success();
}
// 活动的个人行动与群体分配按唯一来源归属；完成训练后允许原分配已释放。
Status activities(const CommonStore &c, const OrganizationState &s,
                  const OrganizationCatalog &catalog,
                  const LocalEconomyState &economy,
                  const DynamicResultRead *results, Integer current_tick,
                  const OrganizationCompleteValidationContext *complete) {
  std::set<StableId> allocations;
  std::map<StableId, Integer> training;
  for (const auto &row : catalog.activity_terms.rows()) {
    const auto &t = row.second;
    if (row.first != t.id || t.duration_ticks <= 0 || t.training_capacity < 0 ||
        t.continuity_policy_id != "continue_under_organization")
      return common_failure("InvalidOrganizationActivityTerms", row.first);
    if (t.kind == OrgActivityKind::population_training &&
        (t.training_capacity <= 0 || t.training_site_id.empty() ||
         t.consenting_cohort_ids.empty()))
      return common_failure("InvalidVoluntaryTrainingPolicy", row.first);
    std::set<StableId> consent;
    for (const auto &id : t.consenting_cohort_ids)
      if (!economy.cohorts.find(id) || !consent.insert(id).second)
        return common_failure("InvalidTrainingConsentCohort", id);
  }
  for (const auto &row : s.activities.rows()) {
    const auto &a = row.second;
    // 只允许原 Calendar 会逐 approval/Opening 反验的非 seed
    // 日历活动预排下一期。 personal Action
    // 与市场有各自实际发生来源，不能借此接受任意未来 started_tick。
    const bool original_calendar =
        complete && !catalog.initial_activities.find(a.id) &&
        std::none_of(
            a.participation.begin(), a.participation.end(), [](const auto &p) {
              return p.binding_kind == ParticipationKind::personal_action ||
                     p.role == "market_service_staffing";
            });
    const auto *terms = catalog.activity_terms.find(a.terms_id);
    const auto *organization = s.organizations.find(a.principal.id);
    const auto *office = s.offices.find(a.responsible_office_id);
    if (row.first != a.id || !terms || terms->kind != a.kind ||
        a.principal.kind != SubjectKind::organization || !organization ||
        !office || office->organization_id != organization->id ||
        a.started_tick < 0 ||
        (a.started_tick > current_tick && !original_calendar) ||
        a.phase_started_tick < a.started_tick || a.due_tick < a.started_tick ||
        a.outcome_sequence < 0 || a.execution.activity_id != a.id ||
        a.execution.phase != a.phase || a.execution.execution_epoch < 0 ||
        a.execution.mode != ExecutionMode::background ||
        a.execution.local_session_id)
      return common_failure("InvalidOrganizationActivity", row.first);
    if (a.execution.last_receipt_id &&
        !has(a.result_fact_ids, *a.execution.last_receipt_id))
      return common_failure("InvalidActivityReceipt", row.first);
    if (!matching_executors(a))
      return common_failure("OrganizationExecutorBindingMismatch", a.id);
    bool has_personal = false, terminal_failed = false;
    StableId terminal_code;
    std::set<StableId> persons;
    for (const auto &p : a.participation) {
      if (p.binding_kind == ParticipationKind::administrative_reference) {
        if (p.subject.kind == SubjectKind::actor && !c.actor(p.subject.id))
          return common_failure("UnknownActivityActor", p.subject.id);
        continue;
      }
      if (p.binding_kind == ParticipationKind::personal_action) {
        has_personal = true;
        if (p.subject.kind != SubjectKind::actor || !c.actor(p.subject.id) ||
            !persons.insert(p.subject.id).second)
          return common_failure("InvalidActivityParticipation", row.first);
        const auto *action = c.action(p.binding_id);
        if (!results)
          return common_failure("OrganizationActionResultReadRequired",
                                p.binding_id,
                                ErrorKind::unsupported_capability);
        if (action && (a.status == OrgActivityStatus::completed ||
                       a.status == OrgActivityStatus::failed))
          return common_failure("PrematureOrganizationActivityCompletion",
                                a.id);
        if (action && !results->supports_action_result(action->kind))
          return common_failure("UnsupportedOrganizationActionResult",
                                action->kind,
                                ErrorKind::unsupported_capability);
        if (!action) {
          auto terminal = results->action_result(p.binding_id);
          if (!terminal || terminal->action_id != p.binding_id ||
              !results->supports_action_result(terminal->action_kind) ||
              terminal->source_fact_id.empty() ||
              !c.state().facts.find(terminal->source_fact_id) ||
              terminal->completed_tick < a.started_tick ||
              terminal->completed_tick > current_tick)
            return common_failure("MissingOrganizationActionResult",
                                  p.binding_id);
          auto bound = terminal_participant(c, *terminal, p.subject);
          if (!bound.ok)
            return bound;
          if (!terminal->success) {
            if (!terminal_failed || terminal_code.empty())
              terminal_code = terminal->code;
            terminal_failed = true;
          } else if (!terminal_failed && terminal_code.empty())
            terminal_code = terminal->code;
          if (a.status == OrgActivityStatus::completed && !terminal->success)
            return common_failure("FailedActionReportedCompleted", a.id);
          if ((a.status == OrgActivityStatus::completed ||
               a.status == OrgActivityStatus::failed) &&
              !a.result_fact_ids.empty()) {
            const auto *receipt = s.facts.find(a.result_fact_ids.back());
            if (!receipt ||
                !has(receipt->cause_fact_ids, terminal->source_fact_id))
              return common_failure("MissingActivityTerminalCause", a.id);
          }
        }
        if (action && action->actor_id != p.subject.id &&
            !has(action->participants, p.subject.id))
          return common_failure("ActivityParticipantMismatch", p.subject.id);
      } else {
        const auto *cohort = economy.cohorts.find(p.subject.id);
        const auto *allocation = economy.labor_allocations.find(p.binding_id);
        if (p.subject.kind != SubjectKind::cohort || !cohort)
          return common_failure("InvalidOrganizationAllocation", p.binding_id);
        if (active_activity(a)) {
          if (!allocation || allocation->cohort_id != cohort->id ||
              allocation->activity_or_production_id != a.id ||
              !allocations.insert(p.binding_id).second)
            return common_failure("InvalidOrganizationAllocation",
                                  p.binding_id);
          if (a.kind == OrgActivityKind::population_training) {
            if (cohort->status != PopulationStatus::training ||
                cohort->assignment_id != std::optional<StableId>(a.id) ||
                cohort->site_id != terms->training_site_id ||
                !add(training[terms->id], cohort->headcount,
                     training[terms->id]))
              return common_failure("InvalidTrainingContinuity", a.id);
          }
        } else if (a.kind == OrgActivityKind::population_training &&
                   (cohort->assignment_id == std::optional<StableId>(a.id) ||
                    allocation ||
                    cohort->status != PopulationStatus::trained_reserve))
          return common_failure("TrainingAllocationNotReleased", a.id);
      }
    }
    if (has_personal && (a.status == OrgActivityStatus::completed ||
                         a.status == OrgActivityStatus::failed)) {
      if ((a.status == OrgActivityStatus::failed) != terminal_failed ||
          a.result_code != terminal_code)
        return common_failure("ActivityTerminalOutcomeMismatch", a.id);
    }
    for (const auto &id : a.result_fact_ids) {
      const auto *fact = s.facts.find(id);
      if (!fact || fact->activity_id != a.id)
        return common_failure("InvalidActivityResultFact", id);
    }
    if ((a.status == OrgActivityStatus::completed ||
         a.status == OrgActivityStatus::failed) &&
        a.result_fact_ids.empty())
      return common_failure("MissingActivityCompletion", a.id);
    if (a.status == OrgActivityStatus::failed) {
      const auto *result = s.facts.find(a.result_fact_ids.back());
      if (!result || result->kind != "organization_activity_failed" ||
          result->code != a.result_code)
        return common_failure("InvalidActivityFailure", a.id);
    }
  }
  for (const auto &row : training) {
    const auto *terms = catalog.activity_terms.find(row.first);
    if (!terms || row.second > terms->training_capacity)
      return common_failure("TrainingCapacityExceeded", row.first);
  }
  return common_success();
}
} // namespace
Status validate_organization(const CommonStore &c, const OrganizationState &s,
                             const OrganizationCatalog &catalog,
                             const LocalEconomyState &economy,
                             const DynamicResultRead *results,
                             const ActivityRead *activity) {
  return validate_organization(c, s, catalog, economy, results, activity, {});
}
// 稳定态允许有真实产权链的旧授权留档；check_authority仍只认当前所有权。
Status validate_organization(const CommonStore &c, const OrganizationState &s,
                             const OrganizationCatalog &catalog,
                             const LocalEconomyState &economy,
                             const DynamicResultRead *results,
                             const ActivityRead *activity,
                             const OrganizationResourceHistoryRead &history) {
  return validate_organization(c, s, catalog, economy, results, activity,
                               history, {});
}
Status validate_organization(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    const DynamicResultRead *results, const ActivityRead *activity,
    const OrganizationResourceHistoryRead &history,
    const OrganizationProspectiveGrantRead &prospective) {
  return validate_organization(c, s, catalog, economy, results, activity,
                               history, prospective, nullptr);
}
Status validate_organization(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    const DynamicResultRead *results, const ActivityRead *activity,
    const OrganizationResourceHistoryRead &history,
    const OrganizationProspectiveGrantRead &prospective,
    const OrganizationAuthoritySources *authority) {
  return organization_detail::validate_organization_body(
      c, s, catalog, economy, results, activity, history, prospective,
      authority, nullptr);
}
// 共用原结构规则；完整路径的新增主体仍须由后续原生命周期逐源反验。
Status organization_detail::validate_organization_body(
    const CommonStore &c, const OrganizationState &s,
    const OrganizationCatalog &catalog, const LocalEconomyState &economy,
    const DynamicResultRead *results, const ActivityRead *activity,
    const OrganizationResourceHistoryRead &history,
    const OrganizationProspectiveGrantRead &prospective,
    const OrganizationAuthoritySources *authority,
    const OrganizationCompleteValidationContext *complete) {
  if (catalog.authority_lifecycle_enabled && !authority)
    return common_failure("OfficeLifecycleSourceReadRequired");
  Integer current_tick = c.tick();
  if (complete) {
    auto now = read_organization_authority_now(c, &complete->clock);
    if (!now.ok)
      return {false, {}, now.error};
    current_tick = now.value.half_day_tick;
  }
  if (!catalog.enabled) {
    if (!s.current_grants.empty() || !s.organizations.empty() ||
        !s.offices.empty() || !s.members.empty() || !s.duties.empty() ||
        !s.activities.empty() || !s.facts.empty() ||
        !s.membership_offers.empty() || !s.membership_decisions.empty())
      return common_failure("DisabledOrganizationHasState");
    return common_success();
  }
  if (!complete && (s.organizations.size() != catalog.organizations.size() ||
                    s.offices.size() != catalog.offices.size()))
    return common_failure("OrganizationRegistryMismatch");
  for (const auto &row : s.organizations.rows()) {
    const auto &o = row.second;
    const auto *initial = catalog.organizations.find(row.first);
    if (row.first != o.id || (!initial && !complete) || o.kind.empty() ||
        (o.headquarters_site ? o.headquarters_site->empty()
                             : !catalog.authority_lifecycle_enabled) ||
        (initial && o.kind != initial->kind))
      return common_failure("InvalidOrganization", row.first);
    if (catalog.authority_lifecycle_enabled &&
        !organization_lifecycle_tag(o.status))
      return common_failure("InvalidOrganizationLifecycle", row.first);
    std::set<StableId> offices;
    for (const auto &id : o.office_ids) {
      const auto *office = s.offices.find(id);
      if (!office || office->organization_id != o.id ||
          !offices.insert(id).second)
        return common_failure("InvalidOrganizationOffice", id);
    }
  }
  auto admin = hierarchy(s, true);
  if (!admin.ok)
    return admin;
  auto branch = hierarchy(s, false);
  if (!branch.ok)
    return branch;
  for (const auto &row : s.offices.rows()) {
    const auto &o = row.second;
    const auto *org = s.organizations.find(o.organization_id);
    if (row.first != o.id || (!complete && !catalog.offices.find(o.id)) ||
        !org || !has(org->office_ids, o.id) || o.epoch < 0 ||
        (o.status == OfficeStatus::active && !o.holder_actor_id) ||
        (o.holder_actor_id && !c.actor(*o.holder_actor_id)) ||
        (o.deputy_actor_id && !c.actor(*o.deputy_actor_id)))
      return common_failure("InvalidOfficeState", row.first);
    std::set<StableId> grants;
    for (const auto &id : o.grant_ids) {
      const auto *runtime = catalog.authority_lifecycle_enabled
                                ? s.current_grants.find(id)
                                : nullptr;
      const auto *grant = catalog.authority_lifecycle_enabled
                              ? (runtime ? &runtime->terms : nullptr)
                              : catalog.grants.find(id);
      if (!grant || grant->office_id != o.id || !grants.insert(id).second)
        return common_failure("InvalidOfficeGrant", id);
    }
  }
  for (const auto &row : catalog.grants.rows()) {
    const auto &g = row.second;
    const auto *office = s.offices.find(g.office_id);
    if (row.first != g.id || !office)
      return common_failure("InvalidAuthorityGrant", row.first);
    auto definition = validate_authored_grant_terms_definition(g);
    if (!definition.ok)
      return definition;
    for (const auto &id : g.wallet_ids) {
      const auto *w = c.wallet(id);
      if (!w ||
          ((w->owner.kind != Owner::Kind::organization ||
            w->owner.id != office->organization_id) &&
           (!history || !history({ResourceKey::Kind::wallet, id},
                                 office->organization_id)) &&
           (!prospective || !prospective(g, {ResourceKey::Kind::wallet, id}))))
        return common_failure("AuthorityResourceMismatch", id);
    }
    for (const auto &id : g.inventory_ids) {
      const auto *i = c.inventory(id);
      if (!i || ((i->owner.kind != Owner::Kind::organization ||
                  i->owner.id != office->organization_id) &&
                 (!history || !history({ResourceKey::Kind::inventory, id},
                                       office->organization_id)) &&
                 (!prospective ||
                  !prospective(g, {ResourceKey::Kind::inventory, id}))))
        return common_failure("AuthorityResourceMismatch", id);
    }
  }
  if (catalog.authority_lifecycle_enabled) {
    for (const auto &row : s.current_grants.rows()) {
      const auto &g = row.second.terms;
      const auto *office = s.offices.find(g.office_id);
      if (row.first != g.id || !office || g.amount_limit < 0 ||
          g.valid_from < 0 ||
          (g.valid_until && *g.valid_until < g.valid_from) ||
          g.allowed_command_kinds.empty() || g.scopes.empty() ||
          (g.continuity_policy_id != "continue_under_organization" &&
           g.continuity_policy_id != "stop_when_vacant"))
        return common_failure("InvalidRuntimeGrant", row.first);
      for (const auto &id : g.wallet_ids) {
        const auto *resource = c.wallet(id);
        if (!resource || ((resource->owner.kind != Owner::Kind::organization ||
                           resource->owner.id != office->organization_id) &&
                          (!history || !history({ResourceKey::Kind::wallet, id},
                                                office->organization_id))))
          return common_failure("AuthorityResourceMismatch", id);
      }
      for (const auto &id : g.inventory_ids) {
        const auto *resource = c.inventory(id);
        if (!resource ||
            ((resource->owner.kind != Owner::Kind::organization ||
              resource->owner.id != office->organization_id) &&
             (!history || !history({ResourceKey::Kind::inventory, id},
                                   office->organization_id))))
          return common_failure("AuthorityResourceMismatch", id);
      }
    }
  }
  for (const auto &row : catalog.institutional_issuers.rows()) {
    const auto &issuer = row.second;
    if (row.first != issuer.id ||
        !s.organizations.find(issuer.organization_id) ||
        issuer.amount_limit < 0 ||
        issuer.continuity_policy_id != "continue_under_organization")
      return common_failure("InvalidInstitutionalIssuer", row.first);
    for (const auto &id : issuer.obligation_ids) {
      const auto *obligation = economy.obligations.find(id);
      if (!obligation || obligation->authority_ref != issuer.id ||
          obligation->principal.kind != SubjectKind::organization ||
          obligation->principal.id != issuer.organization_id ||
          !has(issuer.source_container_ids,
               obligation->source_wallet_or_inventory))
        return common_failure("InvalidInstitutionalObligation", id);
    }
  }
  auto member = members(c, s, catalog, economy, activity, current_tick);
  if (!member.ok)
    return member;
  auto activity_valid =
      activities(c, s, catalog, economy, results, current_tick, complete);
  if (!activity_valid.ok)
    return activity_valid;
  for (const auto &row : s.facts.rows()) {
    const auto &f = row.second;
    const auto *header = c.state().facts.find(row.first);
    if (row.first != f.id || !header || header->domain != "organization" ||
        header->kind != f.kind || !s.organizations.find(f.organization_id) ||
        f.tick < 0 || f.tick > current_tick || f.amount < 0 ||
        (!f.actor_id.empty() && !c.actor(f.actor_id)) ||
        (!f.office_id.empty() && !s.offices.find(f.office_id)) ||
        (!f.activity_id.empty() && !s.activities.find(f.activity_id) &&
         !(catalog.named_membership_enabled &&
           s.membership_offers.find(f.activity_id) &&
           (f.kind == "membership_offered" || f.kind == "membership_decided" ||
            f.kind == "membership_withdrawn" ||
            f.kind == "membership_expired"))))
      return common_failure("InvalidOrganizationFact", row.first);
    if (!(complete &&
          ((f.kind == "organization_public_market_staffing_offered" &&
            f.code == "public_market_staffing_offered") ||
           (f.kind == "organization_activity_cancelled" &&
            f.code == "public_market_staffing_mutual_release") ||
           (f.kind == "organization_member_services_synchronized" &&
            catalog.service_effects_enabled && f.code.empty() &&
            f.amount == 0 && f.office_id.empty() && f.activity_id.empty() &&
            f.cause_fact_ids.empty()))) &&
        !(catalog.authority_lifecycle_enabled &&
          (f.kind == "organization_grant_issued" ||
           f.kind == "organization_grant_amended" ||
           f.kind == "organization_grant_revoked" ||
           f.kind == "organization_office_resigned")) &&
        f.kind != "organization_member_response" &&
        f.kind != "organization_office_changed" &&
        f.kind != "population_training_approved" &&
        f.kind != "population_training_completed" &&
        f.kind != "organization_activity_completed" &&
        f.kind != "organization_activity_failed" &&
        f.kind != "organization_activity_approved" &&
        !(catalog.named_membership_enabled &&
          (f.kind == "membership_offered" || f.kind == "membership_decided" ||
           f.kind == "membership_withdrawn" || f.kind == "membership_expired")))
      return common_failure("UnknownOrganizationFact", row.first);
    for (const auto &id : f.cause_fact_ids)
      if (!c.state().facts.find(id))
        return common_failure("InvalidOrganizationFactCause", id);
  }
  // 完整路径的历史、任期、市场和日历由唯一外层以真实钟与来源接续校验。
  if (complete)
    return common_success();
  auto grants = validate_grant_authority_history(c, s, catalog);
  if (!grants.ok)
    return grants;
  if (catalog.authority_lifecycle_enabled)
    return validate_office_lifecycle(c, s, catalog, authority->stamp,
                                     authority->transitions, authority->deaths);
  for (const auto &row : s.facts.rows())
    if (row.second.office_change_payload || row.second.grant_change_payload)
      return common_failure("UnexpectedOrganizationAuthorityPayload",
                            row.first);
  return common_success();
}
} // namespace hanmo::resident
