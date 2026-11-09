// 所有NPC任免决定走同一预览；本叶不提交、不改人物位置、不占用另一套时钟。
#include "politics_decisions.hpp"
#include "politics_internal.hpp"
#include <set>
namespace hanmo::resident {
using namespace politics_detail;
namespace {
// Actor及组织ID不能用无类型字符串通配扩大范围。
bool contains(const std::vector<StableId> &values, const StableId &id) {
  return std::find(values.begin(), values.end(), id) != values.end();
}
// 上次索引中的活动ID不能跨世界、执行代或候选变更复用。
bool current_index(const CommonStore &c, const PoliticsState &state,
                   const PoliticsDecisionIndex &index) {
  const auto &a = c.state().identity;
  const auto &b = index.identity;
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick &&
         index.appointment_count == state.appointments.size() &&
         index.fact_count == state.facts.size();
}
} // namespace
PoliticsDecisionIndex make_politics_decision_index(const CommonStore &c,
                                                   const PoliticsState &state) {
  PoliticsDecisionIndex result;
  result.identity = c.state().identity;
  result.appointment_count = state.appointments.size();
  result.fact_count = state.facts.size();
  for (const auto &row : state.appointments.rows())
    if (pending(row.second.status))
      result.pending_by_actor[row.second.actor_id].push_back(row.first);
  return result;
}
// 接受范围必须明确；候选数量是计算预算上限，不是人物意愿或政治平衡数值。
Status validate_political_career_policy(const PoliticsState &state,
                                        const PoliticsCatalog &catalog,
                                        const OrganizationState &org,
                                        const PoliticalCareerPolicy &policy) {
  if (policy.id.empty() || policy.maximum_travel_quote_cost < 0 ||
      policy.staffing_posts.size() > 8 ||
      (policy.response != PoliticalOfficeResponse::defer &&
       policy.response != PoliticalOfficeResponse::accept_qualified &&
       policy.response != PoliticalOfficeResponse::decline))
    return common_failure("InvalidPoliticalCareerPolicy", policy.id);
  if (policy.response == PoliticalOfficeResponse::accept_qualified &&
      (policy.acceptable_organization_ids.empty() ||
       policy.acceptable_jurisdiction_ids.empty()))
    return common_failure("PoliticalAcceptanceScopeRequired", policy.id);
  std::set<StableId> ids;
  for (const auto &id : policy.acceptable_organization_ids)
    if (!org.organizations.find(id) || !ids.insert(id).second)
      return common_failure("InvalidPoliticalAcceptanceOrganization", id);
  ids.clear();
  for (const auto &id : policy.acceptable_jurisdiction_ids)
    if (!state.jurisdictions.find(id) || !ids.insert(id).second)
      return common_failure("InvalidPoliticalAcceptanceJurisdiction", id);
  for (const auto &post : policy.staffing_posts) {
    const auto *office = org.offices.find(post.issuing_office_id);
    const auto *j = state.jurisdictions.find(post.jurisdiction_id);
    if (!office || !j ||
        office->organization_id != j->political_organization_id ||
        !catalog.appointment_policies.find(post.appointment_policy_id) ||
        post.candidate_actor_ids.empty() ||
        post.candidate_actor_ids.size() > 16)
      return common_failure("InvalidPoliticalStaffingPost",
                            post.jurisdiction_id);
    ids.clear();
    for (const auto &actor : post.candidate_actor_ids)
      if (actor.empty() || !ids.insert(actor).second)
        return common_failure("InvalidPoliticalStaffingCandidate", actor);
  }
  return common_success();
}
Outcome<PoliticalCareerProposal> propose_political_career(
    const CommonStore &c, const PoliticsState &state,
    const PoliticsCatalog &catalog, const OrganizationState &org,
    const OrganizationCatalog &oc, const LocalEconomyState &economy,
    const SocialState &social, const ActivityRead &activity,
    const PoliticsDecisionIndex &index, const StableId &actor,
    const PoliticalCareerPolicy &policy, Integer budget,
    const PoliticalNavigationRead *navigation,
    const PoliticalOrganizationRuntimeReadContext *runtime) {
  if (!catalog.enabled)
    return {false,
            {},
            common_failure("PoliticsNotEnabled", {},
                           ErrorKind::unsupported_capability)
                .error};
  // 本人提案借同刻读钟和真实资格；不重新构造任职账或以缺源作空成功。
  if (oc.authority_lifecycle_enabled && !runtime)
    return {false,
            {},
            common_failure("PoliticalRuntimeRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  const auto *clock = runtime ? &runtime->clock : nullptr;
  auto now = read_organization_authority_now(c, clock);
  if (!now.ok)
    return {false, {}, now.error};
  if (budget < 1 || budget > 64)
    return {false, {}, common_failure("InvalidPoliticalDecisionBudget").error};
  if (!current_index(c, state, index))
    return {false, {}, common_failure("StalePoliticsDecisionIndex").error};
  auto valid = validate_political_career_policy(state, catalog, org, policy);
  if (!valid.ok)
    return {false, {}, valid.error};
  PoliticalCareerProposal result;
  if (actor == c.state().metadata.player_actor_id) {
    result.reason = "PlayerControlled";
    return {true, result, {}};
  }
  auto active = available(c, activity, actor);
  if (!active.ok) {
    result.reason = active.error.original.code;
    return {true, result, {}};
  }
  const auto proposals = index.pending_by_actor.find(actor);
  if (proposals != index.pending_by_actor.end())
    for (const auto &id : proposals->second) {
      if (result.inspected_candidates >= budget) {
        result.reason = "PoliticalDecisionBudgetReached";
        return {true, result, {}};
      }
      ++result.inspected_candidates;
      const auto *p = state.appointments.find(id);
      if (!p || p->actor_id != actor || !pending(p->status))
        return {
            false, {}, common_failure("StalePoliticsDecisionIndex", id).error};
      auto known = read_political_fact_known(c, state, social, actor,
                                             p->last_fact_id, clock);
      if (!known.ok)
        return {false, {}, known.error};
      if (!known.value)
        continue;
      result.appointment_id = p->id;
      result.source_fact_id = p->last_fact_id;
      if (now.value.half_day_tick >= p->expires_tick) {
        result.reason = "PoliticalAppointmentExpired";
        return {true, result, {}};
      }
      if (p->status == AppointmentStatus::offered) {
        if (policy.response == PoliticalOfficeResponse::defer) {
          result.reason = "PoliticalResponseDeferred";
          return {true, result, {}};
        }
        const bool accept =
            policy.response == PoliticalOfficeResponse::accept_qualified &&
            contains(policy.acceptable_organization_ids, p->organization_id) &&
            contains(policy.acceptable_jurisdiction_ids, p->jurisdiction_id);
        PoliticsCommand command = PoliticalAnswerAppointmentCommand{
            actor, p->id, p->revision, accept, p->last_fact_id};
        auto preview =
            preview_politics_command(c, state, catalog, org, oc, economy,
                                     activity, actor, command, runtime);
        if (preview.ok) {
          result.command = std::move(command);
          result.reason = accept ? "PoliticalPolicyAcceptsOffice"
                                 : "PoliticalPolicyDeclinesOffice";
          return {true, result, {}};
        }
        if (preview.error.kind != ErrorKind::rule)
          return {false, {}, preview.error};
        result.reason = preview.error.original.code;
        return {true, result, {}};
      }
      const auto *j = state.jurisdictions.find(p->jurisdiction_id);
      auto source = appointment_source(c, *p, state, org, oc, clock);
      if (!source.ok) {
        result.reason = source.error.original.code;
        return {true, result, {}};
      }
      const auto location = place(c, actor);
      if (!location || location->place_id != j->seat_place_id) {
        result.reason = "PoliticalArrivalTravelRequired";
        result.travel_required = true;
        result.destination_place_id = j->seat_place_id;
        OrganizationAccess permission(c, org, oc, economy);
        if (!permission.actor_accepts_duty(actor, p->organization_id)) {
          result.reason = "PoliticalCandidateDeclinesDuty";
          return {true, result, {}};
        }
        auto age = political_adult(c, actor, runtime);
        if (!age.ok) {
          result.reason = age.error.original.code;
          return {true, result, {}};
        }
        if (navigation) {
          auto step = plan_political_arrival(
              c, state, catalog, org, oc, economy, social, activity,
              *navigation, actor, p->id, p->revision, p->last_fact_id, runtime);
          if (!step.ok) {
            if (step.error.kind != ErrorKind::rule)
              return {false, {}, step.error};
            result.reason = step.error.original.code;
            return {true, result, {}};
          }
          if (!step.value.command)
            return {false,
                    {},
                    common_failure("PoliticalNavigationLocationMismatch", actor,
                                   ErrorKind::internal)
                        .error};
          if (std::holds_alternative<TravelCommand>(*step.value.command) &&
              !step.value.travel_quote)
            return {false,
                    {},
                    common_failure("PoliticalTravelQuoteRequired", actor,
                                   ErrorKind::internal)
                        .error};
          if (step.value.travel_quote) {
            result.quoted_travel_cost = step.value.travel_quote->cost;
            if (*result.quoted_travel_cost > policy.maximum_travel_quote_cost) {
              result.reason = "PoliticalTravelQuoteLimitExceeded";
              return {true, result, {}};
            }
          }
          result.travel_step = std::move(step.value);
          result.reason = "PoliticalArrivalNavigationReady";
        }
        return {true, result, {}};
      }
      PoliticsCommand command =
          PoliticalArriveCommand{actor, p->id, p->revision, p->last_fact_id};
      auto preview =
          preview_politics_command(c, state, catalog, org, oc, economy,
                                   activity, actor, command, runtime);
      if (preview.ok) {
        result.command = std::move(command);
        result.reason = "PoliticalArrivalReady";
        return {true, result, {}};
      }
      if (preview.error.kind != ErrorKind::rule)
        return {false, {}, preview.error};
      result.reason = preview.error.original.code;
      return {true, result, {}};
    }
  for (const auto &post : policy.staffing_posts) {
    const auto *issuer = org.offices.find(post.issuing_office_id);
    if (issuer->holder_actor_id != std::optional<StableId>(actor))
      continue;
    const auto *j = state.jurisdictions.find(post.jurisdiction_id);
    const auto *office = org.offices.find(j->office_id);
    if (!office)
      continue;
    for (const auto &candidate : post.candidate_actor_ids) {
      if (result.inspected_candidates >= budget) {
        result.reason = "PoliticalDecisionBudgetReached";
        return {true, result, {}};
      }
      ++result.inspected_candidates;
      PoliticsCommand command = PoliticalAppointCommand{
          {issuer->organization_id, issuer->id, issuer->epoch, actor},
          j->id,
          candidate,
          post.appointment_policy_id,
          j->revision,
          office->epoch};
      auto preview =
          preview_politics_command(c, state, catalog, org, oc, economy,
                                   activity, actor, command, runtime);
      if (preview.ok) {
        result.command = std::move(command);
        result.reason = "PoliticalStaffingOpportunity";
        return {true, result, {}};
      }
      if (preview.error.kind != ErrorKind::rule)
        return {false, {}, preview.error};
    }
  }
  result.reason = "NoPoliticalOpportunity";
  return {true, result, {}};
}
} // namespace hanmo::resident
