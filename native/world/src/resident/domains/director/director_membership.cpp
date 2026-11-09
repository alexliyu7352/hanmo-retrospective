#include "director_actor_read.hpp"
// 具名邀请和本人欠薪响应使用可知硬条件，结果仍经与玩家相同的正式命令重验。
#include "director_membership.hpp"
#include "resident/domains/organization/named_membership_rules.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/kernel/command_types.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
// 连分数比较只除法及求余，三个64位因子也不会在交叉相乘时溢出。
bool ratio_at_least(unsigned __int128 a, unsigned __int128 b,
                    unsigned __int128 c, unsigned __int128 d) {
  bool reversed = false;
  for (;;) {
    const auto aq = a / b, cq = c / d;
    if (aq != cq)
      return reversed ? aq < cq : aq > cq;
    a %= b;
    c %= d;
    if (a == 0 || c == 0) {
      if (a == 0 && c == 0)
        return true;
      return reversed ? a == 0 : c == 0;
    }
    std::swap(a, b);
    std::swap(c, d);
    reversed = !reversed;
  }
}
// 接受/拒绝只保留本人已知邀请来源，不把对方公库或未来计划塞进决定。
StableId known_report_id(const CommonStore &common, const SocialState &social,
                         const StableId &actor, const StableId &fact) {
  if (!social.dynamic)
    return {};
  const auto *known = social.dynamic->known_reports.find("0:" + actor);
  if (!known)
    return {};
  for (const auto &row : known->rows()) {
    const auto *report = social.dynamic->reports.find(row.first);
    if (report && report->fact_id == fact &&
        row.second.learned_tick <= common.tick() &&
        report->observation_tick <= common.tick())
      return row.first;
  }
  return {};
}
// 延用原计划身份、生活意图和责任；新的个人决定不会清掉尚未完成的旧计划来源。
Outcome<DecisionProposal> proposal_base(const CommonStore &common,
                                        const DirectorState &state,
                                        const DirectorCatalog &catalog,
                                        const KnownSubjectView &view,
                                        Integer tick, Integer interval) {
  const auto *schedule = director_actor_schedule(state, view.actor_id);
  if (!schedule || view.subject.kind != SubjectKind::actor ||
      view.subject.id != view.actor_id || view.tick != tick ||
      view.revision != common.revision() || tick != common.tick())
    return {false, {}, common_failure("InvalidDirectorInput").error};
  const Integer delay = std::max(
      {interval, catalog.retry_delay_ticks, catalog.review_interval_ticks});
  if (delay < 1 ||
      schedule->decision_seq == std::numeric_limits<Integer>::max() ||
      tick > std::numeric_limits<Integer>::max() - delay)
    return {false, {}, common_failure("DirectorSequenceOverflow").error};
  DecisionProposal proposal;
  proposal.subject_id = view.actor_id;
  auto transition = std::make_shared<DirectorTransition>();
  transition->expected_revision = common.revision();
  if (const auto registration =
          director_actor_registration(state, view.actor_id))
    transition->registration_epoch = registration->epoch;
  transition->decision_seq = schedule->decision_seq + 1;
  transition->next_review_tick = tick + delay;
  transition->template_id = "await_adjustment";
  transition->phase = PlanPhase::waiting;
  auto current_plan = read_director_actor_plan(state, view.actor_id);
  if (!current_plan.ok)
    return {false, {}, current_plan.error};
  if (const auto *plan = current_plan.value) {
    proposal.plan_id = plan->id;
    transition->commitment_ids = plan->commitment_ids;
    transition->livelihood = plan->livelihood;
    transition->pending_life_action_id = plan->pending_life_action_id;
    transition->last_life_fact_id = plan->last_life_fact_id;
  } else
    proposal.plan_id = "plan:" + view.actor_id + ":" +
                       std::to_string(transition->decision_seq);
  proposal.transition = transition;
  proposal.expected_versions.push_back(
      common.read_version({ResourceKey::Kind::actor, view.actor_id}));
  return {true, std::move(proposal), {}};
}
// 同一边界只返回一个候选；创建命令不提前采用、改钱或修改知识。
template <class T>
Outcome<std::vector<DecisionProposal>>
finish(DecisionProposal proposal, const StableId &kind, T command,
       const StableId &goal, const StableId &fact = {},
       const StableId &report = {}) {
  proposal.command = std::make_shared<const CommandPayload>(
      CommandPayload{kind, std::move(command)});
  proposal.reason_fact_id = fact;
  auto &transition = const_cast<DirectorTransition &>(*proposal.transition);
  transition.goal_ref = goal;
  if (!report.empty())
    transition.known_report_ids.push_back(report);
  return {true, {std::move(proposal)}, {}};
}
// 已知欠薪只按同一人物对同一雇主的报告判断，旧组织配置不能冒充当前episode。
bool knows_arrears(const KnownSubjectView &view, const StableId &organization) {
  return std::any_of(
      view.arrears.begin(), view.arrears.end(), [&](const KnownArrear &row) {
        return row.organization_id == organization &&
               row.member_id == view.actor_id && row.outstanding > 0;
      });
}
} // namespace
Outcome<bool>
membership_wage_meets_policy(const MembershipTerms &terms,
                             const ObligationRate &rate,
                             const NamedMembershipPolicy &policy) {
  if (rate.numerator < 0 || rate.denominator < 1 || rate.per_capita ||
      terms.wage_period_ticks < 1 || policy.minimum_wage_numerator < 0 ||
      policy.minimum_wage_denominator < 1 ||
      policy.minimum_wage_period_ticks < 1)
    return {false, false, common_failure("InvalidMembershipWagePolicy").error};
  using Wide = unsigned __int128;
  return {true,
          ratio_at_least(Wide(rate.numerator),
                         Wide(rate.denominator) * Wide(terms.wage_period_ticks),
                         Wide(policy.minimum_wage_numerator),
                         Wide(policy.minimum_wage_denominator) *
                             Wide(policy.minimum_wage_period_ticks)),
          {}};
}
std::optional<KnownArrear> known_membership_paid_source(
    const OrganizationState &organization, const SocialState &social,
    const KnownSubjectView &view, const OrgMemberState &member) {
  if (!social.dynamic || member.actor_id != view.actor_id ||
      (member.response != MemberResponse::payment_requested &&
       member.response != MemberResponse::refusing_new_duties))
    return {};
  std::vector<StableId> obligations;
  for (const auto &id : member.duty_ids)
    if (const auto *duty = organization.duties.find(id))
      obligations.insert(obligations.end(), duty->obligation_ids.begin(),
                         duty->obligation_ids.end());
  std::optional<KnownArrear> paid;
  for (const auto &arrear : view.arrears) {
    const auto *report = social.dynamic->reports.find(arrear.report_id);
    if (arrear.member_id != view.actor_id ||
        arrear.organization_id != member.organization_id || !report ||
        !report->observation.obligation ||
        std::find(obligations.begin(), obligations.end(),
                  report->observation.obligation->obligation_id) ==
            obligations.end())
      continue;
    if (arrear.outstanding > 0)
      return {};
    if (report->payload_kind == "obligation_payment" &&
        report->observation.obligation->paid_amount > 0 &&
        (!paid || paid->observation_tick < arrear.observation_tick))
      paid = arrear;
  }
  return paid;
}
// 本人只按已获知邀请与原责任报价；精确成员路径透传同世界权威读钟，缺源仍由原门拒绝。
Outcome<std::vector<DecisionProposal>> propose_director_membership(
    const CommonStore &common, const DirectorState &state,
    const DirectorCatalog &catalog, const KnownSubjectView &view,
    const SocialState &social, const OrganizationState &organization,
    const OrganizationCatalog &org_catalog, const LocalEconomyState &economy,
    const LocalEconomyCatalog &economy_catalog,
    const InteractionState &interaction, const ActivityRead &activity,
    const MembershipReleaseRead &release, Integer tick,
    const OrganizationAuthorityClock *actual_clock) {
  if (!catalog.named_membership || !org_catalog.named_membership_enabled)
    return {true, {}, {}};
  if (!director_subject_active(state, view.actor_id))
    return {true, {}, {}};
  if (const auto registration =
          director_actor_registration(state, view.actor_id);
      registration && registration->obligations_only)
    return {true, {}, {}};
  const auto *policy = director_policy(state, catalog, view.actor_id);
  const auto *schedule = director_actor_schedule(state, view.actor_id);
  if (!policy || !schedule || schedule->next_tick > tick ||
      schedule->last_decision_tick >= tick || !view.alive || !view.available ||
      !view.current_action_id.empty() || view.interaction_busy)
    return {true, {}, {}};
  // 既有履约已由外层优先处理；这里只作本人有来源的零时长响应。
  for (const auto &row : organization.members.rows()) {
    const auto &member = row.second;
    if (member.actor_id != view.actor_id ||
        member.response == MemberResponse::departed)
      continue;
    const auto *member_policy =
        org_catalog.member_policies.find(member.policy_id);
    if (!member_policy)
      continue;
    for (const auto &arrear : view.arrears) {
      if (arrear.organization_id != member.organization_id ||
          arrear.member_id != view.actor_id ||
          arrear.outstanding < member_policy->minimum_arrears ||
          arrear.due_tick > tick ||
          !membership_fact_known(common, social, view.actor_id, arrear.fact_id))
        continue;
      const auto *report = social.dynamic
                               ? social.dynamic->reports.find(arrear.report_id)
                               : nullptr;
      bool member_source = false;
      if (report && report->observation.obligation)
        for (const auto &duty_id : member.duty_ids)
          if (const auto *duty = organization.duties.find(duty_id))
            if (std::find(duty->obligation_ids.begin(),
                          duty->obligation_ids.end(),
                          report->observation.obligation->obligation_id) !=
                duty->obligation_ids.end())
              member_source = true;
      if (!member_source)
        continue;
      const auto age = tick - arrear.due_tick;
      std::optional<MemberCommandKind> answer;
      if (age >= policy->leave_after_ticks && member_policy->may_leave &&
          member.response != MemberResponse::awaiting_release)
        answer = MemberCommandKind::lawful_leave;
      else if (age >= policy->decline_work_after_ticks &&
               (member.response == MemberResponse::serving ||
                member.response == MemberResponse::payment_requested))
        answer = MemberCommandKind::decline_new_work;
      else if (age >= policy->request_arrear_after_ticks &&
               member.response == MemberResponse::serving)
        answer = MemberCommandKind::request_payment;
      if (!answer)
        continue;
      auto base = proposal_base(common, state, catalog, view, tick, 1);
      if (!base.ok)
        return {false, {}, base.error};
      return finish(std::move(base.value), "organization_member_response",
                    OrganizationMemberCommand{view.actor_id,
                                              member.organization_id,
                                              arrear.fact_id, *answer},
                    member.id, arrear.fact_id, arrear.report_id);
    }
    if (auto paid =
            known_membership_paid_source(organization, social, view, member)) {
      auto base = proposal_base(common, state, catalog, view, tick, 1);
      if (!base.ok)
        return {false, {}, base.error};
      return finish(std::move(base.value), "organization_member_response",
                    OrganizationMemberCommand{
                        view.actor_id, member.organization_id, paid->fact_id,
                        MemberCommandKind::wait_payment},
                    member.id, paid->fact_id, paid->report_id);
    }
  }
  if (policy->named_membership_policy_id) {
    const auto *own = org_catalog.named_membership_policies.find(
        *policy->named_membership_policy_id);
    if (!own)
      return {false,
              {},
              common_failure("UnknownNamedMembershipPolicy",
                             *policy->named_membership_policy_id)
                  .error};
    Integer considered = 0;
    for (const auto &row : organization.membership_offers.rows()) {
      const auto &offer = row.second;
      if (offer.target_actor_id != view.actor_id ||
          offer.status != MembershipOfferStatus::pending ||
          tick < offer.created_tick || tick >= offer.expires_tick)
        continue;
      const auto report =
          known_report_id(common, social, view.actor_id, offer.source_fact_id);
      if (report.empty())
        continue;
      const auto *prior =
          organization.membership_decisions.find(offer.decision_fact_id);
      if (prior && (prior->tick >= tick || prior->next_review_tick > tick))
        continue;
      if (++considered > catalog.candidates_per_subject)
        break;
      const auto *terms = org_catalog.membership_terms.find(offer.terms_id);
      const auto *rate =
          terms ? economy_catalog.rates.find(terms->wage_rate_profile_id)
                : nullptr;
      if (!terms || !rate)
        return {false,
                {},
                common_failure("InvalidMembershipTerms", offer.terms_id).error};
      auto blockers = release.membership_release_view(view.actor_id);
      if (!blockers.ok)
        return {false, {}, blockers.error};
      if (blockers.value.actor_id != view.actor_id ||
          blockers.value.tick != tick ||
          blockers.value.revision != common.revision())
        return {false, {}, common_failure("StaleMembershipReleaseView").error};
      auto wage = membership_wage_meets_policy(*terms, *rate, *own);
      if (!wage.ok)
        return {false, {}, wage.error};
      MembershipDecisionKind answer = MembershipDecisionKind::accept;
      if (!blockers.value.blockers.empty())
        answer = MembershipDecisionKind::defer;
      else if (std::find(own->allowed_duty_kinds.begin(),
                         own->allowed_duty_kinds.end(),
                         terms->duty_kind) == own->allowed_duty_kinds.end() ||
               !wage.value ||
               (own->decline_known_arrears &&
                knows_arrears(view, terms->organization_id)))
        answer = MembershipDecisionKind::decline;
      OrganizationAnswerMembershipCommand command{view.actor_id, offer.id,
                                                  offer.revision, answer,
                                                  offer.source_fact_id};
      auto preview = preview_organization_answer_membership(
          common, organization, org_catalog, economy, economy_catalog, social,
          interaction, activity, release, view.actor_id, command, actual_clock);
      if (!preview.ok) {
        if (preview.error.kind != ErrorKind::rule)
          return {false, {}, preview.error};
        continue;
      }
      auto base = proposal_base(common, state, catalog, view, tick,
                                own->review_interval_ticks);
      if (!base.ok)
        return {false, {}, base.error};
      return finish(std::move(base.value), "organization_answer_membership",
                    command, offer.id, offer.source_fact_id, report);
    }
  }
  // 招聘职事只能访问作者点名候选；同一条款曾答复的历史不会自动重发劝诱。
  Integer considered = 0;
  for (const auto &post : policy->recruitment_posts) {
    const auto *office = organization.offices.find(post.office_id);
    const auto *terms = org_catalog.membership_terms.find(post.terms_id);
    if (!office || !terms ||
        office->holder_actor_id != std::optional<StableId>(view.actor_id) ||
        office->organization_id != terms->organization_id ||
        view.place_id != terms->site_id)
      continue;
    for (const auto &target : post.candidate_actor_ids) {
      if (++considered > catalog.candidates_per_subject)
        return {true, {}, {}};
      bool already = false;
      for (const auto &row : organization.membership_offers.rows())
        if (row.second.issuer_actor_id == view.actor_id &&
            row.second.target_actor_id == target &&
            row.second.terms_id == terms->id) {
          already = true;
          break;
        }
      if (already)
        continue;
      OrganizationOfferMembershipCommand command;
      command.authority = {terms->organization_id, office->id, office->epoch,
                           view.actor_id};
      command.target_actor_id = target;
      command.terms_id = terms->id;
      command.context.place_id = terms->site_id;
      command.context.provider_actor_id = target;
      auto preview = preview_organization_offer_membership(
          common, organization, org_catalog, economy_catalog, social,
          interaction, activity, view.actor_id, command, actual_clock);
      if (!preview.ok) {
        if (preview.error.kind != ErrorKind::rule)
          return {false, {}, preview.error};
        continue;
      }
      auto base = proposal_base(common, state, catalog, view, tick, 1);
      if (!base.ok)
        return {false, {}, base.error};
      return finish(std::move(base.value), "organization_offer_membership",
                    command, target);
    }
  }
  return {true, {}, {}};
}
} // namespace hanmo::resident
