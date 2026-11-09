#include "growth_exact_phase.hpp"
#include "growth_capability_chain.hpp"
#include "growth_trade_grant_inputs.hpp"
#include "growth_fixed_input_changes.hpp"
#include "growth_fixed_terminal.hpp"
#include "growth_internal.hpp"
#include "growth_training_fixed.hpp"
#include "profile_acquisition_world.hpp"
#include "resident/domains/actor_foundation/long_rest_condition.hpp"
#include "resident/domains/actor_foundation/service_condition.hpp"
#include "resident/domains/social/social_relation_execution.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
template <class T, class F> Outcome<T> checked(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status selected(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->growth || !w.catalog->common ||
      !w.catalog->actor_foundation || !w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time) || !w.actor_foundation ||
      !w.action_elapsed || !w.world_timeline ||
      !w.catalog->growth->complete_exact_period_sources ||
      !w.catalog->growth->complete_study_sources)
    return common_failure("CompleteGrowthExactSelectionRequired");
  return common_success();
}
std::optional<GrowthMarketExactSources> market_sources(const WorldBundle &w) {
  if (!w.catalog->growth_market_history)
    return {};
  return GrowthMarketExactSources{*w.catalog->actor_foundation,
                                  *w.catalog->growth_market_history,
                                  w.growth_market_history};
}
const ActionTimeRegistration &
registration(const CharacterConditionSources &body, const StableId &kind) {
  if (!body.action_registrations)
    fail("GrowthExactActionRegistrationsRequired");
  const ActionTimeRegistration *selected = nullptr;
  for (const auto &r : *body.action_registrations)
    if (r.owner_domain == "growth" && r.action_kind == kind) {
      if (selected)
        fail("DuplicateGrowthFixedRegistration", kind);
      selected = &r;
    }
  if (!selected)
    fail("GrowthFixedRegistrationMissing", kind);
  return *selected;
}
const LifeFixedServiceBoundary &latest(const GrowthState &state,
                                       const StableId &action) {
  const LifeFixedServiceBoundary *result = nullptr;
  for (const auto &row : state.fixed_service_boundaries.rows())
    if (row.second.action_ref == action &&
        (!result || row.second.boundary_ordinal > result->boundary_ordinal))
      result = &row.second;
  if (!result)
    fail("GrowthExactOriginalStartRequired", action);
  return *result;
}
Status world_sources(const WorldBundle &w, const WorldMoment &current) {
  return checked<Done>([&] {
    take(selected(w));
    auto lease = take(w.complete_exact_time_bindings());
    auto body = take(lease->reads().character_conditions.sources());
    auto market = market_sources(w);
    if (take(compare_world_moments(body.actual_now, current)))
      fail("GrowthExactSourceClockMismatch");
    take(validate_complete_growth_fixed_sources(body, w.growth,
                                                market ? &*market : nullptr));
    return Done{};
  });
}
Outcome<std::vector<ExactEventPrediction>> predict(const WorldBundle &w,
                                                   const WorldMoment &from,
                                                   const WorldMoment &through) {
  return checked<std::vector<ExactEventPrediction>>([&] {
    take(selected(w));
    auto lease = take(w.complete_exact_time_bindings());
    auto body = take(lease->reads().character_conditions.sources());
    auto market = market_sources(w);
    if (take(compare_world_moments(body.actual_now, from)))
      fail("GrowthExactPredictionClockMismatch");
    std::vector<ExactEventPrediction> out;
    for (const auto &row : w.growth.actions.rows()) {
      const auto *action = w.common.action(row.first);
      if (!action)
        fail("GrowthExactOriginalActionMissing", row.first);
      const auto &reg = registration(body, action->kind);
      auto due = take(predict_growth_fixed_step(
          w.common, w.growth, *w.catalog->growth, *w.catalog->common,
          *w.action_elapsed, w.world_timeline->scale_ref, w.catalog->identity,
          reg, row.first, from, {}, body.interaction, body.interaction_catalog,
          market ? &*market : nullptr));
      if (!due)
        continue;
      if (take(compare_world_moments(*due, from)) < 0)
        fail("GrowthExactOriginalGateOverdue", row.first);
      if (take(compare_world_moments(*due, through)) > 0)
        continue;
      const auto &source = latest(w.growth, row.first);
      out.push_back(
          {*due,
           ExactStage::action_completions,
           {"growth", "growth_fixed_service_boundary", source.boundary_id},
           1,
           source.boundary_ordinal});
    }
    return out;
  });
}
Status interval(WorldBundle &w, const MomentSpan &span,
                ExactPhaseWriteContext &) {
  return checked<Done>([&] {
    take(selected(w));
    auto lease = take(w.complete_exact_time_bindings());
    auto body = take(lease->reads().character_conditions.sources());
    auto market = market_sources(w);
    take(provide_growth_fixed_interval_in_candidate(
        w.common, w.growth, *w.catalog->growth, *w.catalog->common,
        *w.action_elapsed, w.world_timeline->scale_ref, w.catalog->identity,
        *body.action_registrations, span, {}, body.interaction,
        body.interaction_catalog, market ? &*market : nullptr));
    return Done{};
  });
}
Outcome<ExactEventApplied> apply(WorldBundle &w,
                                 const ExactEventPrediction &pending,
                                 ExactPhaseWriteContext &context) {
  return checked<ExactEventApplied>([&] {
    take(selected(w));
    if (pending.stage != ExactStage::action_completions ||
        pending.source_schema != 1 ||
        pending.actual_source_basis.domain != "growth" ||
        pending.actual_source_basis.kind != "growth_fixed_service_boundary")
      fail("GrowthExactDueSourceMismatch", pending.actual_source_basis.id);
    auto lease = take(w.complete_exact_time_bindings());
    auto body = take(lease->reads().character_conditions.sources());
    auto market = market_sources(w);
    if (take(compare_world_moments(body.actual_now, pending.at)))
      fail("GrowthExactDueClockMismatch");
    const auto *basis =
        w.growth.fixed_service_boundaries.find(pending.actual_source_basis.id);
    if (!basis || basis->boundary_ordinal != pending.dependency_revision ||
        latest(w.growth, basis->action_ref).boundary_id != basis->boundary_id)
      fail("GrowthExactDueBasisChanged", pending.actual_source_basis.id);
    const StableId id = basis->action_ref;
    const auto *action = w.common.action(id);
    if (!action)
      fail("GrowthExactDueActionMissing", id);
    const auto &reg = registration(body, action->kind);
    auto gate = take(prepare_growth_fixed_step(
        w.common, w.growth, *w.catalog->growth, *w.catalog->common,
        *w.action_elapsed, w.world_timeline->scale_ref, w.catalog->identity,
        reg, id, pending.at, {}, body.interaction, body.interaction_catalog,
        market ? &*market : nullptr));
    auto captured = take(prepare_growth_fixed_terminal_inputs(
        w.common, w.growth, body, gate, market ? &*market : nullptr));
    auto original = w.services.growth_access();
    original.original_service_relation_writer =
        lease->original_growth_service_relation_writer();
    GrowthFixedStudyApplication applied;
    if (growth::study_action_kind(action->kind))
      applied = take(apply_study_fixed_gate_in_candidate(
          w.common, w.growth, *w.actor_foundation, body, *w.catalog->growth,
          *w.catalog->common, w.social, w.interaction, original, reg, gate));
    else if (action->kind == "learn_skill" ||
             action->kind == "practice_skill" || action->kind == "teach_skill")
      applied = take(apply_training_fixed_gate_in_candidate(
          w.common, w.growth, *w.actor_foundation, body, *w.catalog->growth,
          *w.catalog->common, w.social, w.interaction, original, reg, gate));
    else if (action->kind == "service") {
      const auto *service = w.growth.life_activities.find(id);
      if (body.injury_sources && service && service->view.activity == "rest") {
        if (!w.character_injury_sources ||
            body.injury_sources != &*w.character_injury_sources)
          fail("InjuryRestSameCandidateRequired", id);
        applied = take(apply_injury_rest_fixed_gate_in_candidate(
            w.common, w.growth, *w.actor_foundation,
            *w.character_injury_sources, body, *w.catalog->growth,
            *w.catalog->common, w.social, w.interaction, original, reg, gate));
      } else
        applied = take(apply_service_fixed_gate_in_candidate(
            w.common, w.growth, *w.actor_foundation, body, *w.catalog->growth,
            *w.catalog->common, w.social, w.interaction, original, reg, gate));
    } else if (action->kind == "long_rest")
      applied = take(apply_long_rest_fixed_gate_in_candidate(
          w.common, w.growth, *w.actor_foundation, body, *w.catalog->growth,
          *w.catalog->common, original, w.estate, reg, gate));
    else if (action->kind == "trade")
      applied.result = take(step_growth_at_fixed_gate(
          w.common, w.growth, *w.catalog->growth, *w.catalog->common, w.social,
          w.interaction, original, *action, gate));
    else
      fail("GrowthExactOriginalKindRequired", action->kind);
    StableId adopted;
    if (applied.result.completed) {
      adopted = take(complete_growth_fixed_terminal_in_candidate(
          w.common, w.growth, body, *w.action_elapsed, reg, applied.result,
          captured,
          [&w](const StableId &action) { return release_action(w, action); }));
      ActionCompletion result;
      result.growth = std::move(applied.result.effect);
      if (!context.advance.completed_actions.insert(id, std::move(result)))
        fail("DuplicateGrowthExactCompletion", id);
    } else {
      adopted = take(adopt_growth_fixed_period_inputs_in_candidate(
          w.common, w.growth, body, *w.action_elapsed, reg, gate, applied,
          captured));
      if (const auto *raw = w.growth.facts.find(id))
        if (const auto *fact = std::get_if<StudyFact>(raw);
            fact && growth::study_paused(*fact))
          for (const auto &actor : growth::study_participants(fact->terms))
            context.advance.suspended_actors.push_back(actor);
    }
    if (w.catalog->growth->cards.enabled)
      if (const auto *raw = w.growth.facts.find(id))
        if (const auto *training = std::get_if<TrainingFact>(raw);
            training && training->phase == "completed" && training->success &&
            (training->action_kind == "learn_skill" ||
             training->action_kind == "teach_skill"))
          take(record_growth_training_card_acquisitions_in_candidate(w, id));
    if (w.catalog->growth->complete_capability_application_chain) {
      auto *capability = lease->growth_capability_writer();
      if (!capability)
        fail("GrowthCapabilityActualWriterRequired", id);
      take(record_growth_fixed_capability_result_in_candidate(
          body, w.growth, gate, applied, *capability));
      if (reg.action_kind == "trade" && applied.result.completed)
        take(adopt_growth_trade_grant_inputs_in_candidate(w.common, w.growth, body, *w.action_elapsed, GrowthTradeGrantRef{id}));
    }
    if (applied.result.completed)
      if (w.social.original_relation_version == 2 &&
          reg.action_kind == "service") {
        const auto *service = w.growth.life_activities.find(id);
        if (service && service->view.activity == "work" &&
            service->view.success) {
          if (!original.original_service_relation_writer ||
              !original.original_service_relation_writer->bound_to(
                  w.common, w.social, w.growth))
            throw Failure{
                common_failure("OriginalGrowthServiceRelationWriterRequired",
                               id, ErrorKind::unsupported_capability)
                    .error};
          take(original.original_service_relation_writer
                   ->apply_completed_work_service(
                       {"growth", "growth_fixed_service_boundary", adopted}, 1,
                       pending.at));
        }
      }
    for (auto &binding : applied.actual_condition_writes)
      context.condition_writes.push_back(std::move(binding));
    return ExactEventApplied{
        {"growth", "growth_fixed_service_boundary", std::move(adopted)}, 1};
  });
}
Status applied_source(const WorldBundle &w, const ExactEventApplied &applied) {
  return checked<Done>([&] {
    take(selected(w));
    if (applied.source_schema != 1 ||
        applied.applied_result.domain != "growth" ||
        applied.applied_result.kind != "growth_fixed_service_boundary")
      fail("GrowthExactAppliedSourceMismatch", applied.applied_result.id);
    const auto *source =
        w.growth.fixed_service_boundaries.find(applied.applied_result.id);
    if (!source || source->source_schema != 1 ||
        source->kind == LifeFixedBoundaryKind::started)
      fail("GrowthExactAppliedOriginalBoundaryRequired",
           applied.applied_result.id);
    auto lease = take(w.complete_exact_time_bindings());
    auto body = take(lease->reads().character_conditions.sources());
    auto market = market_sources(w);
    GrowthActionTimeRead timing(
        w.common, w.growth, *w.catalog->growth, *w.catalog->common,
        *w.action_elapsed, w.world_timeline->scale_ref, w.catalog->identity,
        body.actual_now, {}, body.interaction, body.interaction_catalog,
        market ? &*market : nullptr);
    const auto &reg = registration(body, source->action_kind);
    auto proof = take(timing.read_action_time_owner(reg, source->action_ref));
    const auto *elapsed =
        find_fixed_elapsed(*w.action_elapsed, source->action_ref);
    if (!elapsed)
      fail("GrowthExactAppliedElapsedMissing", source->action_ref);
    take(timing.validate_retained_fixed_action_sources(reg, proof, *elapsed));
    return Done{};
  });
}
} // namespace
ExactPhaseRegistration make_growth_exact_phase_registration() {
  ExactPhaseRegistration r;
  r.provider = ExactProviderId::growth;
  r.profile_id = "growth_fixed_service_time2";
  r.version = 1;
  r.stages = {ExactStage::interval_motion_work, ExactStage::action_completions};
  r.interval_writer = true;
  r.effect_writer_keys = {"growth_fixed_action_progress",
                          "growth_original_training_study_service_results",
                          "growth_original_long_rest_period"};
  r.validate_author_selection = selected;
  r.validate_world_sources = world_sources;
  r.predict = predict;
  r.provide_interval = interval;
  r.apply_due = apply;
  r.validate_applied_source = applied_source;
  return r;
}
} // namespace hanmo::resident
