#include "service_condition.hpp"
#include "authored_origin.hpp"
#include "condition_application_fact.hpp"
#include "injury_progress_replay_v1.hpp"
#include "injury_service_coverage_v1.hpp"
#include "injury_sources_v1.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "rest_condition.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool same_head(const ActorConditionHead &a, const ActorConditionHead &b) {
  return a.actor_id == b.actor_id &&
         a.condition_revision == b.condition_revision &&
         a.initial_condition_seed_ref.seed_key ==
             b.initial_condition_seed_ref.seed_key &&
         bool(a.latest_receipt_ref) == bool(b.latest_receipt_ref) &&
         (!a.latest_receipt_ref ||
          a.latest_receipt_ref->result_id == b.latest_receipt_ref->result_id);
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
const LifeActivityRecord &original(const CharacterConditionSources &body,
                                   const StableId &id) {
  if (!body.growth || !body.growth_catalog)
    fail("ServiceConditionOriginalSourcesRequired", id);
  const auto *source = body.growth->life_activities.find(id);
  const auto *header = body.common.state().facts.find(id);
  if (!source || !header || header->domain != "growth" ||
      header->kind != "life_activity" || source->view.fact_id != id ||
      source->view.action_id != id ||
      source->view.kind != LifeActivityKind::service ||
      (source->view.activity != "rest" && source->view.activity != "work"))
    fail("ServiceConditionOriginalSourceRequired", id);
  const auto *terms = std::get_if<ServiceTerms>(&source->terms);
  const auto *role = body.growth_catalog->roles.find(source->context.role_id);
  const auto *rule =
      role ? role->activities.find(source->view.activity) : nullptr;
  if (!terms || !rule || terms->duration != rule->duration ||
      terms->cost != rule->cost || terms->wage != rule->wage ||
      terms->wallet_id != rule->wallet_id || terms->hp_gain != rule->hp_gain ||
      terms->fatigue_delta != rule->fatigue_delta ||
      source->cost_paid != terms->cost || terms->duration < 1)
    fail("ServiceConditionOriginalTermsMismatch", id);
  return *source;
}
void deltas(const ConditionReceiptV2 &receipt, const ServiceTerms &terms,
            bool rest) {
  const auto &changes = receipt.scalar_changes;
  const bool injury = rest && terms.hp_gain > 0;
  if (changes.size() != (injury ? 3u : 2u) ||
      changes[0].field != ConditionScalarChange::Field::hp ||
      changes[1].field != ConditionScalarChange::Field::fatigue ||
      (injury && changes[2].field != ConditionScalarChange::Field::injury))
    fail("ServiceConditionScalarPresenceMismatch", receipt.result_id);
  for (std::size_t n = 0; n < changes.size(); ++n) {
    const auto &value = changes[n];
    const Integer cap = n == 2 ? 3 : 100;
    if (value.before < 0 || value.before > cap || value.after < 0 ||
        value.after > cap)
      fail("ServiceConditionScalarRangeInvalid", receipt.result_id);
  }
  if (changes[0].after !=
          std::clamp<Integer>(changes[0].before + terms.hp_gain, 0, 100) ||
      changes[1].after !=
          std::clamp<Integer>(changes[1].before + terms.fatigue_delta, 0,
                              100) ||
      (injury &&
       changes[2].after != std::max<Integer>(0, changes[2].before - 1)))
    fail("ServiceConditionOriginalFormulaMismatch", receipt.result_id);
}
} // namespace
// source1普通休息沿原Service写身体，再由既有binder绑定唯一伤情应用。
// 先读完整旧前缀，实写后不重建World、不再次写身体；失败交外层撤回整个候选。
Outcome<GrowthFixedStudyApplication> apply_injury_rest_fixed_gate_in_candidate(
    CommonStore &common, GrowthState &growth,
    AuthoredActorFoundationState &foundation,
    CharacterInjurySourcesStateV1 &injury,
    const CharacterConditionSources &body, const GrowthCatalog &catalog,
    const CommonCatalog &shared, SocialState &social,
    InteractionState &interaction, const GrowthAccess &access,
    const ActionTimeRegistration &registration,
    const GrowthFixedStepPermit &gate) {
  try {
    if (&body.common != &common || &body.state != &foundation ||
        body.growth != &growth || body.growth_catalog != &catalog ||
        body.shared != &shared || body.injury_sources != &injury ||
        registration.owner_domain != "growth" ||
        registration.action_kind != "service" || !gate.final_gate() ||
        access.personal_recovery)
      fail("InjuryRestSameCandidateRequired", gate.action_id());
    take(validate_original_injury_rest_service_sources(body));
    const auto *live = common.action(gate.action_id());
    if (!live || live->kind != "service")
      fail("ServiceConditionActualActionRequired", gate.action_id());
    const auto &source = original(body, gate.action_id());
    if (source.view.activity != "rest" ||
        source.view.phase != LifeActivityPhase::active)
      fail("InjuryRestOrdinaryServiceRequired", gate.action_id());
    // 原step可更新Action；边界保同一真实Action的值而非失效的表内引用。
    const auto action = *live;
    const CharacterInjuryReplaySourcesV1 replay{
        body, injury, body.injury_personal, body.injury_field};
    auto boundary = take(begin_injury_rest_condition_write(
        common, foundation, injury, replay, action, &gate));
    auto result =
        take(step_growth_at_fixed_gate(common, growth, catalog, shared, social,
                                       interaction, access, action, gate));
    if (!result.completed)
      fail("ServiceConditionOriginalTerminalRequired", gate.action_id());
    auto bound = take(bind_authored_rest_condition_write(
        common, foundation, body.catalog, growth, action, gate.at(), boundary));
    GrowthFixedStudyApplication out;
    out.result = std::move(result);
    out.actual_condition_writes.push_back(std::move(bound));
    return {true, std::move(out), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}

// 当前Complete只接普通rest的原最终完成；治疗、长休与日历恢复仍归原拒绝门。
Status validate_original_injury_rest_service_sources(
    const CharacterConditionSources &body) {
  try {
    const auto source1 = take(condition_application_facts_selected(
        body.catalog.identity,
        ConditionApplicationHistoryOwner::injury_sources));
    if (!source1 || !body.injury_sources || !body.growth ||
        !body.growth_catalog || !body.shared || !body.elapsed || !body.scale ||
        !body.rest_time || !body.action_registrations ||
        body.catalog.original_growth.get() != body.growth_catalog ||
        body.injury_recovery_catalog)
      fail("InjuryRestOriginalOwningSourcesRequired");
    std::size_t count = 0;
    for (const auto &registration : *body.action_registrations)
      if (registration.owner_domain == "growth" &&
          registration.action_kind == "service") {
        if (++count != 1 ||
            registration.profile_id != "growth_fixed_service_time2" ||
            registration.mode != ActionTimeMode::fixed_duration ||
            registration.source_schema != 1)
          fail("InjuryRestOriginalServiceRegistrationRequired");
      }
    if (count != 1)
      fail("InjuryRestOriginalServiceRegistrationRequired");
    take(validate_injury_recovery_selection_v1(body));
    // 完成表枚举源、应用表反查源，两边必须一一对应；原Time2时间由rest_time读取。
    take(
        validate_injury_service_source_coverage_v1(body, *body.injury_sources));
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}

// 完成真实服务并核对完整人物记录与身体草稿；来源或终态不符时保留原失败。
Outcome<GrowthFixedStudyApplication> apply_service_fixed_gate_in_candidate(
    CommonStore &common, GrowthState &growth,
    AuthoredActorFoundationState &foundation,
    const CharacterConditionSources &body, const GrowthCatalog &catalog,
    const CommonCatalog &shared, SocialState &social,
    InteractionState &interaction, const GrowthAccess &access,
    const ActionTimeRegistration &registration,
    const GrowthFixedStepPermit &gate) {
  try {
    if (&body.common != &common || &body.state != &foundation ||
        body.growth != &growth || body.growth_catalog != &catalog ||
        body.shared != &shared || registration.owner_domain != "growth" ||
        registration.action_kind != "service" || !gate.final_gate() ||
        access.personal_recovery)
      fail("CompleteOriginalServiceSourcesRequired", gate.action_id());
    const auto *action = common.action(gate.action_id());
    if (!action || action->kind != "service")
      fail("ServiceConditionActualActionRequired", gate.action_id());
    take(validate_growth_fixed_step_permit(gate, common, growth, *action));
    if (take(compare_world_moments(gate.at(), body.actual_now)))
      fail("ServiceConditionActualMomentRequired", gate.action_id());
    const auto &source = original(body, gate.action_id());
    if (source.view.phase != LifeActivityPhase::active)
      fail("ServiceConditionAlreadyApplied", gate.action_id());
    const bool rest = source.view.activity == "rest";
    if (rest && body.injury_sources)
      fail("InjuryRestOriginalTypedWriterRequired", gate.action_id());
    const auto terms = std::get<ServiceTerms>(source.terms);
    const auto actor_id = source.view.actor_id;
    const auto provider_id = source.view.provider_actor_id;
    const auto site = source.context.place_id;
    auto before = take(read_current_character_condition(body, actor_id));
    if (before.head.condition_revision == std::numeric_limits<Integer>::max())
      fail("ConditionRevisionOverflow", actor_id);
    if (!rest && !body.catalog.work_or_study_rule)
      fail("GrowthWorkConditionRuleRequired");
    const auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("ServiceConditionActualCaptureRequired", gate.action_id());
    const auto offset = common.actor_condition_execution_drafts().size();
    const auto savepoint = take(common.actor_condition_savepoint_boundary());
    auto result =
        take(step_growth_at_fixed_gate(common, growth, catalog, shared, social,
                                       interaction, access, *action, gate));
    if (!result.completed)
      fail("ServiceConditionOriginalTerminalRequired", gate.action_id());
    take(common.validate_actor_condition_savepoint_boundary(savepoint));
    const auto &drafts = common.actor_condition_execution_drafts();
    if (common.actor_condition_capture_identity() != capture ||
        drafts.size() != offset + 1)
      fail("ServiceConditionActualWriteCountMismatch", gate.action_id());
    const auto &draft = drafts[offset];
    auto *head =
        foundation.character_condition_extensions.actor_condition_heads.find(
            actor_id);
    const auto *actual_actor = common.state().actors.find(actor_id);
    if (!head || !actual_actor || !same_head(*head, before.head) ||
        !same_identity(draft.identity, common.state().identity) ||
        actual_actor->hp != draft.after.hp ||
        actual_actor->fatigue != draft.after.fatigue ||
        actual_actor->injury != draft.after.injury ||
        actual_actor->alive != draft.after.alive ||
        (!draft.injury_written && draft.after.injury != before.body.injury) ||
        draft.sequence != static_cast<Integer>(offset + 1) ||
        draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
        draft.actor_id != actor_id || !draft.before ||
        draft.before->alive != before.body.alive ||
        draft.before->hp != before.body.hp ||
        draft.before->fatigue != before.body.fatigue ||
        draft.before->injury != before.body.injury || !draft.hp_written ||
        !draft.fatigue_written ||
        draft.injury_written != (rest && terms.hp_gain > 0) ||
        draft.alive_written || draft.after.alive != draft.before->alive ||
        draft.next_instance_before != draft.next_instance_after)
      fail("ServiceConditionActualDraftMismatch", actor_id);
    const auto &completed = original(body, gate.action_id());
    if (completed.view.phase != LifeActivityPhase::completed ||
        !completed.view.success ||
        completed.view.completed_tick != body.actual_now.half_day_tick ||
        completed.wage_paid != terms.wage)
      fail("ServiceConditionActualCompletionMismatch", gate.action_id());
    ConditionReceiptV2 receipt;
    receipt.result_id = take(common.next_id("condition"));
    receipt.actor_id = actor_id;
    receipt.condition_revision_before = head->condition_revision;
    receipt.condition_revision_after = head->condition_revision + 1;
    receipt.effective_at.exact_moment = body.actual_now;
    receipt.scalar_changes = {
        {ConditionScalarChange::Field::hp, draft.before->hp, draft.after.hp},
        {ConditionScalarChange::Field::fatigue, draft.before->fatigue,
         draft.after.fatigue}};
    if (rest && terms.hp_gain > 0)
      receipt.scalar_changes.push_back({ConditionScalarChange::Field::injury,
                                        draft.before->injury,
                                        draft.after.injury});
    if (rest) {
      receipt.business_key =
          RestConditionBusinessKey{gate.action_id(), actor_id, 1};
      receipt.cause = OriginalRestProvidedCause{
          gate.action_id(), "rest",          provider_id, site,
          terms.duration,   gate.action_id()};
      receipt.rule_ref = body.catalog.rest_rule;
    } else {
      receipt.business_key =
          WorkOrStudyConditionBusinessKey{gate.action_id(), actor_id, 1};
      receipt.cause = WorkOrStudyConditionCause{
          WorkOrStudyConditionCause::Kind::service_completion, gate.action_id(),
          actor_id, 1};
      receipt.rule_ref = *body.catalog.work_or_study_rule;
    }
    deltas(receipt, terms, rest);
    const ConditionReceiptRefV2 ref{receipt.result_id};
    if (!foundation.character_condition_extensions.applied_receipts.insert(
            ref.result_id, std::move(receipt)))
      fail("DuplicateConditionReceipt", ref.result_id);
    head->condition_revision += 1;
    head->latest_receipt_ref = ref;
    BoundActorConditionWrite actual{ref, capture,
                                    static_cast<Integer>(offset + 1)};
    take(append_condition_application_fact_in_candidate(common, foundation,
                                                        body.catalog, actual));
    GrowthFixedStudyApplication out;
    out.result = std::move(result);
    out.actual_condition_writes.push_back(std::move(actual));
    return {true, std::move(out), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status
validate_original_work_service_condition(const CharacterConditionSources &body,
                                         const ConditionReceiptV2 &receipt) {
  try {
    const auto *cause = std::get_if<WorkOrStudyConditionCause>(&receipt.cause);
    const auto *key =
        std::get_if<WorkOrStudyConditionBusinessKey>(&receipt.business_key);
    if (!cause ||
        cause->kind != WorkOrStudyConditionCause::Kind::service_completion ||
        cause->ordinal != 1 || !key || key->action_id != cause->action_id ||
        key->actor_id != cause->actor_id || key->ordinal != 1 ||
        receipt.actor_id != cause->actor_id ||
        !body.catalog.work_or_study_rule ||
        !actor_foundation_rule_equal(receipt.rule_ref,
                                     *body.catalog.work_or_study_rule))
      fail("ServiceConditionCauseMismatch", receipt.result_id);
    const auto &source = original(body, cause->action_id);
    if (source.view.activity != "work" ||
        source.view.actor_id != cause->actor_id ||
        source.view.phase != LifeActivityPhase::completed ||
        !source.view.success ||
        source.wage_paid != std::get<ServiceTerms>(source.terms).wage ||
        !body.elapsed || !body.scale || !body.shared ||
        !body.action_registrations)
      fail("ServiceConditionCompletedWorkRequired", cause->action_id);
    const ActionTimeRegistration *selected = nullptr;
    for (const auto &registration : *body.action_registrations)
      if (registration.owner_domain == "growth" &&
          registration.action_kind == "service") {
        if (selected)
          fail("DuplicateGrowthServiceRegistration");
        selected = &registration;
      }
    if (!selected)
      fail("GrowthServiceRegistrationRequired");
    GrowthActionTimeRead reader(body.common, *body.growth, *body.growth_catalog,
                                *body.shared, *body.elapsed, *body.scale,
                                body.catalog.identity, body.actual_now, {},
                                body.interaction, body.interaction_catalog);
    take(reader.validate_adopted_terminal_source(*selected, cause->action_id));
    auto time =
        take(read_growth_original_time(reader, *selected, cause->action_id));
    if (!time.ended_at ||
        receipt.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
        take(compare_world_moments(*time.ended_at,
                                   receipt.effective_at.exact_moment)))
      fail("ServiceConditionActualEndMismatch", receipt.result_id);
    deltas(receipt, std::get<ServiceTerms>(source.terms), false);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_original_work_service_condition_coverage(
    const CharacterConditionSources &body) {
  try {
    if (!body.growth)
      return common_success();
    for (const auto &row : body.growth->life_activities.rows()) {
      const auto &source = row.second.view;
      if (source.kind != LifeActivityKind::service ||
          source.activity != "work" ||
          source.phase != LifeActivityPhase::completed)
        continue;
      std::size_t count = 0;
      for (const auto &actual :
           body.state.character_condition_extensions.applied_receipts.rows())
        if (const auto *cause =
                std::get_if<WorkOrStudyConditionCause>(&actual.second.cause);
            cause &&
            cause->kind ==
                WorkOrStudyConditionCause::Kind::service_completion &&
            cause->action_id == row.first) {
          take(validate_original_work_service_condition(body, actual.second));
          ++count;
        }
      if (count != 1)
        fail("ServiceConditionWorkCoverageMismatch", row.first);
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
