#include "growth_fixed_start.hpp"
#include "growth_action_time_read.hpp"
#include "growth_equal.hpp"
#include "growth_internal.hpp"
#include "growth_market_start_read.hpp"
#include "growth_quiet_source.hpp"
#include "growth_started_participation.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/estate/estate_rest_history.hpp"
#include "resident/domains/estate/estate_title_history.hpp"
#include "resident/domains/interaction/interaction_action_confirmation.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/model/procurement_timing.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {},
                       ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, id, kind).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
LifeCatalogFieldRef field(const CharacterConditionSources &body,
                          const StableId &entity, const StableId &selector) {
  return {body.catalog.identity, body.common.state().metadata.scenario_id,
          entity, selector};
}
SourceRef owner(const LifeFixedServiceBoundary &boundary) {
  return {"growth",
          boundary.owner_ref.kind == 1 ? "life_activity"
          : boundary.owner_ref.fact_kind == GrowthFixedFactKind::training
              ? "training"
          : boundary.owner_ref.fact_kind == GrowthFixedFactKind::study
              ? "skill_study"
              : "long_rest",
          boundary.action_ref};
}
const ActionTimeRegistration &
registration(const CharacterConditionSources &body, const StableId &kind) {
  const ActionTimeRegistration *selected = nullptr;
  for (const auto &row : *body.action_registrations)
    if (row.owner_domain == "growth" && row.action_kind == kind) {
      if (selected)
        fail("DuplicateGrowthFixedRegistration", kind);
      selected = &row;
    }
  if (!selected)
    fail("GrowthFixedRegistrationMissing", kind,
         ErrorKind::unsupported_capability);
  return *selected;
}
void actor(LifeFixedServiceBoundary &boundary, const StableId &id,
           const char *role) {
  LifeFixedActorInputs input;
  input.actor_id = id;
  input.role = role;
  input.origin_actor_key = id;
  input.participation_source_refs.emplace_back(
      LifeOwnedFieldRef{owner(boundary), "participant_actor_ids"});
  input.occupation_source_refs.emplace_back(
      LifeOwnedFieldRef{owner(boundary), "action_id"});
  boundary.input_snapshot.actors.push_back(std::move(input));
}
void original_inputs(const CharacterConditionSources &body,
                     const GrowthState &growth, const ActionCoreView &action,
                     LifeFixedServiceBoundary &boundary,
                     const GrowthFixedCaptureServices &services) {
  const auto *raw = growth.facts.find(action.id);
  const auto *life = growth.life_activities.find(action.id);
  if (bool(raw) == bool(life))
    fail("GrowthFixedActualStartOwnerRequired", action.id);
  boundary.action_ref = action.id;
  boundary.action_kind = action.kind;
  boundary.occurred_at = body.actual_now;
  if (raw) {
    boundary.owner_ref.kind = 0;
    boundary.owner_ref.action_id = action.id;
    boundary.input_snapshot.specific.kind = 0;
    auto &specific = boundary.input_snapshot.specific.growth_fact;
    specific.terms_action_ref = action.id;
    if (const auto *training = std::get_if<TrainingFact>(raw)) {
      if (training->phase != "started" ||
          training->start_tick != body.actual_now.half_day_tick ||
          training->action_kind != action.kind)
        fail("GrowthFixedActualTrainingStartRequired", action.id);
      if (action.kind == "learn_skill" && !training->context)
        fail("GrowthOriginalTrainingContextRequired", action.id);
      boundary.owner_ref.fact_kind = GrowthFixedFactKind::training;
      actor(boundary, training->training.student_id, "student");
      if (!training->training.teacher_id.empty())
        actor(boundary, training->training.teacher_id, "teacher");
      specific.original_training_or_study_rule_ref =
          field(body,
                action.kind == "learn_skill" ? training->context->role_id
                                             : "cultivation",
                action.kind == "learn_skill"
                    ? "lessons." + training->training.skill_id
                    : action.kind);
      if (training->training.teaching_agreement)
        specific.teaching_or_study_acceptance_ref =
            LifeOwnedFieldRef{owner(boundary), "training.teaching_agreement"};
      boundary.input_snapshot.accepted_permission_refs.push_back(
          {"training_terms", LifeOwnedFieldRef{owner(boundary), "training"},
           1});
      if (training->training.fee)
        boundary.input_snapshot.resource_basis_refs.push_back(
            {LifeResourceBasisRef::Purpose::fee,
             LifeOwnedFieldRef{owner(boundary), "training.fee"}, 1});
    } else if (const auto *study = std::get_if<StudyFact>(raw)) {
      if (study->phase != "started" ||
          study->start_tick != body.actual_now.half_day_tick ||
          !study->study_service_binding || !study->lessons.empty())
        fail("GrowthFixedActualStudyStartRequired", action.id);
      boundary.owner_ref.fact_kind = GrowthFixedFactKind::study;
      actor(boundary, study->terms.student_id, "student");
      actor(boundary, study->terms.teacher_id, "teacher");
      specific.original_training_or_study_rule_ref =
          field(body, study->terms.context.role_id,
                "courses." + study->terms.skill_id);
      specific.teaching_or_study_acceptance_ref = LifeOwnedFieldRef{
          owner(boundary), "study_service_binding.accepted_execution_ref"};
      boundary.input_snapshot.accepted_permission_refs.push_back(
          {"study_terms", LifeOwnedFieldRef{owner(boundary), "terms"}, 2});
      boundary.input_snapshot.resource_basis_refs.push_back(
          {LifeResourceBasisRef::Purpose::fee,
           LifeOwnedFieldRef{owner(boundary), "terms.fee"}, 2});
    } else if (const auto *rest = std::get_if<RestFact>(raw)) {
      if (!body.growth_catalog->complete_exact_rest_mouth_sources ||
          !body.rest_estates || !body.rest_estate_state ||
          !body.catalog.original_long_rest_rule)
        fail("GrowthLongRestActualStartSourcesRequired", action.id,
             ErrorKind::unsupported_capability);
      if (rest->phase != "active" ||
          rest->start_tick != body.actual_now.half_day_tick || rest->elapsed ||
          !rest->actual_periods.empty())
        fail("GrowthLongRestActualStartMismatch", action.id);
      const auto *estate =
          body.rest_estates->estates.estates.find(rest->estate_id);
      if (!estate)
        fail("GrowthLongRestOriginalEstateRequired", rest->estate_id);
      NativeEstateAssetTitleRead titles(*body.rest_estates,
                                        *body.rest_estate_state);
      auto title = take(titles.read_estate_title_before_source(
          rest->estate_id, {"services", "long_rest", action.id}));
      if (title.legal_owner.kind != Owner::Kind::actor ||
          title.legal_owner.id != rest->actor_id)
        fail("RestNeedsOwnEstate", rest->estate_id, ErrorKind::rule);
      boundary.owner_ref.fact_kind = GrowthFixedFactKind::rest;
      actor(boundary, rest->actor_id, "recipient");
      specific.original_training_or_study_rule_ref =
          field(body, rest->estate_id, "long_rest");
      specific.original_quiet_or_rest_basis =
          take(capture_growth_long_rest_quiet(body, rest->actor_id,
                                              estate->place_id));
      auto &quiet = *specific.original_quiet_or_rest_basis;
      if (!quiet.is_quiet)
        fail("RestNeedsQuiet", rest->actor_id, ErrorKind::rule);
      quiet.site_access_source_refs.emplace_back(title.initial_origin);
      for (auto &source : title.original_title_applications)
        quiet.site_access_source_refs.emplace_back(std::move(source));
      boundary.input_snapshot.accepted_permission_refs.push_back(
          {"long_rest_terms", LifeOwnedFieldRef{owner(boundary), "estate_id"},
           1});
      boundary.input_snapshot.resource_basis_refs.push_back(
          {LifeResourceBasisRef::Purpose::food,
           LifeOwnedFieldRef{owner(boundary), "ration_per_person"}, 1});
    } else
      fail("GrowthFixedActualStartOwnerRequired", action.id);
    boundary.input_snapshot.locked_rule_refs.push_back(
        specific.original_training_or_study_rule_ref);
  } else {
    const bool trade =
        action.kind == "trade" && life->view.kind == LifeActivityKind::trade;
    if (!trade && (action.kind != "service" ||
                   life->view.kind != LifeActivityKind::service))
      fail("GrowthFixedActualLifeStartRequired", action.id);
    if (life->view.phase != LifeActivityPhase::active ||
        life->view.start_tick != body.actual_now.half_day_tick)
      fail("GrowthFixedActualServiceStartMismatch", action.id);
    boundary.owner_ref.kind = 1;
    boundary.owner_ref.life_activity_id = action.id;
    boundary.owner_ref.terms_kind = trade ? 1 : 0;
    boundary.input_snapshot.specific.kind = 1;
    auto &specific = boundary.input_snapshot.specific.life_activity;
    specific.activity_id = action.id;
    specific.terms_kind = trade ? 1 : 0;
    actor(boundary, life->view.actor_id, "recipient");
    actor(boundary, life->view.provider_actor_id, "provider");
    if (trade) {
      if (!services.market_exact ||
          &services.market_exact->actor_foundation != &body.catalog ||
          life->selected_trade_settlement || life->trade_terminal_time ||
          action.remaining_ticks != trade_action_ticks)
        fail("GrowthTradeActualSettlementCaptureRequired", action.id,
             ErrorKind::unsupported_capability);
      const auto &market = *services.market_exact;
      const auto actual = take(read_growth_scalar_market_start_for_action_v2(
          body.common, *body.shared, *body.growth_catalog, growth,
          market.actor_foundation, market.history_catalog, market.history,
          action.id, body.actual_now));
      if (actual.activity != life || !actual.movement_id ||
          take(compare_world_moments(*actual.started_at, body.actual_now)))
        fail("GrowthTradeActualStartMismatch", action.id);
      specific.trade_settlement_ref =
          SourceRef{"growth_market_history", "growth_market_movement",
                    *actual.movement_id};
      // 内部暂定地址只锚原H1编译常量，不虚造店铺activity或第二结算。
      boundary.input_snapshot.locked_rule_refs.push_back(
          field(body, "regional_life_v1", "trade_action_ticks"));
      boundary.input_snapshot.accepted_permission_refs = {
          {"trade_terms", LifeOwnedFieldRef{owner(boundary), "terms"}, 1},
          {"provider_role", field(body, life->context.role_id, "role"), 1}};
      boundary.input_snapshot.resource_basis_refs = {
          {LifeResourceBasisRef::Purpose::fee, *specific.trade_settlement_ref,
           2},
          {LifeResourceBasisRef::Purpose::material,
           *specific.trade_settlement_ref, 2}};
    } else {
      specific.service_role_ref = field(body, life->context.role_id, "role");
      specific.activity_ref = field(body, life->context.role_id,
                                    "activities." + life->view.activity);
      boundary.input_snapshot.locked_rule_refs.push_back(
          *specific.activity_ref);
      boundary.input_snapshot.accepted_permission_refs = {
          {"service_terms", LifeOwnedFieldRef{owner(boundary), "terms"}, 1},
          {"provider_role", *specific.service_role_ref, 1}};
      boundary.input_snapshot.resource_basis_refs = {
          {LifeResourceBasisRef::Purpose::fee,
           LifeOwnedFieldRef{owner(boundary), "cost_paid"}, 1},
          {LifeResourceBasisRef::Purpose::wage_reservation,
           LifeOwnedFieldRef{owner(boundary), "terms.wage"}, 1}};
    }
  }
  std::sort(
      boundary.input_snapshot.actors.begin(),
      boundary.input_snapshot.actors.end(), [](const auto &a, const auto &b) {
        return std::tie(a.actor_id, a.role) < std::tie(b.actor_id, b.role);
      });
}
void capture(const CharacterConditionSources &body, GrowthState &growth,
             const GrowthFixedCaptureServices &services, const StableId &id) {
  auto *boundary = growth.fixed_service_boundaries.find(id);
  if (!boundary)
    fail("GrowthFixedActualBoundaryRequired", id);


  auto memory = CheckpointMemoryCharge(0);
  std::vector<ResourceKey> resources;
  std::vector<DomainReadKey> domains;
  std::vector<StableId> source_ids{id, boundary->action_ref};
  resources.reserve(boundary->input_snapshot.actors.size());
  domains.reserve(boundary->input_snapshot.actors.size());
  for (const auto &actor : boundary->input_snapshot.actors) {
    if (!resources.empty() && resources.back().id == actor.actor_id)
      continue;
    resources.push_back({ResourceKey::Kind::actor, actor.actor_id});
    domains.push_back({"actor_foundation", "condition_head", actor.actor_id});
  }
  auto stamp = take(services.reads.capture(resources, domains, source_ids));
  if (!stamp)
    fail("GrowthFixedActualReadStampRequired", id);
  for (auto &input : boundary->input_snapshot.actors) {
    ActorFoundationRuleRef purpose;
    std::optional<StableId> skill;
    if (boundary->owner_ref.kind == 0 &&
        boundary->owner_ref.fact_kind == GrowthFixedFactKind::study) {
      const auto &fact =
          std::get<StudyFact>(*growth.facts.find(boundary->action_ref));
      purpose = {input.role == "student" ? "study" : "provide_teaching", 1,
                 body.catalog.identity.content_digest};
      skill = fact.terms.skill_id;
    } else if (boundary->owner_ref.kind == 0 &&
               boundary->owner_ref.fact_kind == GrowthFixedFactKind::training)
      purpose = {"personal_training_participant", 1,
                 body.catalog.identity.content_digest};
    else if (boundary->owner_ref.kind == 0 &&
             boundary->owner_ref.fact_kind == GrowthFixedFactKind::rest)
      purpose = {"long_rest", 1, body.catalog.identity.content_digest};
    else {
      const auto &activity = *growth.life_activities.find(boundary->action_ref);
      if (activity.view.kind == LifeActivityKind::trade)
        purpose = {"growth_trade_participant", 1,
                   body.catalog.identity.content_digest};
      else if (activity.view.activity == "rest")
        purpose = input.role == "recipient" ? body.catalog.rest_purpose
                                            : body.catalog.provider_purpose;
      else if (activity.view.activity == "work")
        purpose =
            input.role == "recipient"
                ? ActorFoundationRuleRef{"growth_service_worker", 1,
                                         body.catalog.identity.content_digest}
                : body.catalog.provider_purpose;
      else
        fail("GrowthOriginalServicePurposeRequired", id,
             ErrorKind::unsupported_capability);
      if (activity.view.kind == LifeActivityKind::service &&
          services.original.personal_recovery)
        fail("GrowthExactRecoveryServiceCaptureRequired", id,
             ErrorKind::unsupported_capability);
    }
    auto value = take(
        read_character_capability(body, input.actor_id, purpose, skill, *stamp,
                                  services.authority, services.versions));
    if (!value.allowed)
      fail("GrowthFixedOriginalCapabilityDenied", input.actor_id,
           ErrorKind::rule);
    input.capability_frames.push_back(take(freeze_character_capability(value)));
  }
  take(capture_growth_fixed_skill_inputs_in_candidate(
      body, growth, id, *stamp, services.authority, services.versions));
  take(capture_growth_fixed_spatial_inputs_in_candidate(
      body, *body.fixed_spatial, growth, id, *stamp, services.authority,
      services.versions));
}
} // namespace
Status validate_growth_dialogue_confirmation(
    const CommonStore &common, const GrowthState &growth,
    const InteractionState *interaction,
    const InteractionCatalog *interaction_catalog, const StableId &result_id,
    Integer request_seq, const StableId &actor_id, const StableId &kind,
    const StableId &action_id, const WorldMoment &started_at) {
  try {
    if (!interaction || !interaction_catalog)
      fail("GrowthOriginalDialogueConfirmationSourcesRequired", action_id,
           ErrorKind::unsupported_capability);
    auto actual =
        take(read_common_command_source(common, std::string_view(result_id)));
    if (actual.result->kind != "dialogue_confirm" ||
        actual.history->actor_id != actor_id ||
        actual.request_seq != request_seq)
      fail("GrowthOriginalDialogueConfirmationSealMismatch", action_id);
    auto confirmed = take(read_original_interaction_action_confirmation(
        common, *interaction, *interaction_catalog, result_id, action_id));
    const auto &source = *confirmed.source;
    const auto &command = source.confirmed_command;
    const auto &opening = *confirmed.opening->session_opening;
    if (command.kind != kind || opening.actor_id != actor_id ||
        take(compare_world_moments(source.confirmation_at, started_at)))
      fail("GrowthOriginalDialogueConfirmationMismatch", action_id);
    // These are precisely the original Growth kinds representable by the saved
    // DialogueCommand. Other kinds require their own real source, never an
    // alias.
    if (kind == "service") {
      const auto *row = growth.life_activities.find(action_id);
      if (!row || row->view.kind != LifeActivityKind::service ||
          row->view.actor_id != actor_id || row->view.action_id != action_id ||
          row->view.activity != command.activity ||
          row->view.start_tick != started_at.half_day_tick ||
          !growth::context_equal(row->context, opening.context))
        fail("GrowthConfirmedOriginalServiceTermsMismatch", action_id);
    } else if (kind == "learn_skill" || kind == "teach_skill") {
      const auto *raw = growth.facts.find(action_id);
      const auto *row = raw ? std::get_if<TrainingFact>(raw) : nullptr;
      if (!row || row->action_kind != kind || row->actor_id != actor_id ||
          row->training.skill_id != command.skill_id ||
          row->start_tick != started_at.half_day_tick || !row->context ||
          !growth::context_equal(*row->context, opening.context))
        fail("GrowthConfirmedOriginalTrainingTermsMismatch", action_id);
    } else
      fail("GrowthOriginalDialogueKindSourceRequired", action_id,
           ErrorKind::unsupported_capability);
    // While the original Action exists, also join its actual typed request. On
    // terminal read the immutable TrainingFact/LifeActivity above is the
    // source.
    if (const auto *action = growth.actions.find(action_id)) {
      const auto &request = action->request;
      if (request.kind != kind || !request.context ||
          !growth::context_equal(*request.context, opening.context) ||
          (kind == "service" ? request.activity != command.activity
                             : request.skill_id != command.skill_id))
        fail("GrowthConfirmedOriginalRequestMismatch", action_id);
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<StableId> bind_growth_fixed_started_command_in_candidate(
    const GrowthState &before, CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const GrowthFixedCaptureServices &services, const StableId &actor_id,
    Integer sequence, const CommitResult &result) {
  try {
    if (&body.common != &common || body.growth != &growth ||
        body.elapsed != &elapsed || !body.growth_catalog || !body.shared ||
        !body.scale || !body.action_registrations || !body.fixed_spatial ||
        !body.control || !body.growth_catalog->complete_exact_period_sources)
      fail("GrowthCompleteStartSourcesRequired", {},
           ErrorKind::unsupported_capability);
    {
      auto ready = validate_growth_start_participation_writer(
          common, growth, body.catalog, services.participation);
      if (!ready.ok)
        throw Failure{std::move(ready.error)};
    }
    auto participation = [&](const LifeFixedServiceBoundary &started) {
      if (!growth_start_participation_consumer_selected(body.catalog))
        return;
      auto consumed = services.participation->consume_actual_growth_start(
          {owner(started), 1}, body.actual_now);
      if (!consumed.ok)
        throw Failure{std::move(consumed.error)};
    };
    const auto *cached =
        common.state().results.find(actor_id + ":" + std::to_string(sequence));
    const auto *seq = common.state().request_sequences.find(actor_id);
    if (!cached || cached->get() != &result || !seq || *seq != sequence ||
        !result.ok || !result.details || result.revision != common.revision() ||
        result.tick != body.actual_now.half_day_tick ||
        common.state().history.empty())
      fail("GrowthActualStartCommitRequired", result.result_id);
    const auto &history = common.state().history.back();
    if (history.actor_id != actor_id || history.result_id != result.result_id ||
        history.kind != result.kind || history.tick != result.tick)
      fail("GrowthActualStartCommitHistoryRequired", result.result_id);
    const auto *started = std::get_if<ActionStarted>(&result.details->data);
    const auto *action = started ? common.action(started->action_id) : nullptr;
    if (!action || action->actor_id != actor_id ||
        before.actions.find(action->id) || before.facts.find(action->id) ||
        before.life_activities.find(action->id))
      fail("GrowthOriginalNewActionStartRequired", result.result_id);
    if (action->kind != result.kind)
      take(validate_growth_dialogue_confirmation(
          common, growth, body.interaction, body.interaction_catalog,
          result.result_id, sequence, actor_id, action->kind, action->id,
          body.actual_now));
    const auto &reg = registration(body, action->kind);
    for (const auto &row : growth.fixed_service_boundaries.rows())
      if (row.second.action_ref == action->id) {
        const auto *command = std::get_if<LifeActualCommand>(&row.second.cause);
        if (row.second.kind != LifeFixedBoundaryKind::started || !command ||
            command->actor_id != actor_id ||
            command->command_binding.result_id != result.result_id ||
            command->command_binding.request_seq != sequence)
          fail("GrowthConflictingActualStart", action->id);
        auto adopted = take(adopt_growth_fixed_boundary_in_candidate(
            common, growth, *body.growth_catalog, *body.shared, elapsed,
            *body.scale, body.catalog.identity, reg, row.first, body.actual_now,
            {}, body.interaction, body.interaction_catalog,
            services.market_exact));
        participation(row.second);
        return {true, std::move(adopted), {}};
      }
    LifeFixedServiceBoundary boundary;
    original_inputs(body, growth, *action, boundary, services);
    boundary.boundary_id =
        take(common.next_id("growth_fixed_service_boundary"));
    const auto id = boundary.boundary_id;
    LifeActualCommand command;
    command.actor_id = actor_id;
    command.command_binding = {result.kind, sequence, result.result_id};
    command.accepted_business_ref = owner(boundary);
    boundary.cause = std::move(command);
    if (!growth.fixed_service_boundaries.insert(id, std::move(boundary)))
      fail("DuplicateGrowthFixedBoundary", id);
    auto *actual = growth.fixed_service_boundaries.find(id);
    std::get<LifeActualCommand>(actual->cause).route_basis =
        take(capture_growth_actor_route(
            body, growth, id, std::get<LifeActualCommand>(actual->cause)));
    capture(body, growth, services, id);
    if (growth::study_action_kind(action->kind))
      take(bind_study_time_after_actual_start(
          common, growth, *body.growth_catalog, body.catalog, id,
          body.actual_now));
    take(adopt_growth_fixed_boundary_in_candidate(
        common, growth, *body.growth_catalog, *body.shared, elapsed,
        *body.scale, body.catalog.identity, reg, id, body.actual_now, {},
        body.interaction, body.interaction_catalog, services.market_exact));
    participation(*growth.fixed_service_boundaries.find(id));
    return {true, id, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<MomentOwnerSource> read_growth_started_business_source(
    const GrowthState &before, const GrowthState &after,
    const CommonStore &common, const ActionCoreView &action,
    const WorldMoment &at) {
  try {
    take(compare_world_moments(at, at));
    const auto *active = common.action(action.id);
    const auto *payload = after.actions.find(action.id);
    if (!active || active->actor_id != action.actor_id ||
        active->kind != action.kind ||
        active->start_tick != action.start_tick ||
        active->remaining_ticks != action.remaining_ticks ||
        active->participants != action.participants || !payload ||
        before.actions.find(action.id) || before.facts.find(action.id) ||
        before.life_activities.find(action.id) || action.actor_id.empty() ||
        !common.actor(action.actor_id) ||
        action.start_tick != at.half_day_tick ||
        payload->request.kind != action.kind || payload->quote.duration < 1 ||
        payload->quote.duration != action.remaining_ticks)
      fail("GrowthOriginalNewActionStartRequired", action.id);
    const auto *raw = after.facts.find(action.id);
    const auto *life = after.life_activities.find(action.id);
    if (bool(raw) == bool(life))
      fail("GrowthFixedActualStartOwnerRequired", action.id);
    const char *domain = nullptr;
    const char *kind = nullptr;
    const char *result_kind = nullptr;
    if (raw) {
      if (const auto *f = std::get_if<TrainingFact>(raw)) {
        if ((action.kind != "learn_skill" && action.kind != "practice_skill" &&
             action.kind != "teach_skill") ||
            f->phase != "started" || f->action_kind != action.kind ||
            f->actor_id != action.actor_id ||
            f->start_tick != at.half_day_tick || f->completed_tick != -1 ||
            f->success || f->training.duration != payload->quote.duration)
          fail("GrowthFixedActualTrainingStartRequired", action.id);
        domain = "cultivation";
        kind = "training";
        result_kind = "training";
      } else if (const auto *f = std::get_if<StudyFact>(raw)) {
        if (!growth::study_action_kind(action.kind) || f->phase != "started" ||
            growth::study_initiator(f->terms) != action.actor_id ||
            f->start_tick != at.half_day_tick || f->completed_tick != -1 ||
            f->success || !f->lessons.empty() || !f->study_service_binding ||
            f->terms.duration != payload->quote.duration)
          fail("GrowthFixedActualStudyStartRequired", action.id);
        domain = "cultivation";
        kind = "skill_study";
        result_kind = "skill_study";
      } else if (const auto *f = std::get_if<RestFact>(raw)) {
        if (action.kind != "long_rest" || f->phase != "active" ||
            f->actor_id != action.actor_id ||
            f->start_tick != at.half_day_tick || f->completed_tick != -1 ||
            f->elapsed != 0 || f->consumed != 0 || !f->actual_periods.empty() ||
            f->duration != payload->quote.duration)
          fail("GrowthFixedActualLongRestStartRequired", action.id);
        domain = "services";
        kind = "long_rest";
        result_kind = "long_rest";
      } else
        fail("GrowthFixedActualStartOwnerRequired", action.id);
    } else {
      const auto &v = life->view;
      if ((action.kind != "service" && action.kind != "trade") ||
          v.phase != LifeActivityPhase::active || v.fact_id != action.id ||
          v.action_id != action.id || v.actor_id != action.actor_id ||
          v.start_tick != at.half_day_tick || v.completed_tick != -1 ||
          v.success || v.participant_actor_ids != action.participants ||
          (action.kind == "service") != (v.kind == LifeActivityKind::service))
        fail("GrowthFixedActualLifeStartRequired", action.id);
      domain = "growth";
      kind = "life_activity";
      result_kind = "life_activity";
    }
    const auto *header = common.state().facts.find(action.id);
    if (!header || header->id != action.id || header->domain != domain ||
        header->kind != kind)
      fail("GrowthOriginalStartHeaderRequired", action.id);
    // Same original owner namespace already used by the full FSL reader.
    // Common's publication domain is checked above, and is not mistaken for
    // this payload ABI.
    return {true, {{"growth", result_kind, action.id}, 1}, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<GrowthStartedBusinessIdentity> read_growth_started_business_identity(
    const CommonStore &common, const GrowthState &growth,
    const MomentOwnerSource &source, const WorldMoment &now,
    const InteractionState *interaction,
    const InteractionCatalog *interaction_catalog) {
  try {
    const auto &ref = source.source_ref;
    take(compare_world_moments(now, now));
    if (source.source_schema != 1 || ref.domain != "growth" || ref.id.empty())
      fail("GrowthStartedIdentitySourceRequired", ref.id);
    const auto *action = growth.actions.find(ref.id);
    const auto *raw = growth.facts.find(ref.id);
    const auto *life = growth.life_activities.find(ref.id);
    if (bool(raw) == bool(life) ||
        (action && (action->quote.duration < 1 ||
                    action->quote.command.kind != action->request.kind)))
      fail("GrowthStartedIdentityOriginalActionRequired", ref.id);
    const LifeFixedServiceBoundary *started = nullptr;
    for (const auto &row : growth.fixed_service_boundaries.rows())
      if (row.second.action_ref == ref.id &&
          row.second.kind == LifeFixedBoundaryKind::started) {
        if (started || row.first != row.second.boundary_id)
          fail("GrowthStartedIdentityDuplicateStart", ref.id);
        started = &row.second;
      }
    if (!started)
      fail("GrowthStartedIdentityExactStartRequired", ref.id);
    const StableId *actor_id = nullptr;
    const StableId *kind = &started->action_kind;
    bool terminal = false;
    Integer start_tick = -1;
    const char *header_domain = nullptr;
    const char *header_kind = nullptr;
    GrowthFixedFactKind fact_kind = GrowthFixedFactKind::training;
    if (raw) {
      if (const auto *row = std::get_if<TrainingFact>(raw)) {
        if (ref.kind != "training" ||
            (row->phase != "started" && row->phase != "completed") ||
            row->action_kind != *kind ||
            (*kind != "learn_skill" && *kind != "practice_skill" &&
             *kind != "teach_skill") ||
            (action && row->training.duration != action->quote.duration) ||
            row->actor_id != (row->action_kind == "teach_skill"
                                  ? row->training.teacher_id
                                  : row->training.student_id))
          fail("GrowthStartedIdentityTrainingMismatch", ref.id);
        actor_id = &row->actor_id;
        terminal = row->phase == "completed";
        start_tick = row->start_tick;
        header_domain = "cultivation";
        header_kind = "training";
      } else if (const auto *row = std::get_if<StudyFact>(raw)) {
        if (ref.kind != "skill_study" ||
            (row->phase != "started" && row->phase != "completed") ||
            !growth::study_action_kind(*kind) ||
            (*kind == "teach_study") !=
                row->terms.study_teaching_agreement.has_value() ||
            row->actor_id != row->terms.student_id ||
            (action && row->terms.duration != action->quote.duration) ||
            !row->study_service_binding)
          fail("GrowthStartedIdentityStudyMismatch", ref.id);
        actor_id = row->terms.study_teaching_agreement ? &row->terms.teacher_id
                                                       : &row->terms.student_id;
        terminal = row->phase == "completed";
        start_tick = row->start_tick;
        header_domain = "cultivation";
        header_kind = "skill_study";
        fact_kind = GrowthFixedFactKind::study;
      } else if (const auto *row = std::get_if<RestFact>(raw)) {
        if (ref.kind != "long_rest" || *kind != "long_rest" ||
            (row->phase != "active" && row->phase != "completed") ||
            (action && row->duration != action->quote.duration) ||
            row->estate_id.empty())
          fail("GrowthStartedIdentityRestMismatch", ref.id);
        actor_id = &row->actor_id;
        terminal = row->phase == "completed";
        start_tick = row->start_tick;
        header_domain = "services";
        header_kind = "long_rest";
        fact_kind = GrowthFixedFactKind::rest;
      } else
        fail("GrowthStartedIdentityOriginalPayloadRequired", ref.id);
    } else {
      const auto &row = life->view;
      if (ref.kind != "life_activity" || row.fact_id != ref.id ||
          row.action_id != ref.id || (*kind != "service" && *kind != "trade") ||
          (*kind == "service") != (row.kind == LifeActivityKind::service) ||
          static_cast<unsigned>(row.phase) > 2)
        fail("GrowthStartedIdentityLifeMismatch", ref.id);
      actor_id = &row.actor_id;
      terminal = row.phase != LifeActivityPhase::active;
      start_tick = row.start_tick;
      header_domain = "growth";
      header_kind = "life_activity";
    }
    const auto *header = common.state().facts.find(ref.id);
    if (!actor_id || actor_id->empty() || !common.actor(*actor_id) ||
        start_tick < 0 || !header || header->id != ref.id ||
        header->domain != header_domain || header->kind != header_kind)
      fail("GrowthStartedIdentityOriginalHeaderRequired", ref.id);

    if (!started || started->source_schema != 1 || started->boundary_ordinal ||
        started->previous_boundary_ref || started->original_outcome_ref ||
        started->action_kind != *kind ||
        started->occurred_at.half_day_tick != start_tick ||
        take(compare_world_moments(started->occurred_at, now)) > 0)
      fail("GrowthStartedIdentityExactStartRequired", ref.id);
    if (life) {
      if (started->owner_ref.kind != 1 ||
          started->owner_ref.life_activity_id != ref.id ||
          !started->owner_ref.action_id.empty() ||
          started->owner_ref.terms_kind != (*kind == "trade" ? 1 : 0))
        fail("GrowthStartedIdentityOwnerMismatch", ref.id);
    } else if (started->owner_ref.kind != 0 ||
               started->owner_ref.action_id != ref.id ||
               started->owner_ref.fact_kind != fact_kind ||
               !started->owner_ref.life_activity_id.empty() ||
               started->owner_ref.terms_kind)
      fail("GrowthStartedIdentityOwnerMismatch", ref.id);
    const auto *command = std::get_if<LifeActualCommand>(&started->cause);
    const auto *accepted =
        command ? std::get_if<SourceRef>(&command->accepted_business_ref)
                : nullptr;
    if (!command || !accepted || command->actor_id != *actor_id ||
        command->command_binding.request_seq < 0 ||
        command->command_binding.result_id.empty() ||
        accepted->domain != ref.domain || accepted->kind != ref.kind ||
        accepted->id != ref.id)
      fail("GrowthStartedIdentityActualCommandRequired", ref.id);
    if (command->command_binding.command_kind != *kind) {
      if (command->command_binding.command_kind != "dialogue_confirm")
        fail("GrowthStartedIdentityActualCommandRequired", ref.id);
      take(validate_growth_dialogue_confirmation(
          common, growth, interaction, interaction_catalog,
          command->command_binding.result_id,
          command->command_binding.request_seq, *actor_id, *kind, ref.id,
          started->occurred_at));
    }
    const auto *active = common.action(ref.id);
    if (terminal) {
      if (active || action)
        fail("GrowthStartedIdentityTerminalStillActive", ref.id);
    } else if (!active || !action || active->actor_id != *actor_id ||
               active->kind != *kind || active->start_tick != start_tick ||
               action->request.kind != *kind)
      fail("GrowthStartedIdentityCurrentActionMismatch", ref.id);
    return {true, {actor_id, kind, started, command, action}, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
