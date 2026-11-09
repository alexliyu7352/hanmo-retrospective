#include "resident/kernel/catalog_section_observation.hpp"
#include "condition_application_fact.hpp"
#include "condition_capacity_contract.hpp"
// 原Growth真实rest写入的同栈证据；保原数学与apply_actor一次，不新设身体setter。
#include "authored_origin.hpp"
#include "character_condition_read.hpp"
#include "character_condition_replay.hpp"
#include "created_origin.hpp"
#include "injury_progress_replay_v1.hpp"
#include "injury_source_links_v1.hpp"
#include "injury_service_coverage_v1.hpp"
#include "injury_sources_v1.hpp"
#include "resident/domains/growth/growth_fixed_execution.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include "resident/domains/growth/growth_recovery_qualification.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include "rest_condition.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {
struct AuthoredRestConditionBoundary::State {
  const CommonStore *common = nullptr;
  const AuthoredActorFoundationState *foundation = nullptr;
  std::shared_ptr<const ActorConditionCaptureIdentity> capture;
  ActorConditionSavepointBoundary savepoint_boundary;
  WorldIdentity identity;
  WorldMoment at;
  ActionCoreView action;
  ServiceTerms terms;
  ActorConditionHead head;
  FrozenBodyScalars before;
  std::size_t draft_offset = 0;
  Integer next_instance = 0;
  CharacterInjurySourcesStateV1 *injury_sources = nullptr;
  std::optional<GrowthRecoveryProgress> injury_before;
  bool consumed = false;
};
namespace {
// 原世界候选身份必须逐字段相同，不能仅比较tick。
bool same_identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
// 原ServiceTerms不可在开始后偷换。
bool same_terms(const ServiceTerms &a, const ServiceTerms &b) {
  return a.duration == b.duration && a.cost == b.cost && a.wage == b.wage &&
         a.hp_gain == b.hp_gain && a.fatigue_delta == b.fatigue_delta &&
         a.wallet_id == b.wallet_id;
}
// 检查原全局分配器实际拥有的身份，不以随意字符串发结果。
bool allocated(const CommonState &c, const StableId &id,
               const StableId &prefix) {
  if (id.rfind(prefix, 0) != 0)
    return false;
  const auto tail = id.substr(prefix.size());
  Integer n = 0;
  auto p = std::from_chars(tail.data(), tail.data() + tail.size(), n);
  return p.ec == std::errc{} && p.ptr == tail.data() + tail.size() && n > 0 &&
         n < c.next_instance_id && std::to_string(n) == tail;
}
// 只认原LifeActivity/原Common头和真实两人，不借今日钱包差值猜履约。
Outcome<const LifeActivityRecord *>
rest_source(const CommonState &c, const GrowthState &g,
            const ActorFoundationCatalog &catalog, const StableId &id) {
  const auto *r = g.life_activities.find(id);
  const auto *h = c.facts.find(id);
  const auto *t = r ? std::get_if<ServiceTerms>(&r->terms) : nullptr;
  if (!allocated(c, id, "action:") || !r || !h || h->id != id ||
      h->domain != "growth" || h->kind != "life_activity" || !t)
    return {false, nullptr,
            common_failure("AuthoredRestOriginalSourceRequired", id).error};
  if (!catalog.original_growth || !catalog.original_growth->regional_life)
    return {false, nullptr,
            common_failure("AuthoredRestOriginalCatalogRequired", id).error};
  const auto *role = catalog.original_growth->roles.find(r->context.role_id);
  const auto *authored = role ? role->activities.find("rest") : nullptr;
  if (!authored || !same_terms(*t, *authored))
    return {false, nullptr,
            common_failure("AuthoredRestOriginalTermsMismatch", id).error};
  const auto &v = r->view;
  if (v.fact_id != id || v.action_id != id ||
      v.kind != LifeActivityKind::service || v.activity != "rest" ||
      v.actor_id == v.provider_actor_id || !c.actors.find(v.actor_id) ||
      !c.actors.find(v.provider_actor_id) ||
      r->context.provider_actor_id != v.provider_actor_id ||
      r->context.place_id.empty() || r->context.role_id.empty() ||
      r->context.kind || r->context.encounter_id ||
      v.participant_actor_ids !=
          std::vector<StableId>{v.actor_id, v.provider_actor_id} ||
      t->duration < 1 || t->duration > 720 || t->hp_gain < 0 ||
      t->hp_gain > 100 || t->fatigue_delta < -100 || t->fatigue_delta > 100 ||
      t->cost < 0 || t->wage < 0 || r->cost_paid != t->cost)
    return {false, nullptr,
            common_failure("AuthoredRestOriginalSourceInvalid", id).error};
  return {true, r, {}};
}
// 原rest公式逐字段核实际差量；净零也保hp/fatigue真实写presence。
Status changes(const ConditionReceiptV2 &r, const ServiceTerms &t) {
  const bool injury = t.hp_gain > 0;
  const auto &a = r.scalar_changes;
  if (a.size() != (injury ? 3u : 2u))
    return common_failure("AuthoredRestScalarWriteSetMismatch", r.result_id);
  if (a[0].field != ConditionScalarChange::Field::hp ||
      a[1].field != ConditionScalarChange::Field::fatigue ||
      (injury && a[2].field != ConditionScalarChange::Field::injury))
    return common_failure("AuthoredRestScalarOrderMismatch", r.result_id);
  for (std::size_t i = 0; i < a.size(); ++i) {
    const Integer max = i == 2 ? 3 : 100;
    if (a[i].before < 0 || a[i].before > max || a[i].after < 0 ||
        a[i].after > max)
      return common_failure("AuthoredRestScalarRangeInvalid", r.result_id);
  }
  if (a[0].after != std::clamp<Integer>(a[0].before + t.hp_gain, 0, 100) ||
      a[1].after !=
          std::clamp<Integer>(a[1].before + t.fatigue_delta, 0, 100) ||
      (injury && a[2].after != std::max<Integer>(0, a[2].before - 1)))
    return common_failure("AuthoredRestScalarFormulaMismatch", r.result_id);
  return common_success();
}
// 收据自身与原完成源逐字段配对；实际Moment另由K2独立反读。
Status completed_shape(const CommonState &c, const ConditionReceiptV2 &r,
                       const ActorFoundationCatalog &catalog,
                       const GrowthState &growth) {
  if (!std::holds_alternative<OriginalRestProvidedCause>(r.cause) ||
      !std::holds_alternative<RestConditionBusinessKey>(r.business_key))
    return common_failure("RestConditionCauseMismatch", r.result_id);
  auto source =
      rest_source(c, growth, catalog,
                  std::get<OriginalRestProvidedCause>(r.cause).action_id);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &v = source.value->view;
  const auto &t = std::get<ServiceTerms>(source.value->terms);
  if (!allocated(c, r.result_id, "condition:") || r.actor_id != v.actor_id ||
      std::get<RestConditionBusinessKey>(r.business_key).action_id !=
          v.action_id ||
      std::get<RestConditionBusinessKey>(r.business_key).actor_id !=
          v.actor_id ||
      std::get<RestConditionBusinessKey>(r.business_key).completion_ordinal !=
          1 ||
      std::get<OriginalRestProvidedCause>(r.cause).rest_kind != "rest" ||
      std::get<OriginalRestProvidedCause>(r.cause).provider_actor_id !=
          v.provider_actor_id ||
      std::get<OriginalRestProvidedCause>(r.cause).site_id !=
          source.value->context.place_id ||
      std::get<OriginalRestProvidedCause>(r.cause).provided_half_days !=
          t.duration ||
      std::get<OriginalRestProvidedCause>(r.cause).service_fulfillment_ref !=
          v.action_id ||
      !actor_foundation_rule_equal(r.rule_ref, catalog.rest_rule) ||
      r.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      r.effective_at.exact_moment.half_day_tick != v.completed_tick ||
      v.phase != LifeActivityPhase::completed || !v.success ||
      !v.code.empty() || source.value->wage_paid != t.wage ||
      r.selected_injury_result.kind !=
          SelectedInjuryResult::Kind::not_an_injury ||
      !r.created_effect_keys.empty() || !r.recovery_resolution_refs.empty() ||
      r.life_transition_ref ||
      !original_condition_capacity_refs_shape(catalog, r))
    return common_failure("AuthoredRestReceiptSourceMismatch", r.result_id);
  return changes(r, t);
}
} // namespace
Outcome<AuthoredRestCompletionRead> read_authored_rest_completion(
    const CommonState &common, const GrowthState &growth,
    const ActorFoundationCatalog &catalog,
    const OriginalRestProvidedCause &cause, const StableId &actor_id,
    const AuthoredRestTimeRead &time, const WorldMoment &actual_now) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::authored_read, 12, "ActorFoundationCatalog", "read_authored_rest_completion", &catalog);

  auto source = rest_source(common, growth, catalog, cause.action_id);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &activity = *source.value;
  const auto &v = activity.view;
  const auto &terms = std::get<ServiceTerms>(activity.terms);
  if (catalog.identity.content_digest.empty() ||
      catalog.identity.content_digest != common.metadata.content_digest ||
      catalog.scenario_id != common.metadata.scenario_id ||
      cause.rest_kind != "rest" || actor_id != v.actor_id ||
      cause.provider_actor_id != v.provider_actor_id ||
      cause.site_id != activity.context.place_id ||
      cause.provided_half_days != terms.duration ||
      cause.service_fulfillment_ref != v.action_id ||
      v.phase != LifeActivityPhase::completed || !v.success ||
      !v.code.empty() || activity.wage_paid != terms.wage)
    return {
        false,
        {},
        common_failure("AuthoredRestCompletionSourceMismatch", cause.action_id)
            .error};
  auto completed = time.completed_rest_at(v.action_id);
  if (!completed.ok)
    return {false, {}, completed.error};
  auto order = compare_world_moments(completed.value, actual_now);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value > 0 || completed.value.half_day_tick != v.completed_tick)
    return {
        false,
        {},
        common_failure("AuthoredRestCompletionMomentMismatch", cause.action_id)
            .error};
  return {true, {&activity, completed.value, terms.hp_gain > 0}, {}};
}
Outcome<AuthoredRestConditionBoundary> begin_authored_rest_condition_write(
    CommonStore &common, const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &catalog, const GrowthState &growth,
    const ActionCoreView &action, const WorldMoment &actual_at) {
  auto fail =
      [](const StableId &code) -> Outcome<AuthoredRestConditionBoundary> {
    return {false, {}, common_failure(code).error};
  };
  auto origin = read_authored_character_origin_v2(common.state(), foundation,
                                                  catalog, action.actor_id);
  if (!origin.ok)
    return {false, {}, origin.error};
  auto source = rest_source(common.state(), growth, catalog, action.id);
  if (!source.ok)
    return {false, {}, source.error};
  const auto *payload = growth.actions.find(action.id);
  const auto *live = common.action(action.id);
  const auto *a = common.state().actors.find(action.actor_id);
  const auto *head =
      foundation.character_condition_extensions.actor_condition_heads.find(
          action.actor_id);
  auto moment = encode_world_moment(actual_at);
  if (!moment.ok)
    return {false, {}, moment.error};
  if (actual_at.half_day_tick < common.tick())
    return fail("AuthoredRestActualTimeInvalid");
  if (!live || !payload || !a || !head || action.kind != "service" ||
      live->kind != "service" || live->actor_id != action.actor_id ||
      live->participants != action.participants ||
      action.participants != source.value->view.participant_actor_ids ||
      action.remaining_ticks != 0 || live->remaining_ticks != 0 ||
      payload->request.activity != "rest" ||
      source.value->view.phase != LifeActivityPhase::active ||
      head->condition_revision == std::numeric_limits<Integer>::max())
    return fail("AuthoredRestActualWriteBoundaryRequired");
  const auto *terms = std::get_if<ServiceTerms>(&payload->quote.data);
  if (!terms ||
      !same_terms(*terms, std::get<ServiceTerms>(source.value->terms)))
    return fail("AuthoredRestTermsMismatch");
  for (const auto &row :
       foundation.character_condition_extensions.applied_receipts.rows())
    if ((std::holds_alternative<RestConditionBusinessKey>(
             row.second.business_key) &&
         std::get<RestConditionBusinessKey>(row.second.business_key)
                 .action_id == action.id))
      return fail("AuthoredRestAlreadyApplied");
  auto capture = common.actor_condition_capture_identity();
  auto boundary = common.actor_condition_savepoint_boundary();
  if (!capture || !boundary.ok)
    return fail("AuthoredRestActualCaptureRequired");
  auto p = std::make_shared<AuthoredRestConditionBoundary::State>();
  p->common = &common;
  p->foundation = &foundation;
  p->capture = std::move(capture);
  p->savepoint_boundary = std::move(boundary.value);
  p->identity = common.state().identity;
  p->at = actual_at;
  p->action = action;
  p->terms = *terms;
  p->head = *head;
  p->before = {a->alive, a->hp, a->fatigue, a->injury, {}};
  p->draft_offset = common.actor_condition_execution_drafts().size();
  p->next_instance = common.state().next_instance_id;
  AuthoredRestConditionBoundary out;
  out.state_ = std::move(p);
  return {true, std::move(out), {}};
}
Outcome<AuthoredRestConditionBoundary> begin_injury_rest_condition_write(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    CharacterInjurySourcesStateV1 &injury_sources,
    const CharacterInjuryReplaySourcesV1 &context, const ActionCoreView &action,
    const GrowthFixedStepPermit *actual_gate) {
  const auto &body = context.conditions;
  if (&body.common != &common || &body.state != &foundation ||
      &context.injury != &injury_sources || !body.growth ||
      !body.growth_catalog || !body.shared)
    return {false,
            {},
            common_failure("InjuryRestSameCandidateRequired", action.id).error};
  const auto &catalog = body.catalog;
  const auto &growth = *body.growth;
  const auto &actual_at = body.actual_now;
  auto qualified = validate_injury_recovery_selection_v1(body);
  if (!qualified.ok)
    return {false, {}, qualified.error};
  const auto *activity = growth.life_activities.find(action.id);
  if (!activity || (body.injury_recovery_catalog &&
                    body.injury_recovery_catalog->service_roles.find(
                        activity->context.role_id)))
    return {
        false,
        {},
        common_failure("InjuryRestOrdinaryServiceRequired", action.id).error};
  auto fail =
      [](const StableId &code) -> Outcome<AuthoredRestConditionBoundary> {
    return {false, {}, common_failure(code).error};
  };
  if (actual_gate) {
    auto valid_gate =
        validate_growth_fixed_step_permit(*actual_gate, common, growth, action);
    if (!valid_gate.ok)
      return {false, {}, valid_gate.error};
    auto at = compare_world_moments(actual_gate->at(), actual_at);
    if (!at.ok)
      return {false, {}, at.error};
    if (!actual_gate->final_gate() || at.value)
      return fail("InjuryRestFullGateRequired");
  }
  auto origin = read_character_initial_values_v2(
      common, foundation, catalog, body.family_catalog, body.shared,
      action.actor_id, actual_at, body.born_sources);
  if (!origin.ok)
    return {false, {}, origin.error};
  auto source = rest_source(common.state(), growth, catalog, action.id);
  if (!source.ok)
    return {false, {}, source.error};
  const auto *payload = growth.actions.find(action.id);
  const auto *live = common.action(action.id);
  const auto *a = common.state().actors.find(action.actor_id);
  const auto *head =
      foundation.character_condition_extensions.actor_condition_heads.find(
          action.actor_id);
  auto moment = encode_world_moment(actual_at);
  if (!moment.ok)
    return {false, {}, moment.error};
  if (actual_at.half_day_tick < common.tick())
    return fail("AuthoredRestActualTimeInvalid");
  if (!live || !payload || !a || !head || action.kind != "service" ||
      live->kind != "service" || live->actor_id != action.actor_id ||
      live->participants != action.participants ||
      action.participants != source.value->view.participant_actor_ids ||
      (!actual_gate &&
       (action.remaining_ticks != 0 || live->remaining_ticks != 0)) ||
      payload->request.activity != "rest" ||
      source.value->view.phase != LifeActivityPhase::active ||
      head->condition_revision == std::numeric_limits<Integer>::max())
    return fail("AuthoredRestActualWriteBoundaryRequired");
  const auto *terms = std::get_if<ServiceTerms>(&payload->quote.data);
  if (!terms ||
      !same_terms(*terms, std::get<ServiceTerms>(source.value->terms)))
    return fail("AuthoredRestTermsMismatch");
  for (const auto &row :
       foundation.character_condition_extensions.applied_receipts.rows())
    if ((std::holds_alternative<RestConditionBusinessKey>(
             row.second.business_key) &&
         std::get<RestConditionBusinessKey>(row.second.business_key)
                 .action_id == action.id))
      return fail("AuthoredRestAlreadyApplied");
  // 点数投影不能替代身体权限；真实旧因果前缀必须另验，再与当前身体配对。
  auto verified =
      read_character_condition_verified_prefix(body, *head, actual_at);
  if (!verified.ok)
    return {false, {}, verified.error};
  if (verified.value.body.hp != a->hp ||
      verified.value.body.fatigue != a->fatigue ||
      verified.value.body.injury != a->injury ||
      verified.value.body.alive != a->alive)
    return fail("InjuryRestVerifiedBodyMismatch");
  auto progress =
      read_character_injury_progress_at_head_v1(context, *head, actual_at);
  if (!progress.ok)
    return {false, {}, progress.error};
  if (progress.value.injury != a->injury)
    return fail("InjuryRestBeforeBodyMismatch");
  for (const auto &row : injury_sources.applications.rows())
    if (const auto *rest =
            std::get_if<OriginalRestProvidedCause>(&row.second.original_source);
        rest && rest->action_id == action.id)
      return fail("InjuryRestAlreadyApplied");
  auto capture = common.actor_condition_capture_identity();
  auto boundary = common.actor_condition_savepoint_boundary();
  if (!capture || !boundary.ok)
    return fail("AuthoredRestActualCaptureRequired");
  auto p = std::make_shared<AuthoredRestConditionBoundary::State>();
  p->injury_sources = &injury_sources;
  p->injury_before = progress.value;
  p->common = &common;
  p->foundation = &foundation;
  p->capture = std::move(capture);
  p->savepoint_boundary = std::move(boundary.value);
  p->identity = common.state().identity;
  p->at = actual_at;
  p->action = action;
  p->terms = *terms;
  p->head = *head;
  p->before = {a->alive, a->hp, a->fatigue, a->injury, {}};
  p->draft_offset = common.actor_condition_execution_drafts().size();
  p->next_instance = common.state().next_instance_id;
  AuthoredRestConditionBoundary out;
  out.state_ = std::move(p);
  return {true, std::move(out), {}};
}
Outcome<BoundAuthoredRestConditionWrite> bind_authored_rest_condition_write(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &catalog, const GrowthState &growth,
    const ActionCoreView &action, const WorldMoment &actual_at,
    AuthoredRestConditionBoundary &boundary) {
  auto fail =
      [](const StableId &code) -> Outcome<BoundAuthoredRestConditionWrite> {
    return {false, {}, common_failure(code).error};
  };
  auto p = boundary.state_;
  if (!p || p->consumed || p->common != &common ||
      p->foundation != &foundation ||
      p->capture != common.actor_condition_capture_identity() ||
      !same_identity(p->identity, common.state().identity) ||
      !common.validate_actor_condition_savepoint_boundary(p->savepoint_boundary)
           .ok ||
      action.id != p->action.id)
    return fail("AuthoredRestConditionBoundaryExpired");
  auto order = compare_world_moments(p->at, actual_at);
  if (!order.ok)
    return {false, {}, order.error};
  if (order.value != 0)
    return fail("AuthoredRestActualTimeMismatch");
  const auto &drafts = common.actor_condition_execution_drafts();
  if (drafts.size() != p->draft_offset + 1)
    return fail("AuthoredRestActualWriteCountMismatch");
  const auto &draft = drafts[p->draft_offset];
  const auto *a = common.state().actors.find(action.actor_id);
  auto *h =
      foundation.character_condition_extensions.actor_condition_heads.find(
          action.actor_id);
  if (!a || !h || h->condition_revision != p->head.condition_revision ||
      h->latest_receipt_ref.has_value() !=
          p->head.latest_receipt_ref.has_value() ||
      (h->latest_receipt_ref && h->latest_receipt_ref->result_id !=
                                    p->head.latest_receipt_ref->result_id) ||
      draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
      draft.actor_id != action.actor_id || !draft.before || !draft.hp_written ||
      !draft.fatigue_written || !same_identity(draft.identity, p->identity) ||
      draft.sequence != Integer(p->draft_offset + 1) ||
      draft.before->hp != p->before.hp ||
      draft.before->fatigue != p->before.fatigue ||
      draft.before->injury != p->before.injury ||
      draft.before->alive != p->before.alive ||
      draft.injury_written != (p->terms.hp_gain > 0) || draft.alive_written ||
      draft.after.injury != a->injury || draft.after.alive != a->alive ||
      draft.after.hp != a->hp || draft.after.fatigue != a->fatigue ||
      a->alive != p->before.alive ||
      draft.next_instance_before < p->next_instance ||
      draft.next_instance_before != draft.next_instance_after)
    return fail("AuthoredRestActualWriteMismatch");
  auto source = rest_source(common.state(), growth, catalog, action.id);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &v = source.value->view;
  const auto &terms = std::get<ServiceTerms>(source.value->terms);
  if (!same_terms(terms, p->terms) || v.phase != LifeActivityPhase::completed ||
      !v.success || !v.code.empty() ||
      v.completed_tick != actual_at.half_day_tick ||
      source.value->wage_paid != terms.wage)
    return fail("AuthoredRestCompletionRequired");
  ConditionReceiptV2 r;
  r.actor_id = action.actor_id;
  r.business_key = RestConditionBusinessKey{action.id, action.actor_id, 1};
  r.cause = OriginalRestProvidedCause{
      action.id,           "rest",
      v.provider_actor_id, source.value->context.place_id,
      terms.duration,      action.id};
  r.rule_ref = catalog.rest_rule;
  r.effective_at.exact_moment = actual_at;
  r.condition_revision_before = h->condition_revision;
  r.condition_revision_after = h->condition_revision + 1;
  r.scalar_changes = {
      {ConditionScalarChange::Field::hp, p->before.hp, a->hp},
      {ConditionScalarChange::Field::fatigue, p->before.fatigue, a->fatigue}};
  if (terms.hp_gain > 0)
    r.scalar_changes.push_back(
        {ConditionScalarChange::Field::injury, p->before.injury, a->injury});
  else if (a->injury != p->before.injury)
    return fail("AuthoredRestUnexpectedInjuryWrite");
  auto valid = changes(r, terms);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto id = common.next_id("condition");
  if (!id.ok)
    return {false, {}, id.error};
  r.result_id = id.value;
  if (p->injury_sources) {
    if (!p->injury_before)
      return fail("InjuryRestPreparedProgressRequired");
    auto after =
        apply_growth_rest_recovery_v1(*p->injury_before, terms.hp_gain > 0);
    if (!after.ok)
      return {false, {}, after.error};
    if (after.value.injury != a->injury)
      return fail("InjuryRestActualProgressMismatch");


    auto copied_source = CheckpointMemoryCharge(0);
    CharacterInjuryApplicationV1 app{
        r.actor_id,
        std::get<OriginalRestProvidedCause>(r.cause),
        {*p->injury_before, after.value, 0, 0}};
    if (!p->injury_sources->applications.insert(r.result_id, std::move(app)))
      return fail("InjuryRestApplicationCollision");
    r.cause = CharacterInjurySourceRefV1{r.result_id};
    r.business_key =
        CharacterInjuryConditionBusinessKeyV1{{r.result_id}, r.actor_id};
  }
  const ConditionReceiptRefV2 ref{r.result_id};
  if (!foundation.character_condition_extensions.applied_receipts.insert(
          ref.result_id, std::move(r)))
    return fail("AuthoredRestReceiptCollision");
  h->condition_revision += 1;
  h->latest_receipt_ref = ref;
  BoundActorConditionWrite actual{ref, p->capture, draft.sequence};
  valid = append_condition_application_fact_in_candidate(common, foundation,
                                                         catalog, actual);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (p->injury_sources) {
    valid = validate_character_injury_bound_source_link_v1(
        common, foundation, catalog, *p->injury_sources, actual);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  p->consumed = true;
  return {true, std::move(actual), {}};
}
Status validate_authored_rest_receipt(const CommonState &c,
                                      const ConditionReceiptV2 &r,
                                      const ActorFoundationCatalog &catalog,
                                      const GrowthState &growth,
                                      const AuthoredRestTimeRead &times) {
  auto valid = completed_shape(c, r, catalog, growth);
  if (!valid.ok)
    return valid;
  auto at = times.completed_rest_at(
      std::get<OriginalRestProvidedCause>(r.cause).action_id);
  if (!at.ok)
    return {false, {}, at.error};
  auto equal = compare_world_moments(at.value, r.effective_at.exact_moment);
  if (!equal.ok)
    return {false, {}, equal.error};
  return equal.value == 0
             ? common_success()
             : common_failure("AuthoredRestCompletedMomentMismatch",
                              r.result_id);
}
Status validate_authored_rest_capture(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const std::vector<BoundAuthoredRestConditionWrite> &bindings) {
  const auto capture = common.actor_condition_capture_identity();
  const auto &drafts = common.actor_condition_execution_drafts();
  if (!capture || drafts.size() != bindings.size())
    return common_failure("AuthoredRestCaptureCoverageMismatch");
  for (std::size_t n = 0; n < drafts.size(); ++n) {
    const auto &b = bindings[n];
    const auto &d = drafts[n];
    const auto *r = state.character_condition_extensions.applied_receipts.find(
        b.receipt_ref.result_id);
    if (b.capture_identity != capture || b.capture_sequence != Integer(n + 1) ||
        d.sequence != b.capture_sequence || !r || d.actor_id != r->actor_id ||
        d.kind != ActorConditionExecutionDraft::Kind::mutation || !d.before ||
        !d.hp_written || !d.fatigue_written || d.alive_written ||
        d.before->alive != d.after.alive ||
        d.injury_written != (r->scalar_changes.size() == 3) ||
        (!d.injury_written && d.before->injury != d.after.injury) ||
        (d.injury_written && (r->scalar_changes[2].before != d.before->injury ||
                              r->scalar_changes[2].after != d.after.injury)) ||
        r->scalar_changes.size() < 2 ||
        r->scalar_changes[0].before != d.before->hp ||
        r->scalar_changes[0].after != d.after.hp ||
        r->scalar_changes[1].before != d.before->fatigue ||
        r->scalar_changes[1].after != d.after.fatigue)
      return common_failure("AuthoredRestCaptureBindingMismatch");
  }
  return common_success();
}
} // namespace hanmo::resident
