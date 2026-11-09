// 辖区任命、本人答复、真实到任与撤权均在同候选原子办理；不消耗或复制公私资产。
#include "politics_internal.hpp"
#include "politics_organization_runtime.hpp"
#include "politics_time2.hpp"
#include "resident/domains/organization/political_office_transition.hpp"
namespace hanmo::resident {
using namespace politics_detail;
namespace {
// 上级的辖区操作同时检查事实控制、当前读取版本和具名Grant。
Status jurisdiction_authority(const CommonStore &c, const PoliticsState &s,
                              const OrganizationState &o,
                              const OrganizationCatalog &oc,
                              const ActingAuthority &authority,
                              const StableId &id, Integer revision,
                              Integer epoch, const StableId &kind,
                              const OrganizationAuthorityClock *clock) {
  const auto *j = s.jurisdictions.find(id);
  if (!j)
    return common_failure("UnknownPoliticalJurisdiction", id);
  if (j->revision != revision)
    return common_failure("StalePoliticalJurisdiction", id);
  if (j->political_organization_id != authority.organization_id ||
      j->controlling_organization_id != authority.organization_id)
    return common_failure("PoliticalControlDenied", id);
  const auto *office = o.offices.find(j->office_id);
  if (!office || office->epoch != epoch)
    return common_failure("StalePoliticalOffice", j->office_id);
  return political_authority(c, o, oc, authority, kind,
                             {ScopeRef::Kind::site, j->seat_place_id}, clock);
}
// 本人决定只可消费自己的当前任命及已交付的前序事实。
Status answer_source(const CommonStore &c, const PoliticsState &s,
                     const OrganizationState &o, const OrganizationCatalog &oc,
                     const LocalEconomyState &economy,
                     const ActivityRead &activity, const StableId &actor,
                     const StableId &id, Integer revision,
                     const StableId &source, AppointmentStatus expected,
                     bool needs_duty, const OrganizationAuthorityClock *clock) {
  auto now = read_organization_authority_now(c, clock);
  if (!now.ok)
    return {false, {}, now.error};
  const auto *p = s.appointments.find(id);
  if (!p || p->actor_id != actor)
    return common_failure("UnknownPersonalAppointment", id);
  if (p->revision != revision || p->status != expected)
    return common_failure("StalePoliticalAppointment", id);
  if (source != p->last_fact_id)
    return common_failure("UnknownAppointmentSource", source);
  if (now.value.half_day_tick >= p->expires_tick)
    return common_failure("PoliticalAppointmentExpired", id);
  auto active = available(c, activity, actor);
  if (!active.ok)
    return active;
  auto current = appointment_source(c, *p, s, o, oc, clock);
  if (!current.ok)
    return current;
  OrganizationAccess access(c, o, oc, economy);
  if (needs_duty && !access.actor_accepts_duty(actor, p->organization_id))
    return common_failure("PoliticalCandidateDeclinesDuty", actor);
  return common_success();
}
} // namespace
// 预览只核验来源，不分配ID、消耗RNG或更改任何任职。
Status preview_politics_command(
    const CommonStore &c, const PoliticsState &s,
    const PoliticsCatalog &catalog, const OrganizationState &o,
    const OrganizationCatalog &oc, const LocalEconomyState &economy,
    const ActivityRead &activity, const StableId &actor,
    const PoliticsCommand &command,
    const PoliticalOrganizationRuntimeReadContext *runtime) {
  if (!catalog.enabled)
    return common_failure("PoliticsNotEnabled", {},
                          ErrorKind::unsupported_capability);
  if (oc.authority_lifecycle_enabled && !runtime)
    return common_failure("PoliticalRuntimeRequired", {},
                          ErrorKind::unsupported_capability);
  const auto *clock = runtime ? &runtime->clock : nullptr;
  auto now = read_organization_authority_now(c, clock);
  if (!now.ok)
    return {false, {}, now.error};
  const auto actual_tick = now.value.half_day_tick;
  return std::visit(
      [&](const auto &cmd) -> Status {
        using T = std::decay_t<decltype(cmd)>;
        if constexpr (std::is_same_v<T, PoliticalAppointCommand>) {
          if (cmd.authority.actor_id != actor)
            return common_failure("PoliticalCommandActorMismatch");
          auto active = available(c, activity, actor);
          if (!active.ok)
            return active;
          auto authority = jurisdiction_authority(
              c, s, o, oc, cmd.authority, cmd.jurisdiction_id,
              cmd.expected_jurisdiction_revision, cmd.expected_office_epoch,
              "political_appoint", clock);
          if (!authority.ok)
            return authority;
          const auto *j = s.jurisdictions.find(cmd.jurisdiction_id);
          const auto *office = o.offices.find(j->office_id);
          if (office->holder_actor_id || office->status == OfficeStatus::active)
            return common_failure("PoliticalOfficeNotVacant", office->id);
          auto delegation = delegation_allowed_at(
              c, o, oc, cmd.authority.office_id, j->office_id, clock);
          if (!delegation.ok)
            return {false, {}, delegation.error};
          if (!delegation.value)
            return common_failure("PoliticalDelegationExceedsAuthority",
                                  j->office_id);
          const auto *policy = catalog.appointment_policies.find(cmd.policy_id);
          if (!policy || policy->response_ticks <= 0 ||
              policy->arrival_ticks <= 0 ||
              policy->minimum_administration <= 0 || policy->minimum_merit < 0)
            return common_failure("InvalidAppointmentPolicy", cmd.policy_id);
          if (actual_tick > INT64_MAX - policy->response_ticks)
            return common_failure("PoliticalDeadlineOverflow");
          auto candidate = available(c, activity, cmd.target_actor_id);
          if (!candidate.ok)
            return candidate;
          auto age = political_adult(c, cmd.target_actor_id, runtime);
          if (!age.ok)
            return age;
          const auto *person = c.state().actors.find(cmd.target_actor_id);
          if (person->attributes.administration <
                  policy->minimum_administration ||
              person->merit < policy->minimum_merit)
            return common_failure("PoliticalCandidateUnqualified",
                                  cmd.target_actor_id);
          OrganizationAccess access(c, o, oc, economy);
          if (!access.actor_accepts_duty(cmd.target_actor_id,
                                         cmd.authority.organization_id))
            return common_failure("PoliticalCandidateDeclinesDuty",
                                  cmd.target_actor_id);
          const auto here = place(c, actor),
                     there = place(c, cmd.target_actor_id);
          if (!here || !there || here->place_id != there->place_id ||
              here->settlement_id != there->settlement_id)
            return common_failure("PoliticalContactNotDelivered",
                                  cmd.target_actor_id);
          for (const auto &row : s.appointments.rows())
            if (pending(row.second.status) &&
                row.second.jurisdiction_id == j->id)
              return common_failure("PoliticalAppointmentPending", j->id);
          return common_success();
        } else if constexpr (std::is_same_v<
                                 T, PoliticalAnswerAppointmentCommand> ||
                             std::is_same_v<T, PoliticalArriveCommand>) {
          if (cmd.actor_id != actor)
            return common_failure("PoliticalCommandActorMismatch");
          constexpr bool arriving = std::is_same_v<T, PoliticalArriveCommand>;
          bool needs_duty = true;
          if constexpr (std::is_same_v<T, PoliticalAnswerAppointmentCommand>)
            needs_duty = cmd.accept;
          auto source = answer_source(c, s, o, oc, economy, activity, actor,
                                      cmd.appointment_id, cmd.expected_revision,
                                      cmd.source_fact_id,
                                      arriving ? AppointmentStatus::accepted
                                               : AppointmentStatus::offered,
                                      needs_duty, clock);
          if (!source.ok)
            return source;
          const auto *p = s.appointments.find(cmd.appointment_id);
          const auto *required =
              catalog.appointment_policies.find(p->policy_id);
          const auto *person = c.state().actors.find(actor);
          if (needs_duty) {
            auto age = political_adult(c, actor, runtime);
            if (!age.ok)
              return age;
          }
          if (needs_duty && (!required ||
                             person->attributes.administration <
                                 required->minimum_administration ||
                             person->merit < required->minimum_merit))
            return common_failure("PoliticalCandidateUnqualified", actor);
          if (p->revision == INT64_MAX)
            return common_failure("PoliticalRevisionOverflow", p->id);
          if constexpr (arriving) {
            const auto *j = s.jurisdictions.find(p->jurisdiction_id);
            const auto location = place(c, actor);
            if (!location || location->place_id != j->seat_place_id)
              return common_failure("PoliticalCandidateNotAtSeat",
                                    j->seat_place_id);
            if (p->target_office_epoch == INT64_MAX)
              return common_failure("AuthorityEpochOverflow", j->office_id);
          } else {
            const auto *policy =
                catalog.appointment_policies.find(p->policy_id);
            if (!policy || actual_tick > INT64_MAX - policy->arrival_ticks)
              return common_failure("PoliticalDeadlineOverflow");
          }
          return common_success();
        } else if constexpr (std::is_same_v<T, PoliticalWithdrawCommand>) {
          if (cmd.authority.actor_id != actor)
            return common_failure("PoliticalCommandActorMismatch");
          const auto *p = s.appointments.find(cmd.appointment_id);
          if (!p || !pending(p->status) || p->revision != cmd.expected_revision)
            return common_failure("StalePoliticalAppointment",
                                  cmd.appointment_id);
          if (p->revision == INT64_MAX)
            return common_failure("PoliticalRevisionOverflow", p->id);
          return jurisdiction_authority(
              c, s, o, oc, cmd.authority, p->jurisdiction_id,
              p->jurisdiction_revision, p->target_office_epoch,
              "political_appoint", clock);
        } else {
          if (cmd.authority.actor_id != actor)
            return common_failure("PoliticalCommandActorMismatch");
          auto authority = jurisdiction_authority(
              c, s, o, oc, cmd.authority, cmd.jurisdiction_id,
              cmd.expected_jurisdiction_revision, cmd.expected_office_epoch,
              "political_dismiss", clock);
          if (!authority.ok)
            return authority;
          const auto *office = o.offices.find(
              s.jurisdictions.find(cmd.jurisdiction_id)->office_id);
          if (!office->holder_actor_id ||
              office->status != OfficeStatus::active)
            return common_failure("PoliticalOfficeAlreadyVacant", office->id);
          if (office->deputy_actor_id)
            return common_failure("PoliticalDeputyTransferRequired", office->id,
                                  ErrorKind::unsupported_capability);
          if (office->epoch == INT64_MAX)
            return common_failure("AuthorityEpochOverflow", office->id);
          return common_success();
        }
      },
      command);
}
// 公职替换只有政治业务已通过权限、本人同意、真实到场后才调用；通用Grant仍绑定同一Office。
Outcome<PoliticsEffects> apply_politics_command_in_transaction(
    CommonStore &c, PoliticsState &state, const PoliticsCatalog &catalog,
    OrganizationState &organization, const OrganizationCatalog &oc,
    const LocalEconomyState &economy, const ActivityRead &activity,
    const StableId &actor, const PoliticsCommand &command,
    const PoliticalOrganizationRuntimeWriteContext *runtime) {
  std::optional<WorldMoment> actual_moment;
  if (runtime) {
    auto now = read_organization_authority_now(c, &runtime->clock);
    if (!now.ok)
      return {false, {}, now.error};
    actual_moment = std::move(now.value);
  } else if (oc.authority_lifecycle_enabled &&
             (std::holds_alternative<PoliticalArriveCommand>(command) ||
              std::holds_alternative<PoliticalDismissCommand>(command)))
    return {false,
            {},
            common_failure("PoliticalOfficeTransitionWriterRequired").error};
  // 写候选重验沿同一原来源的只读视图，不把预览世界的引用带到提交。
  std::optional<PoliticalOrganizationRuntimeReadContext> read_runtime;
  if (runtime)
    read_runtime.emplace(runtime->read());
  auto valid = preview_politics_command(
      c, state, catalog, organization, oc, economy, activity, actor, command,
      read_runtime ? &*read_runtime : nullptr);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto s = state;
  auto o = organization;
  PoliticsEffects effects;
  std::optional<SingleGrantAuthorityRequest> authority_request;
  std::optional<SingleGrantAuthorityRead> authority_proof;
  if (runtime) {
    auto captured = std::visit(
        [&](const auto &cmd) -> Status {
          using T = std::decay_t<decltype(cmd)>;
          if constexpr (std::is_same_v<T, PoliticalAppointCommand> ||
                        std::is_same_v<T, PoliticalWithdrawCommand> ||
                        std::is_same_v<T, PoliticalDismissCommand>) {
            StableId jurisdiction;
            if constexpr (std::is_same_v<T, PoliticalWithdrawCommand>)
              jurisdiction =
                  state.appointments.find(cmd.appointment_id)->jurisdiction_id;
            else
              jurisdiction = cmd.jurisdiction_id;
            const auto *j = state.jurisdictions.find(jurisdiction);
            const StableId kind = std::is_same_v<T, PoliticalDismissCommand>
                                      ? "political_dismiss"
                                      : "political_appoint";
            authority_request = SingleGrantAuthorityRequest{
                cmd.authority,
                kind,
                {ScopeRef::Kind::site, j->seat_place_id},
                0,
                {},
                false,
                {}};
            auto proof = check_current_grant_authority(
                c, organization, oc, oc.authority_catalog_identity,
                *authority_request, &runtime->clock);
            if (!proof.ok)
              return {false, {}, proof.error};
            authority_proof = std::move(proof.value);
          }
          return common_success();
        },
        command);
    if (!captured.ok)
      return {false, {}, captured.error};
  }
  auto record_authority = [&](const StableId &id) -> Status {
    if (!runtime || !authority_request)
      return common_success();
    NativePoliticsAuthorityRead reader(c, s, runtime->clock);
    const auto *fact = s.facts.find(id);
    if (!fact || !authority_proof)
      return common_failure("PoliticalAuthorityAppendRequired", id);
    auto registered = register_authority_basis_for_fact(
        c, runtime->runtime.authority_bases, o, oc,
        oc.authority_catalog_identity, *authority_request, *authority_proof,
        {"politics", fact->kind, id}, reader);
    return registered.ok ? common_success()
                         : Status{false, {}, registered.error};
  };
  auto result = std::visit(
      [&](const auto &cmd) -> Status {
        using T = std::decay_t<decltype(cmd)>;
        if constexpr (std::is_same_v<T, PoliticalAppointCommand>) {
          auto id = c.next_id("appointment");
          if (!id.ok)
            return {false, {}, id.error};
          PoliticalAppointment p;
          p.id = id.value;
          p.jurisdiction_id = cmd.jurisdiction_id;
          p.policy_id = cmd.policy_id;
          p.actor_id = cmd.target_actor_id;
          p.issuer_actor_id = actor;
          p.issuer_office_id = cmd.authority.office_id;
          p.organization_id = cmd.authority.organization_id;
          p.issuer_epoch = cmd.authority.authority_epoch;
          p.target_office_epoch = cmd.expected_office_epoch;
          p.jurisdiction_revision = cmd.expected_jurisdiction_revision;
          p.created_tick =
              actual_moment ? actual_moment->half_day_tick : c.tick();
          p.expires_tick =
              p.created_tick +
              catalog.appointment_policies.find(cmd.policy_id)->response_ticks;
          auto fact = appointment_fact(c, s, p, "political_appointment_offered",
                                       AppointmentStatus::offered, actor,
                                       actual_moment);
          if (!fact.ok)
            return {false, {}, fact.error};
          p.offered_fact_id = fact.value;
          auto registered = record_authority(fact.value);
          if (!registered.ok)
            return registered;
          effects.appointment_id = p.id;
          effects.fact_ids.push_back(fact.value);
          s.appointments.insert(id.value, std::move(p));
          return common_success();
        } else if constexpr (std::is_same_v<
                                 T, PoliticalAnswerAppointmentCommand> ||
                             std::is_same_v<T, PoliticalArriveCommand> ||
                             std::is_same_v<T, PoliticalWithdrawCommand>) {
          auto *p = s.appointments.find(cmd.appointment_id);
          AppointmentStatus status;
          StableId kind;
          if constexpr (std::is_same_v<T, PoliticalAnswerAppointmentCommand>) {
            status = cmd.accept ? AppointmentStatus::accepted
                                : AppointmentStatus::declined;
            kind = cmd.accept ? "political_appointment_accepted"
                              : "political_appointment_declined";
            if (cmd.accept)
              p->expires_tick =
                  (actual_moment ? actual_moment->half_day_tick : c.tick()) +
                  catalog.appointment_policies.find(p->policy_id)
                      ->arrival_ticks;
          } else if constexpr (std::is_same_v<T, PoliticalArriveCommand>) {
            status = AppointmentStatus::arrived;
            kind = "political_appointment_arrived";
          } else {
            status = AppointmentStatus::withdrawn;
            kind = "political_appointment_withdrawn";
          }
          auto fact =
              appointment_fact(c, s, *p, kind, status, actor, actual_moment);
          if (!fact.ok)
            return {false, {}, fact.error};
          auto registered = record_authority(fact.value);
          if (!registered.ok)
            return registered;
          effects.appointment_id = p->id;
          effects.fact_ids.push_back(fact.value);
          if constexpr (std::is_same_v<T, PoliticalArriveCommand>) {
            if (runtime) {
              auto changed = apply_political_office_transition_after_fact(
                  c, s, o, oc, runtime->runtime.office_transition_sources,
                  fact.value, runtime->clock, runtime->read_stamp);
              if (!changed.ok)
                return {false, {}, changed.error};
              if (!changed.value.changed || !changed.value.fact_id)
                return common_failure("PoliticalOfficeTransitionNotApplied",
                                      fact.value);
              effects.fact_ids.push_back(*changed.value.fact_id);
              return common_success();
            }
            auto *office = o.offices.find(
                s.jurisdictions.find(p->jurisdiction_id)->office_id);
            office->holder_actor_id = actor;
            office->status = OfficeStatus::active;
            ++office->epoch;
            auto id = c.next_id("organization");
            if (!id.ok)
              return {false, {}, id.error};
            auto appended = c.append_fact(
                {id.value, "organization_office_changed", "organization"});
            if (!appended.ok)
              return appended;
            OrganizationFact f;
            f.id = id.value;
            f.kind = "organization_office_changed";
            f.organization_id = p->organization_id;
            f.actor_id = actor;
            f.office_id = office->id;
            f.tick = c.tick();
            f.cause_fact_ids = {fact.value};
            o.facts.insert(id.value, std::move(f));
            effects.fact_ids.push_back(id.value);
          }
          return common_success();
        } else {
          const auto *j = s.jurisdictions.find(cmd.jurisdiction_id);
          const auto *office = o.offices.find(j->office_id);
          PoliticsFact f;
          f.kind = "political_office_dismissed";
          f.jurisdiction_id = j->id;
          f.actor_id = *office->holder_actor_id;
          f.issuer_actor_id = actor;
          f.organization_id = j->political_organization_id;
          f.office_id = j->office_id;
          f.office_epoch = office->epoch;
          f.tick = actual_moment ? actual_moment->half_day_tick : c.tick();
          f.occurred_at = actual_moment;
          f.decision_actor_id = actor;
          f.witness_actor_ids = {actor};
          const auto here = place(c, actor), there = place(c, f.actor_id);
          if (f.actor_id != actor && here && there &&
              here->place_id == there->place_id &&
              here->settlement_id == there->settlement_id)
            f.witness_actor_ids.push_back(f.actor_id);
          f.status = AppointmentStatus::invalidated;
          auto id = append_fact(c, s, std::move(f));
          if (!id.ok)
            return {false, {}, id.error};
          auto registered = record_authority(id.value);
          if (!registered.ok)
            return registered;
          effects.fact_ids.push_back(id.value);
          if (runtime) {
            auto changed = apply_political_office_transition_after_fact(
                c, s, o, oc, runtime->runtime.office_transition_sources,
                id.value, runtime->clock, runtime->read_stamp);
            if (!changed.ok)
              return {false, {}, changed.error};
            if (!changed.value.changed || !changed.value.fact_id)
              return common_failure("PoliticalOfficeTransitionNotApplied",
                                    id.value);
            effects.fact_ids.push_back(*changed.value.fact_id);
            return common_success();
          }
          const auto before = o.facts.size();
          auto dismissed =
              invalidate_office(c, o, oc, j->office_id, c.tick(), id.value);
          if (!dismissed.ok)
            return dismissed;
          for (std::size_t n = before; n < o.facts.size(); ++n)
            effects.fact_ids.push_back(o.facts.rows()[n].first);
          return common_success();
        }
      },
      command);
  if (!result.ok)
    return {false, {}, result.error};
  state = std::move(s);
  organization = std::move(o);
  effects.receipt_ids = effects.fact_ids;
  return {true, std::move(effects), {}};
}
// 独立领域调用持自己的savepoint；中央候选只调用明确的in_transaction入口。
Outcome<PoliticsEffects> apply_politics_command(
    CommonStore &c, PoliticsState &state, const PoliticsCatalog &catalog,
    OrganizationState &organization, const OrganizationCatalog &oc,
    const LocalEconomyState &economy, const ActivityRead &activity,
    const StableId &actor, const PoliticsCommand &command) {
  local_economy_detail::PhaseSavepoint guard(c);
  if (!guard.token().ok)
    return {false, {}, guard.token().error};
  auto next = state;
  auto offices = organization;
  auto result = apply_politics_command_in_transaction(
      c, next, catalog, offices, oc, economy, activity, actor, command);
  if (!result.ok)
    return result;
  auto committed = guard.commit();
  if (!committed.ok)
    return {false, {}, committed.error};
  state = std::move(next);
  organization = std::move(offices);
  return result;
}
// 稳定边界只关闭已过期或真实来源失效的未决任命，不替本人接受、旅行或就任。
Outcome<PoliticsEffects>
reconcile_politics(CommonStore &c, PoliticsState &state,
                   const PoliticsCatalog &catalog, const OrganizationState &o,
                   const OrganizationCatalog &oc,
                   const LocalEconomyState &economy,
                   const PoliticalOrganizationRuntimeWriteContext *runtime) {
  if (!catalog.enabled)
    return {true, {}, {}};
  if (oc.authority_lifecycle_enabled && !runtime)
    return {false,
            {},
            common_failure("PoliticalRuntimeRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto now =
      read_organization_authority_now(c, runtime ? &runtime->clock : nullptr);
  if (!now.ok)
    return {false, {}, now.error};
  const std::optional<WorldMoment> actual_moment =
      runtime ? std::optional<WorldMoment>{now.value} : std::nullopt;
  local_economy_detail::PhaseSavepoint guard(c);
  if (!guard.token().ok)
    return {false, {}, guard.token().error};
  auto s = state;
  PoliticsEffects effects;
  OrganizationAccess access(c, o, oc, economy);
  for (auto &row : s.appointments.rows()) {
    auto &p = row.second;
    if (!pending(p.status))
      continue;
    const auto *person = c.actor(p.actor_id);
    auto source =
        appointment_source(c, p, s, o, oc, runtime ? &runtime->clock : nullptr);
    if (!source.ok && source.error.kind != ErrorKind::rule)
      return {false, {}, source.error};
    const bool stale =
        !person || !person->alive || !person->available ||
        !access.actor_accepts_duty(p.actor_id, p.organization_id) || !source.ok;
    const bool expired = now.value.half_day_tick >= p.expires_tick;
    if (!stale && !expired)
      continue;
    if (p.revision == INT64_MAX)
      return {
          false, {}, common_failure("PoliticalRevisionOverflow", p.id).error};
    auto fact = appointment_fact(c, s, p,
                                 expired ? "political_appointment_expired"
                                         : "political_appointment_invalidated",
                                 expired ? AppointmentStatus::expired
                                         : AppointmentStatus::invalidated,
                                 {}, actual_moment);
    if (!fact.ok)
      return {false, {}, fact.error};
    effects.fact_ids.push_back(fact.value);
  }
  auto committed = guard.commit();
  if (!committed.ok)
    return {false, {}, committed.error};
  state = std::move(s);
  effects.receipt_ids = effects.fact_ids;
  return {true, std::move(effects), {}};
}
} // namespace hanmo::resident
