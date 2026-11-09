#include "director_actor_proposal2.hpp"
#include "director_actor_read.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/interaction/interaction_model.hpp"
#include "resident/domains/interaction/public_supply_inspection.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
namespace {
// 两个对外别名共用真实拥有对象；普通深字符串的预留活到提案最后一个引用释放。
struct SupplyInspectionProposalOwned {
  CheckpointMemoryCharge memory;
  DirectorTransition transition;
  CommandPayload command;
  SupplyInspectionProposalOwned(CheckpointMemoryCharge charge,
                                const DirectorTransition &original,
                                const StableId &prop)
      : memory(std::move(charge)), transition(original),
        command{"inspect", InteractionCommand{Inspect{prop}}} {}
};
// 原责任选择先执行；仅因货源知识不足等待，才读本人尚未知的新一代原公告。
Status propose_actual_supply_inspection(const DirectorActorProposalContext2 &x,
                                        const KnownSubjectView &view,
                                        DecisionProposal &proposal) {
  if (!proposal.transition || proposal.command ||
      proposal.transition->template_id != "private_livelihood" ||
      !x.public_supply_inspection)
    return common_success();
  const auto &reason = proposal.transition->wait_reason;
  if (reason != "NoKnownSupply" && reason != "StaleSupplyObservation" &&
      reason != "InsufficientStock")
    return common_success();
  const auto &source = *x.public_supply_inspection;
  if (&source.common != &x.common || !x.public_supply_results ||
      !x.public_supply_stamp)
    return common_failure("DirectorPublicSupplyOwnerRequired", view.actor_id,
                          ErrorKind::internal);
  const auto *policy = director_policy(x.state, x.catalog, view.actor_id);
  if (!policy || !policy->private_livelihood)
    return common_failure("DirectorPrivateSupplyPolicyRequired", view.actor_id,
                          ErrorKind::internal);
  const StableId *selected = nullptr;
  for (const auto &row : source.catalog.notice_sources.rows()) {
    if (std::find(policy->report_source_keys.begin(),
                  policy->report_source_keys.end(),
                  row.second) == policy->report_source_keys.end())
      continue;
    const auto *definition =
        source.economy_catalog.supply_sources.find(row.second);
    if (!definition ||
        definition->item_id != policy->private_livelihood->item_id ||
        definition->site_id != view.place_id)
      continue;
    auto report = preview_public_supply_inspection(
        source, *x.public_supply_results, *x.public_supply_stamp, view.actor_id,
        row.first);
    if (!report.ok) {
      if (report.error.kind != ErrorKind::rule)
        return {false, {}, report.error};
      continue;
    }
    if (find_social_knowledge(*source.social.dynamic, view.subject,
                              report.value->id))
      continue;
    if (!selected || row.first < *selected)
      selected = &row.first;
  }
  if (!selected)
    return common_success();
  // 这里只提交原命令值，不把预览到的报告ID、库存或 receipt 提前写入计划。


  auto owned = checkpoint_make_shared<SupplyInspectionProposalOwned>(
      CheckpointMemoryCharge(0), *proposal.transition, *selected);
  owned->transition.phase = PlanPhase::sourcing;
  owned->transition.wait_reason.clear();
  proposal.transition =
      std::shared_ptr<const DirectorTransition>(owned, &owned->transition);
  proposal.command =
      std::shared_ptr<const CommandPayload>(owned, &owned->command);
  return common_success();
}
} // namespace

Status validate_director_actor_proposal2(const SubjectDecisionProposal2 &p) {
  if (p.route != DirectorDecisionRoute2::actor ||
      p.subject.kind != SubjectKind::actor || !p.actor_proposal)
    return common_failure("DirectorOriginalActorProposalRequired",
                          p.subject.id);
  const auto &a = *p.actor_proposal;
  // The scheduler's original no-candidate branch is an explicit waiting record;
  // it does not claim a business transition or a successful command.
  if (!a.transition)
    return !a.command && a.subject_id == p.subject.id &&
                   a.plan_id == p.plan_id && p.phase == PlanPhase::waiting &&
                   p.wait_reason == "NoKnownExecutableWork"
               ? common_success()
               : common_failure("DirectorOriginalActorProposalRequired",
                                p.subject.id);
  const auto &t = *a.transition;
  if (a.subject_id != p.subject.id || a.plan_id != p.plan_id ||
      t.registration_epoch != p.registration_epoch ||
      t.decision_seq != p.decision_seq || t.template_id != p.template_id ||
      t.goal_ref != p.goal_ref || t.next_step_index != p.next_step_index ||
      t.phase != p.phase || t.commitment_ids != p.commitment_ids ||
      t.known_report_ids != p.known_report_ids ||
      (!t.wait_reason.empty() && t.wait_reason != p.wait_reason) ||
      (!p.wait_reason.empty() && a.command))
    return common_failure("DirectorOriginalActorTransitionMismatch",
                          p.subject.id);
  return common_success();
}
Outcome<std::optional<SubjectDecisionProposal2>> propose_native_director_actor2(
    const DirectorActorProposalContext2 &x, const DirectorRegistration2 &r,
    const KnownSubjectView &view, Integer candidates) {
  if (!x.state.subject2 || !x.catalog.subjects2 ||
      r.subject.kind != SubjectKind::actor ||
      view.subject.kind != SubjectKind::actor ||
      view.subject.id != r.subject.id || view.actor_id != r.subject.id ||
      candidates < 1 || candidates > x.catalog.candidates_per_subject)
    return {
        false,
        {},
        common_failure("DirectorActorProposalContextMismatch2", r.subject.id)
            .error};
  const auto *registration =
      x.state.subject2->registrations.find(director_subject_key2(r.subject));
  if (!registration || registration != &r ||
      r.lifecycle != DirectorLifecycle::active)
    return {
        false,
        {},
        common_failure("DirectorActualActorRegistrationRequired2", r.subject.id)
            .error};
  auto proposals = propose_director(
      x.common, x.state, x.catalog, view, x.organization, x.affairs, x.tasks,
      x.task_catalog, x.common_catalog, x.growth, x.travel, x.organizations,
      x.organization_catalog, x.common.tick(), x.growth_access, x.life,
      x.membership, x.politics, x.diplomacy, x.issued_supply,
      x.task_responsibility, x.visit_reception, x.appointment_attendance,
      x.family_post);
  if (!proposals.ok)
    return {false, {}, proposals.error};
  if (proposals.value.empty())
    return {true, std::nullopt, {}};
  // The original selector emits its one ordered candidate. An unexpected wider
  // output cannot silently consume additional work or choose a different
  // policy.
  if (proposals.value.size() != 1 ||
      proposals.value.size() > static_cast<std::size_t>(candidates))
    return {false,
            {},
            common_failure("DirectorOriginalActorCandidateBudget", r.subject.id)
                .error};
  auto actor = std::move(proposals.value.front());
  auto inspection = propose_actual_supply_inspection(x, view, actor);
  if (!inspection.ok)
    return {false, {}, inspection.error};
  if (!actor.transition)
    return {
        false,
        {},
        common_failure("DirectorOriginalActorTransitionRequired", r.subject.id)
            .error};
  const auto &t = *actor.transition;
  SubjectDecisionProposal2 p;
  p.subject = r.subject;
  p.registration_epoch = r.epoch;
  p.decision_seq = t.decision_seq;
  p.policy_id = r.policy_id;
  p.plan_id = actor.plan_id;
  p.template_id = t.template_id;
  p.goal_ref = t.goal_ref;
  p.route = DirectorDecisionRoute2::actor;
  p.next_step_index = t.next_step_index;
  p.phase = t.phase;
  p.commitment_ids = t.commitment_ids;
  p.known_report_ids = t.known_report_ids;
  p.wait_reason = t.wait_reason;
  if (!actor.command && p.wait_reason.empty())
    p.wait_reason = "NoExecutableOriginalCommand";
  if (!p.wait_reason.empty())
    p.phase = PlanPhase::waiting;
  p.policy_source_ref = {"director", "actor_admission", r.admission_id};
  p.owner_source_revision = 0;
  p.expected_versions = actor.expected_versions;
  p.actor_proposal = std::move(actor);
  // A no-command original selecting transition is a durable wait, never
  // adopted.
  if (!p.actor_proposal->command &&
      p.actor_proposal->transition->phase != PlanPhase::waiting) {
    auto transition = checkpoint_make_shared<DirectorTransition>(
        *p.actor_proposal->transition);
    transition->phase = PlanPhase::waiting;
    p.actor_proposal->transition = std::move(transition);
  }
  auto valid = validate_director_actor_proposal2(p);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::optional<SubjectDecisionProposal2>{std::move(p)}, {}};
}
} // namespace hanmo::resident
