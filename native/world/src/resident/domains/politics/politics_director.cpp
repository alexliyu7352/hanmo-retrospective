// 本人的受任/赴任优先使用真实已送达来源；命令和旅行仍经原中央preview/apply。
#include "politics_director.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
Outcome<std::optional<DecisionProposal>> propose_director_political_career(
    const CommonStore &c, const DirectorCatalog &director,
    const KnownSubjectView &view, const OrganizationState &org,
    const OrganizationCatalog &oc, const DirectorPoliticalContext &ctx,
    const PoliticalCareerPolicy &policy, const DecisionProposal &base) {
  if (!base.transition || base.subject_id != view.actor_id ||
      base.transition->expected_revision != c.revision() ||
      view.tick != c.tick())
    return {
        false, {}, common_failure("InvalidPoliticalDirectorProposal").error};
  const auto index = make_politics_decision_index(c, ctx.politics);
  auto candidate = propose_political_career(
      c, ctx.politics, ctx.catalog, org, oc, ctx.economy, ctx.social,
      ctx.activity, index, view.actor_id, policy,
      director.candidates_per_subject, &ctx.navigation, ctx.runtime);
  if (!candidate.ok)
    return {false, {}, candidate.error};
  const auto &choice = candidate.value;
  if (!choice.command && !choice.travel_step && choice.appointment_id.empty())
    return {true, std::nullopt, {}};
  DecisionProposal result = base;
  result.transition = std::make_shared<DirectorTransition>(*base.transition);
  auto &next = const_cast<DirectorTransition &>(*result.transition);
  next.template_id = "political_career";
  next.goal_ref = choice.appointment_id;
  next.current_action_id.reset();
  next.next_step_index = 0;
  next.phase = PlanPhase::waiting;
  next.wait_reason.clear();
  result.reason_fact_id = choice.source_fact_id;
  if (choice.command) {
    result.command = std::make_shared<const CommandPayload>(CommandPayload{
        politics_command_kind(*choice.command), *choice.command});
    next.phase = PlanPhase::accepting;
    if (const auto *appoint =
            std::get_if<PoliticalAppointCommand>(&*choice.command))
      next.goal_ref = appoint->jurisdiction_id;
    if (std::holds_alternative<PoliticalArriveCommand>(*choice.command))
      next.phase = PlanPhase::completed;
    if (const auto *answer =
            std::get_if<PoliticalAnswerAppointmentCommand>(&*choice.command))
      if (!answer->accept)
        next.phase = PlanPhase::completed;
  } else if (choice.travel_step && choice.travel_step->command) {
    std::visit(
        [&](const auto &command) {
          using T = std::decay_t<decltype(command)>;
          result.command =
              std::make_shared<const CommandPayload>(CommandPayload{
                  std::is_same_v<T, MoveLocalCommand> ? "move_local" : "travel",
                  TravelCommandPayload{command}});
        },
        *choice.travel_step->command);
    next.phase = PlanPhase::sourcing;
  } else {
    next.wait_reason =
        choice.reason.empty() ? "PoliticalDecisionWaiting" : choice.reason;
  }
  return {true, std::move(result), {}};
}
// 未启用politics的旧世界拒新增模板/策略；政策与真实辖区、公职候选引用逐项闭合。
Status validate_political_director(const CommonStore &c,
                                   const PoliticsState &state,
                                   const PoliticsCatalog &catalog,
                                   const OrganizationState &org,
                                   const DirectorState &director,
                                   const DirectorCatalog &rules) {
  if (rules.politics != catalog.enabled)
    return common_failure("PoliticalDirectorProfileMismatch");
  const auto check = [&](const auto &table) -> Status {
    for (const auto &row : table.rows()) {
      const auto &p = row.second;
      const bool enabled =
          std::find(p.templates.begin(), p.templates.end(),
                    DirectorTemplate::political_career) != p.templates.end();
      if (enabled != p.political_career.has_value() ||
          (enabled && !catalog.enabled))
        return common_failure("InvalidPoliticalDirectorPolicy", row.first);
      if (!p.political_career)
        continue;
      auto valid = validate_political_career_policy(state, catalog, org,
                                                    *p.political_career);
      if (!valid.ok)
        return valid;
      for (const auto &post : p.political_career->staffing_posts)
        for (const auto &actor : post.candidate_actor_ids)
          if (!c.actor(actor))
            return common_failure("UnknownPoliticalStaffingCandidate", actor);
    }
    return common_success();
  };
  for (auto valid : {check(rules.subjects), check(rules.policies)})
    if (!valid.ok)
      return valid;
  const auto plans = [&](const auto &table) -> Status {
    for (const auto &row : table.rows()) {
      const auto &p = row.second;
      if (p.template_id != "political_career")
        continue;
      if (p.subject.kind != SubjectKind::actor)
        return common_failure("PoliticalDirectorActorSubjectRequired",
                              row.first);
      if (!catalog.enabled)
        return common_failure("PoliticalProfileRequired", row.first);
      const auto *appointment = state.appointments.find(p.goal_ref);
      if (appointment) {
        if (appointment->actor_id != p.subject.id &&
            appointment->issuer_actor_id != p.subject.id)
          return common_failure("PoliticalPlanActorMismatch", row.first);
      } else if (!state.jurisdictions.find(p.goal_ref))
        return common_failure("PoliticalPlanSourceMissing", row.first);
    }
    return common_success();
  };
  auto valid = plans(director.plans);
  if (!valid.ok)
    return valid;
  return plans(director.archived_plans);
}
} // namespace hanmo::resident
