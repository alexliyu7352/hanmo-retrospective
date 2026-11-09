// 有限决策验证约束主体、模板、序号与时间；不访问未知资源或替NPC补做行动。
#include "director_actor_read.hpp"
#include "director_model.hpp"
#include <set>
namespace hanmo::resident {
Status validate_director_actor_plan_fields(
    const CommonStore &c, const DirectorCatalog &catalog,
    const DirectorPolicy &original_policy, const FinitePlan &p,
    const GrowthLifeRead *life) {
  if (p.subject.kind != SubjectKind::actor || p.subject.id.empty() ||
      !c.actor(p.subject.id) || p.id.empty() || p.step_index < 0 ||
      p.next_review_tick < 0 || static_cast<Integer>(p.phase) < 0 ||
      static_cast<Integer>(p.phase) > 7)
    return common_failure("InvalidFinitePlan", p.subject.id);
  // 未安装的 LC01 与提案、采用及来源读口同样明确拒绝，不借旧家庭目录放行。
  if (p.template_id == "life_growth_care_v1")
    return common_failure("NOT_IMPLEMENTED", p.id,
                          ErrorKind::unsupported_capability);
  const std::set<StableId> templates = {
      "maintain_commitment", "replenish_supply", "fulfill_contract",
      "organize_activity", "await_adjustment"};
  if (!templates.count(p.template_id) &&
      !(catalog.regional_life && p.template_id == "private_livelihood") &&
      !(catalog.politics && p.template_id == "political_career") &&
      !(catalog.subordinate_appointments &&
        p.template_id == "political_subordinate_appointment") &&
      !(catalog.diplomatic_response &&
        p.template_id == "diplomatic_response") &&
      !(catalog.diplomatic_delegation &&
        p.template_id == "diplomatic_delegation") &&
      !(catalog.treaty_passage && p.template_id == "treaty_ratification") &&
      !(catalog.military_appointments &&
        p.template_id == "military_appointment") &&
      !(catalog.field_appointments && p.template_id == "field_appointment") &&
      !(catalog.supply_renewals &&
        p.template_id == "political_supply_renewal") &&
      !(catalog.visit_reception && p.template_id == "visit_reception") &&
      !(catalog.shop_registry_intents &&
        p.template_id == "shop_registry_acceptance") &&
      !(catalog.personal_appointment_attendance &&
        p.template_id == "personal_appointment_attendance") &&
      !(catalog.governance_intents &&
        p.template_id == "governance_commission") &&
      !(catalog.governance_agency_intents &&
        p.template_id == "governance_agency") &&
      !(catalog.office_resignation_responses &&
        p.template_id == "office_resignation_response"))
    return common_failure("InvalidDirectorTemplate", p.subject.id);
  if (p.current_action_id) {
    auto *a = c.action(*p.current_action_id);
    if (a && a->actor_id != p.subject.id &&
        std::find(a->participants.begin(), a->participants.end(),
                  p.subject.id) == a->participants.end())
      return common_failure("InvalidPlanAction", p.subject.id);
  }
  std::set<StableId> reports;
  for (const auto &id : p.known_report_ids)
    if (id.empty() || !reports.insert(id).second)
      return common_failure("InvalidPlanReport", p.subject.id);
  if (!catalog.regional_life &&
      (p.livelihood || p.pending_life_action_id || p.last_life_fact_id))
    return common_failure("RegionalLifeCapabilityRequired", p.subject.id);
  if (p.template_id == "diplomatic_delegation") {
    const auto *policy = &original_policy;
    if (!policy || !policy->diplomatic_delegation_policy_id ||
        p.goal_ref != *policy->diplomatic_delegation_policy_id ||
        p.current_action_id)
      return common_failure("InvalidDiplomaticDelegationPlan", p.subject.id);
  }
  if (p.template_id == "diplomatic_response") {
    const auto *policy = &original_policy;
    if (!policy || !policy->diplomatic_response || p.goal_ref.empty() ||
        p.current_action_id)
      return common_failure("InvalidDiplomaticDirectorPlan", p.subject.id);
  }
  if (p.template_id == "private_livelihood" && !p.livelihood)
    return common_failure("MissingLivelihoodIntent", p.subject.id);
  if (p.livelihood) {
    const auto *policy = &original_policy;
    if (!policy || !policy->private_livelihood)
      return common_failure("InvalidLivelihoodIntent", p.subject.id);
    const auto &intent = *p.livelihood;
    const auto &terms = *policy->private_livelihood;
    if (intent.goal_id.empty() || intent.item_id != terms.item_id ||
        intent.target_quantity != terms.target_quantity ||
        intent.return_place_id != terms.return_place_id ||
        intent.selected_tick < 0 || intent.selected_tick > c.tick() ||
        (intent.selected_shop_id.empty() !=
         intent.selected_source_id.empty()) ||
        (!intent.selected_service_role_id.empty() &&
         std::find(terms.known_service_role_ids.begin(),
                   terms.known_service_role_ids.end(),
                   intent.selected_service_role_id) ==
             terms.known_service_role_ids.end()))
      return common_failure("InvalidLivelihoodIntent", p.subject.id);
  }
  for (const auto *id : {&p.pending_life_action_id, &p.last_life_fact_id})
    if (*id) {
      if ((*id)->empty() || !p.livelihood)
        return common_failure("InvalidLifePlanSource", p.subject.id);
      if (life) {
        auto result = life->life_result(**id);
        if (!result || result->actor_id != p.subject.id ||
            result->action_id != **id || result->fact_id != **id ||
            result->fact_id.empty() || !c.state().facts.find(result->fact_id))
          return common_failure("InvalidLifePlanSource", p.subject.id);
        if (result->phase == LifeActivityPhase::active &&
            !c.action(result->action_id))
          return common_failure("InvalidLifePlanSource", p.subject.id);
        if (id == &p.last_life_fact_id &&
            result->phase == LifeActivityPhase::active)
          return common_failure("InvalidLifePlanSource", p.subject.id);
      }
    }

  return common_success();
}
// 共享政策可包含当前成员；只有静态专属政策才把主体本人视作非法候选。
static Status validate_director_impl(const CommonStore &c,
                                     const DirectorState &s,
                                     const DirectorCatalog &catalog,
                                     const GrowthLifeRead *life,
                                     const StableId &initial_player_id,
                                     bool shared_policy_projection) {
  auto registry = validate_director_registry(c, s, catalog);
  if (!registry.ok)
    return registry;
  if (catalog.dynamic_subjects) {
    // 复用原政策和计划不变量，但不把只读验证投影保存成第二份注册或权限。
    DirectorCatalog resolved = catalog;
    resolved.dynamic_subjects = false;
    resolved.exclude_selected_player = false;
    for (const auto &row : s.registrations.rows()) {
      const auto *policy = director_policy(s, catalog, row.first);
      if (!policy)
        return common_failure("UnknownDirectorPolicy", row.first);
      resolved.subjects.insert(row.first, *policy);
    }
    DirectorState legacy = s;
    legacy.dynamic_subjects = false;
    legacy.registrations = {};
    legacy.archived_plans = {};
    for (const auto &row : s.plans.rows()) {
      const auto *registration = s.registrations.find(row.first);
      if (!registration || row.second.policy_id != registration->policy_id)
        return common_failure("DirectorPlanPolicyMismatch", row.first);
    }
    auto active = validate_director_impl(c, legacy, resolved, life,
                                         initial_player_id, true);
    if (!active.ok)
      return active;
    for (const auto &row : s.archived_plans.rows()) {
      const auto &plan = row.second;
      DirectorCatalog history = catalog;
      history.dynamic_subjects = false;
      history.exclude_selected_player = false;
      history.subjects = {};
      history.subjects.insert(plan.subject.id,
                              *catalog.policies.find(plan.policy_id));
      DirectorState archived;
      archived.regional_life = s.regional_life;
      archived.named_membership = s.named_membership;
      archived.schedules.insert(
          plan.subject.id,
          {plan.subject, plan.next_review_tick, 1, 0, -1, {}, {}});
      archived.plans.insert(plan.subject.id, plan);
      // 归档来源是历史引用，允许按原growth保留策略过期；不会作为活计划重放或强行补全。
      auto valid = validate_director_impl(c, archived, history, nullptr,
                                          initial_player_id, true);
      if (!valid.ok)
        return valid;
    }
    return common_success();
  }
  if (s.named_membership != catalog.named_membership ||
      s.regional_life != catalog.regional_life)
    return common_failure("DirectorVersionMismatch");
  // 原founder的缺省自主主体不会随合法继承移动；当前玩家和原founder均不得留下自主复评或计划。
  const auto &player = c.state().metadata.player_actor_id;
  const auto &initial_player =
      initial_player_id.empty() ? player : initial_player_id;
  if (catalog.exclude_selected_player) {
    if (player.empty() || !c.actor(player) || s.schedules.find(player) ||
        s.plans.find(player))
      return common_failure("PlayerDirectorOwnership", player);
    if (initial_player.empty() || !c.actor(initial_player))
      return common_failure("InvalidDirectorInitialPlayer", initial_player);
    if (s.schedules.find(initial_player) || s.plans.find(initial_player))
      return common_failure("PlayerDirectorOwnership", initial_player);
  }
  const auto expected =
      catalog.subjects.size() -
      (catalog.exclude_selected_player && catalog.subjects.find(initial_player)
           ? 1u
           : 0u);
  if (catalog.review_interval_ticks < 1 || catalog.retry_delay_ticks < 1 ||
      catalog.decision_budget < 1 || catalog.decision_budget > 64 ||
      catalog.candidates_per_subject < 1 ||
      catalog.candidates_per_subject > 64 || s.fairness_cursor < 0 ||
      s.last_boundary_tick < -1 || s.last_boundary_tick > c.tick() ||
      s.schedules.size() != expected)
    return common_failure("InvalidDirectorState");
  for (const auto &row : s.schedules.rows()) {
    const auto &r = row.second;
    auto *p = catalog.subjects.find(row.first);
    if (p && !catalog.regional_life && p->private_livelihood)
      return common_failure("RegionalLifeCapabilityRequired", row.first);
    if (!p || !c.actor(row.first) || r.subject.kind != SubjectKind::actor ||
        r.subject.id != row.first || r.next_tick < 0 || r.decision_seq < 0 ||
        r.waiting_since < 0 || r.waiting_since > c.tick() ||
        r.last_decision_tick < -1 || r.last_decision_tick > c.tick() ||
        r.last_decision_tick >= r.next_tick ||
        (r.decision_seq == 0 && r.last_decision_tick != -1))
      return common_failure("InvalidDirectorSchedule", row.first);
    if (p->templates.empty() || p->request_arrear_after_ticks < 0 ||
        p->decline_work_after_ticks < p->request_arrear_after_ticks ||
        p->leave_after_ticks < p->decline_work_after_ticks)
      return common_failure("InvalidDirectorPolicy", row.first);
    std::set<StableId> sources;
    for (const auto &key : p->report_source_keys)
      if (key.empty() || !sources.insert(key).second)
        return common_failure("InvalidDirectorReportRoute", row.first);
    if (sources.size() > 64)
      return common_failure("DirectorReportRouteLimit", row.first);
    std::set<Integer> templates;
    for (auto t : p->templates)
      if (static_cast<Integer>(t) < 0 ||
          (t == DirectorTemplate::office_resignation_response
               ? !catalog.office_resignation_responses
           : t == DirectorTemplate::governance_agency
               ? !catalog.governance_agency_intents
           : t == DirectorTemplate::governance_commission
               ? !catalog.governance_intents
           : t == DirectorTemplate::personal_appointment_attendance
               ? !catalog.personal_appointment_attendance
           : t == DirectorTemplate::shop_registry_acceptance
               ? !catalog.shop_registry_intents
           : t == DirectorTemplate::visit_reception ? !catalog.visit_reception
           : t == DirectorTemplate::political_supply_renewal
               ? !catalog.supply_renewals
           : t == DirectorTemplate::field_appointment
               ? !catalog.field_appointments
           : t == DirectorTemplate::military_appointment
               ? !catalog.military_appointments
               : static_cast<Integer>(t) > (catalog.treaty_passage          ? 9
                                            : catalog.diplomatic_delegation ? 8
                                            : catalog.diplomatic_response   ? 7
                                            : catalog.politics              ? 6
                                            : catalog.regional_life ? 5
                                                                    : 4)) ||
          !templates.insert(static_cast<Integer>(t)).second)
        return common_failure("InvalidDirectorTemplate", row.first);
  }
  if (catalog.regional_life)
    for (const auto &row : catalog.subjects.rows()) {
      const auto &p = row.second;
      const bool active =
          std::find(p.templates.begin(), p.templates.end(),
                    DirectorTemplate::private_livelihood) != p.templates.end();
      if (active != p.private_livelihood.has_value())
        return common_failure("InvalidPrivateLivelihoodPolicy", row.first);
      if (!catalog.named_membership && p.organization_id.empty() &&
          std::any_of(
              p.templates.begin(), p.templates.end(), [](DirectorTemplate t) {
                return t != DirectorTemplate::private_livelihood &&
                       t != DirectorTemplate::maintain_commitment &&
                       t != DirectorTemplate::political_career &&
                       t != DirectorTemplate::visit_reception &&
                       t != DirectorTemplate::personal_appointment_attendance;
              }))
        return common_failure("DirectorOrganizationRequired", row.first);
      if (!p.private_livelihood)
        continue;
      const auto &policy = *p.private_livelihood;
      std::set<StableId> roles;
      for (const auto &id : policy.known_service_role_ids)
        if (id.empty() || !roles.insert(id).second || !c.state().roles.find(id))
          return common_failure("InvalidKnownService", row.first);
      if (!c.item_definition(policy.item_id) || policy.target_quantity < 1 ||
          policy.target_quantity > 500 || policy.cash_reserve < 0 ||
          policy.cash_reserve > 2147483647 || policy.work_fatigue_limit < 1 ||
          policy.work_fatigue_limit > 100 || roles.empty() ||
          roles.size() > 16 || policy.return_place_id.empty())
        return common_failure("InvalidPrivateLivelihoodPolicy", row.first);
    }
  for (const auto &row : catalog.subjects.rows()) {
    const auto &policy = row.second;
    const bool resignation =
        std::find(policy.templates.begin(), policy.templates.end(),
                  DirectorTemplate::office_resignation_response) !=
        policy.templates.end();
    if (resignation !=
            policy.office_resignation_response_intent_id.has_value() ||
        (resignation && !catalog.office_resignation_responses))
      return common_failure("OfficeResignationResponseDirectorPolicyMismatch",
                            row.first);
    const bool agency =
        std::find(policy.templates.begin(), policy.templates.end(),
                  DirectorTemplate::governance_agency) !=
        policy.templates.end();
    if (agency != policy.governance_agency_intent_id.has_value() ||
        (agency && !catalog.governance_agency_intents))
      return common_failure("GovernanceAgencyIntentDirectorPolicyMismatch",
                            row.first);
    const bool delegation =
        std::find(policy.templates.begin(), policy.templates.end(),
                  DirectorTemplate::diplomatic_delegation) !=
        policy.templates.end();
    if (delegation != policy.diplomatic_delegation_policy_id.has_value() ||
        (delegation && !catalog.diplomatic_delegation))
      return common_failure("InvalidDiplomaticDelegationDirectorPolicy",
                            row.first);
    const bool response =
        std::find(policy.templates.begin(), policy.templates.end(),
                  DirectorTemplate::diplomatic_response) !=
        policy.templates.end();
    if (response != policy.diplomatic_response.has_value() ||
        (response && !catalog.diplomatic_response))
      return common_failure("InvalidDiplomaticDirectorPolicy", row.first);
    const bool political =
        std::find(policy.templates.begin(), policy.templates.end(),
                  DirectorTemplate::political_career) != policy.templates.end();
    if (political != policy.political_career.has_value() ||
        (political && !catalog.politics))
      return common_failure("InvalidPoliticalDirectorPolicy", row.first);
    if (!catalog.named_membership && (policy.named_membership_policy_id ||
                                      !policy.recruitment_posts.empty()))
      return common_failure("NamedMembershipCapabilityRequired", row.first);
    if (policy.named_membership_policy_id &&
        policy.named_membership_policy_id->empty())
      return common_failure("InvalidNamedMembershipPolicy", row.first);
    if (policy.recruitment_posts.size() > 8)
      return common_failure("RecruitmentPostLimit", row.first);
    std::set<std::pair<StableId, StableId>> posts;
    for (const auto &post : policy.recruitment_posts) {
      if (post.office_id.empty() || post.terms_id.empty() ||
          post.candidate_actor_ids.empty() ||
          post.candidate_actor_ids.size() > 16 ||
          !posts.emplace(post.office_id, post.terms_id).second)
        return common_failure("InvalidRecruitmentPost", row.first);
      std::set<StableId> actors;
      for (const auto &id : post.candidate_actor_ids)
        if ((!shared_policy_projection && id == row.first) || !c.actor(id) ||
            !actors.insert(id).second)
          return common_failure("InvalidRecruitmentCandidate", row.first);
    }
  }
  std::set<StableId> ids;
  for (const auto &row : s.plans.rows()) {
    const auto &p = row.second;
    const auto *r = s.schedules.find(row.first);
    const auto *policy = catalog.subjects.find(row.first);
    if (!r || !policy || p.subject.id != row.first ||
        !ids.insert(p.id).second || p.next_review_tick != r->next_tick)
      return common_failure("InvalidFinitePlan", row.first);
    auto valid =
        validate_director_actor_plan_fields(c, catalog, *policy, p, life);
    if (!valid.ok)
      return valid;
  }
  return common_success();
}
// 公开验证保原合同；共享展开仅用于只读验证，不授予招聘者身份或执行权。
Status validate_director(const CommonStore &c, const DirectorState &s,
                         const DirectorCatalog &catalog,
                         const GrowthLifeRead *life,
                         const StableId &initial_player_id) {
  return validate_director_impl(c, s, catalog, life, initial_player_id, false);
}
} // namespace hanmo::resident
