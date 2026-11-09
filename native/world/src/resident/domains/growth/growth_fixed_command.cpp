#include "growth_fixed_command.hpp"
#include "growth_equal.hpp"
#include "growth_fixed_input_changes.hpp"
#include "growth_internal.hpp"
#include "growth_quiet_source.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/command_types.hpp"
#include "study_permanent_contribution_read.hpp"
#include <limits>
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
bool same(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool same(const ActorConditionHead &a, const ActorConditionHead &b) {
  return a.actor_id == b.actor_id &&
         a.condition_revision == b.condition_revision &&
         a.initial_condition_seed_ref.seed_key ==
             b.initial_condition_seed_ref.seed_key &&
         bool(a.latest_receipt_ref) == bool(b.latest_receipt_ref) &&
         (!a.latest_receipt_ref ||
          a.latest_receipt_ref->result_id == b.latest_receipt_ref->result_id);
}
bool selected(const StableId &kind) {
  return kind == "study_pause" || kind == "study_resume" ||
         kind == "study_cancel" || kind == "rest_cancel" ||
         kind == "study_challenge_choose" || kind == "study_challenge_exit" ||
         kind == "study_etiquette_choose" || kind == "study_etiquette_exit";
}
const LifeFixedServiceBoundary &last(const GrowthState &growth,
                                     const StableId &action) {
  const LifeFixedServiceBoundary *found = nullptr;
  for (const auto &row : growth.fixed_service_boundaries.rows())
    if (row.second.action_ref == action &&
        (!found || row.second.boundary_ordinal > found->boundary_ordinal))
      found = &row.second;
  if (!found)
    fail("GrowthFixedOriginalStartRequired", action);
  return *found;
}
const ActionTimeRegistration &
registration(const CharacterConditionSources &body, const StableId &kind) {
  const ActionTimeRegistration *found = nullptr;
  for (const auto &row : *body.action_registrations)
    if (row.owner_domain == "growth" && row.action_kind == kind) {
      if (found)
        fail("DuplicateGrowthFixedRegistration", kind);
      found = &row;
    }
  if (!found)
    fail("GrowthFixedRegistrationMissing", kind,
         ErrorKind::unsupported_capability);
  return *found;
}
const StudyFact *study(const GrowthState &growth, const StableId &id) {
  const auto *raw = growth.facts.find(id);
  return raw ? std::get_if<StudyFact>(raw) : nullptr;
}
SourceRef owner(const LifeFixedServiceBoundary &b) {
  return {"growth",
          b.owner_ref.fact_kind == GrowthFixedFactKind::study ? "skill_study"
                                                              : "long_rest",
          b.action_ref};
}
LifeActualCommand *command(LifeFixedServiceBoundary &b) {
  if (auto *c = std::get_if<LifeActualCommand>(&b.cause))
    return c;
  if (auto *t = std::get_if<LifeOriginalTerminalApplication>(&b.cause))
    if (auto *s = std::get_if<LifeActualStopTrigger>(&t->trigger))
      return std::get_if<LifeActualCommand>(&s->source);
  return nullptr;
}
} // namespace
Outcome<GrowthFixedCommandPermit>
prepare_growth_fixed_command(const CharacterConditionSources &body,
                             const GrowthState &growth, const StableId &actor,
                             const GrowthCommand &request,
                             const GrowthMarketExactSources *market_exact) {
  try {
    if (!body.growth_catalog ||
        !body.growth_catalog->complete_exact_period_sources || !body.shared ||
        !body.elapsed || !body.scale || !body.action_registrations)
      fail("GrowthFixedCommandSourcesRequired", {},
           ErrorKind::unsupported_capability);
    if (body.growth != &growth)
      fail("GrowthFixedCommandSameWorldRequired");
    if (!selected(request.kind))
      fail("UnsupportedGrowthFixedControlCommand", request.kind,
           ErrorKind::unsupported_capability);
    const auto *person = body.common.actor(actor);
    if (!person)
      fail("UnknownActor", actor, ErrorKind::rule);
    const auto &id =
        request.kind == "rest_cancel" ? person->action_id : request.action_id;
    const auto *action = body.common.action(id);
    if (!action || action->actor_id != actor ||
        (request.kind == "rest_cancel"
             ? action->kind != "long_rest"
             : !growth::study_action_kind(action->kind)))
      fail("GrowthFixedActualControlActionRequired", id, ErrorKind::rule);
    take(validate_complete_growth_fixed_sources(body, growth, market_exact));
    const auto &reg = registration(body, action->kind);
    auto due = take(predict_growth_fixed_step(
        body.common, growth, *body.growth_catalog, *body.shared, *body.elapsed,
        *body.scale, body.catalog.identity, reg, id, body.actual_now, {},
        body.interaction, body.interaction_catalog, market_exact));
    if (due && take(compare_world_moments(*due, body.actual_now)) <= 0)
      fail("GrowthOriginalPeriodMustApplyBeforeCommand", id);
    const auto &previous = last(growth, id);
    if (previous.kind == LifeFixedBoundaryKind::completed ||
        previous.kind == LifeFixedBoundaryKind::stopped)
      fail("GrowthFixedActionAlreadyTerminal", id, ErrorKind::rule);
    GrowthFixedCommandPermit out;


    out.memory_ = CheckpointMemoryCharge(0);
    out.common_ = &body.common;
    out.growth_ = &growth;
    out.elapsed_ = body.elapsed;
    out.identity_ = body.common.state().identity;
    out.actor_ = actor;
    out.action_ = id;
    out.kind_ = request.kind;
    out.previous_ = previous.boundary_id;
    out.at_ = body.actual_now;
    return {true, std::move(out), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status validate_growth_fixed_command_permit(
    const GrowthFixedCommandPermit &permit, const CommonStore &common,
    const GrowthState &growth, const StableId &actor,
    const GrowthCommand &request) {
  try {
    if (permit.common_ != &common || permit.growth_ != &growth ||
        !permit.elapsed_ || !same(permit.identity_, common.state().identity) ||
        permit.actor_ != actor || permit.kind_ != request.kind ||
        !selected(request.kind) ||
        (request.kind != "rest_cancel" && permit.action_ != request.action_id))
      fail("GrowthFixedCommandPermitExpired", actor);
    const auto *action = common.action(permit.action_);
    const auto *elapsed = find_fixed_elapsed(*permit.elapsed_, permit.action_);
    if (!action || action->actor_id != actor || !elapsed ||
        last(growth, permit.action_).boundary_id != permit.previous_ ||
        take(compare_world_moments(elapsed->last_settled_at, permit.at_)) > 0)
      fail("GrowthFixedCommandPermitExpired", permit.action_);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<StableId> bind_growth_fixed_control_command_in_candidate(
    const GrowthState &before, CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const GrowthFixedCaptureServices &, const StableId &actor, Integer seq,
    const CommitResult &result) {
  try {
    if (&body.common != &common || body.growth != &growth ||
        body.elapsed != &elapsed || !body.growth_catalog ||
        !body.growth_catalog->complete_exact_period_sources || !body.shared ||
        !body.scale || !body.action_registrations || !body.fixed_spatial ||
        !body.control)
      fail("GrowthFixedControlSourcesRequired", {},
           ErrorKind::unsupported_capability);
    if (result.kind != "study_pause" && result.kind != "study_resume" &&
        result.kind != "rest_cancel" && result.kind != "study_cancel")
      fail("GrowthFixedControlCommandRequired", result.result_id);
    const auto *cached =
        common.state().results.find(actor + ":" + std::to_string(seq));
    const auto *sequence = common.state().request_sequences.find(actor);
    const auto *effect = result.details
                             ? std::get_if<GrowthEffect>(&result.details->data)
                             : nullptr;
    if (!cached || cached->get() != &result || !sequence || *sequence != seq ||
        !result.ok || !effect || effect->kind != result.kind ||
        effect->action_id.empty() || result.revision != common.revision() ||
        result.tick != body.actual_now.half_day_tick ||
        common.state().history.empty())
      fail("GrowthFixedActualControlCommitRequired", result.result_id);
    const auto &h = common.state().history.back();
    if (h.actor_id != actor || h.kind != result.kind ||
        h.result_id != result.result_id || h.tick != result.tick)
      fail("GrowthFixedActualControlHistoryRequired", result.result_id);
    const auto &id = effect->action_id;
    const auto &old = last(before, id);
    const auto &current = last(growth, id);
    const auto &reg = registration(body, old.action_kind);
    if (current.boundary_id != old.boundary_id) {
      auto *c =
          command(*growth.fixed_service_boundaries.find(current.boundary_id));
      if (!c || c->actor_id != actor || c->command_binding.request_seq != seq ||
          c->command_binding.result_id != result.result_id)
        fail("GrowthFixedConflictingControlApplication", id);
      return adopt_growth_fixed_boundary_in_candidate(
          common, growth, *body.growth_catalog, *body.shared, elapsed,
          *body.scale, body.catalog.identity, reg, current.boundary_id,
          body.actual_now, {}, body.interaction, body.interaction_catalog);
    }
    if (old.boundary_ordinal == std::numeric_limits<Integer>::max())
      fail("GrowthFixedOrdinalOverflow", id);
    const auto *old_study = study(before, id);
    const auto *new_study = study(growth, id);
    LifeFixedBoundaryKind kind;
    if (result.kind == "study_pause" || result.kind == "study_resume") {
      const bool paused = result.kind == "study_pause";
      if (!old_study || !new_study || old_study->phase != "started" ||
          new_study->phase != "started" ||
          growth::study_initiator(new_study->terms) != actor ||
          growth::study_paused(*old_study) == paused ||
          growth::study_paused(*new_study) != paused ||
          old_study->lessons.size() != new_study->lessons.size() ||
          !growth::study_equal(old_study->terms, new_study->terms) ||
          !new_study->pauses || new_study->pauses->empty() ||
          effect->paused != paused ||
          effect->completed_lessons !=
              static_cast<Integer>(new_study->lessons.size()) ||
          !common.action(id) || !growth.actions.find(id))
        fail("GrowthOriginalPauseResumeResultRequired", id);
      const auto &p = new_study->pauses->back();
      if ((paused ? p.start_tick : p.end_tick) !=
              body.actual_now.half_day_tick ||
          new_study->pauses->size() !=
              (old_study->pauses ? old_study->pauses->size() : 0) +
                  (paused ? 1 : 0))
        fail("GrowthOriginalPauseResumeTimeMismatch", id);
      kind = paused ? LifeFixedBoundaryKind::suspended
                    : LifeFixedBoundaryKind::resumed;
    } else if (result.kind == "rest_cancel") {
      const auto *a = before.facts.find(id);
      const auto *b = growth.facts.find(id);
      const auto *prior = a ? std::get_if<RestFact>(a) : nullptr;
      const auto *actual = b ? std::get_if<RestFact>(b) : nullptr;
      if (!prior || !actual || prior->phase != "active" ||
          actual->phase != "completed" || actual->actor_id != actor ||
          actual->stop_reason != "cancelled" ||
          actual->completed_tick != body.actual_now.half_day_tick ||
          actual->elapsed != prior->elapsed ||
          actual->consumed != prior->consumed ||
          effect->elapsed != actual->elapsed || common.action(id) ||
          growth.actions.find(id))
        fail("GrowthOriginalRestCancelResultRequired", id);
      kind = LifeFixedBoundaryKind::stopped;
    } else
      fail("AcceptedStudyServiceCannotSelfCancel", id, ErrorKind::rule);
    LifeFixedServiceBoundary next;
    next.action_ref = id;
    next.action_kind = old.action_kind;
    next.owner_ref = old.owner_ref;
    next.previous_boundary_ref = old.boundary_id;
    next.boundary_ordinal = old.boundary_ordinal + 1;
    next.occurred_at = body.actual_now;
    next.kind = kind;
    next.input_snapshot = old.input_snapshot;
    for (auto &person : next.input_snapshot.actors) {
      const auto *head =
          body.state.character_condition_extensions.actor_condition_heads.find(
              person.actor_id);
      const auto *actual = common.state().actors.find(person.actor_id);
      if (!head || !actual || person.capability_frames.empty())
        fail("GrowthFixedControlBodyRequired", person.actor_id);
      const auto *skill =
          take(read_growth_fixed_required_skill(body, growth, next, person));
      for (auto &frozen : person.capability_frames) {
        if (!same(*head, frozen.condition_head_snapshot))
          fail("GrowthFixedControlUnadoptedConditionChange", person.actor_id);
        std::optional<StableId> required;
        if (frozen.purpose_ref.id == "study" ||
            frozen.purpose_ref.id == "provide_teaching") {
          if (!skill)
            fail("GrowthFixedControlSkillMaskMismatch", person.actor_id);
          required = *skill;
        }
        auto value = take(read_character_capability_at_head(
            body, *head, frozen.purpose_ref, required, body.actual_now));
        if (kind == LifeFixedBoundaryKind::resumed && !value.allowed)
          fail("GrowthFixedResumeCapabilityDenied", person.actor_id,
               ErrorKind::rule);
        frozen = take(freeze_character_capability(value));
      }
      if (skill && (person.skill_qualification_views.size() != 1 ||
                    person.skill_qualification_views.front().minimum_xp !=
                        growth_skill_xp(actual->skills, *skill)))
        fail("GrowthFixedControlUnadoptedSkillChange", person.actor_id);
      if (kind == LifeFixedBoundaryKind::resumed ? actual->action_id != id
                                                 : actual->action_id == id)
        fail("GrowthFixedControlOccupationMismatch", person.actor_id);
      for (auto &value : person.skill_qualification_views) {
        value.source_identity = common.state().identity;
        value.observed_at = body.actual_now;
      }
    }
    take(refresh_growth_fixed_spatial_projection(body, *body.fixed_spatial,
                                                 next));
    if (next.action_kind == "long_rest") {
      auto *raw = growth.facts.find(next.action_ref);
      auto *rest = raw ? std::get_if<RestFact>(raw) : nullptr;
      auto &quiet =
          next.input_snapshot.specific.growth_fact.original_quiet_or_rest_basis;
      if (!rest || !quiet)
        fail("GrowthLongRestQuietSourceRequired", next.action_ref);
      take(refresh_growth_long_rest_quiet_projection(body, rest->actor_id,
                                                     *quiet));
    }
    next.boundary_id = take(common.next_id("growth_fixed_service_boundary"));
    const auto boundary_id = next.boundary_id;
    for (auto &person : next.input_snapshot.actors)
      for (auto &value : person.skill_qualification_views)
        value.actual_boundary_ref = {
            {"growth", "growth_fixed_service_boundary", boundary_id}, 1};
    LifeActualCommand actual;
    actual.actor_id = actor;
    actual.command_binding = {result.kind, seq, result.result_id};
    actual.accepted_business_ref = owner(next);
    if (kind == LifeFixedBoundaryKind::stopped) {
      LifeOriginalTerminalApplication terminal;
      terminal.disposition =
          LifeOriginalTerminalApplication::Disposition::stopped;
      LifeFixedOutcomeRef outcome;
      outcome.kind = LifeFixedOutcomeRef::Kind::rest;
      outcome.id = id;
      terminal.actual_result_refs.push_back(outcome);
      LifeActualStopTrigger stop;
      if (!body.catalog.original_long_rest_rule)
        fail("GrowthOriginalLongRestRuleRequired", id,
             ErrorKind::unsupported_capability);
      stop.original_stop_rule_ref =
          body.catalog.original_long_rest_rule->rule_ref;
      stop.source = std::move(actual);
      terminal.trigger = std::move(stop);
      next.cause = std::move(terminal);
      next.original_outcome_ref = std::move(outcome);
    } else
      next.cause = std::move(actual);
    if (!growth.fixed_service_boundaries.insert(boundary_id, std::move(next)))
      fail("DuplicateGrowthFixedBoundary", boundary_id);
    auto *row = growth.fixed_service_boundaries.find(boundary_id);
    auto *bound = command(*row);
    bound->route_basis =
        take(capture_growth_actor_route(body, growth, boundary_id, *bound));
    if (new_study)
      take(bind_study_time_after_actual_boundary(common, growth, boundary_id));
    take(adopt_growth_fixed_boundary_in_candidate(
        common, growth, *body.growth_catalog, *body.shared, elapsed,
        *body.scale, body.catalog.identity, reg, boundary_id, body.actual_now,
        {}, body.interaction, body.interaction_catalog));
    return {true, boundary_id, {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status adopt_growth_adjustment_inputs_in_candidate(
    const GrowthState &before, CommonStore &common, GrowthState &growth,
    const CharacterConditionSources &body, ActionElapsedStore &elapsed,
    const StableId &actor, Integer sequence, const CommitResult &result) {
  try {
    if (&body.common != &common || body.growth != &growth ||
        body.elapsed != &elapsed || !body.growth_catalog ||
        !body.growth_catalog->complete_exact_command_cuts || !body.shared ||
        !body.scale || !body.action_registrations)
      fail("GrowthAdjustmentInputSourcesRequired", {},
           ErrorKind::unsupported_capability);
    const auto *cached =
        common.state().results.find(actor + ":" + std::to_string(sequence));
    if (!cached || cached->get() != &result || !result.ok ||
        result.tick != body.actual_now.half_day_tick)
      fail("GrowthAdjustmentActualCommitRequired", result.result_id);
    take(validate_study_adjustment_time2_sources(
        common, growth, *body.growth_catalog, body.actual_now));
    // Source writer already froze application_ordinal at its actual successful
    // XP writes. No sorting by result/fact identifiers, and no copying the
    // source table.
    for (Integer ordinal = 1; ordinal <= 6; ++ordinal) {
      const StableId *source_action = nullptr;
      Integer lesson_ordinal = 0;
      for (const auto &row : growth.facts.rows())
        if (const auto *fact = std::get_if<StudyFact>(&row.second))
          for (std::size_t n = 0; n < fact->lessons.size(); ++n) {
            const auto &lesson = fact->lessons[n];
            if (!lesson.segment || !lesson.segment->adjustment ||
                !lesson.segment->adjustment->exact_binding)
              continue;
            const auto &binding = *lesson.segment->adjustment->exact_binding;
            if (binding.result_id != result.result_id ||
                binding.application_ordinal != ordinal)
              continue;
            if (source_action)
              fail("GrowthAdjustmentApplicationOrdinalAmbiguous",
                   result.result_id);
            const auto *old = study(before, row.first);
            if (!old || n >= old->lessons.size())
              fail("GrowthAdjustmentOriginalLessonRequired", row.first);
            if (old->lessons[n].segment && old->lessons[n].segment->adjustment)
              continue;
            if (binding.actor_id != actor || binding.request_seq != sequence ||
                binding.command_kind != result.kind ||
                take(
                    compare_world_moments(binding.settled_at, body.actual_now)))
              fail("GrowthAdjustmentActualBindingMismatch", row.first);
            source_action = &row.first;
            lesson_ordinal = static_cast<Integer>(n + 1);
          }
      if (!source_action)
        continue;
      const StudyPerformanceAdjustmentRef source{*source_action,
                                                 lesson_ordinal};
      auto net = take(read_study_adjustment_permanent_contribution(
          common, growth, *body.growth_catalog, source, body.actual_now));
      if (!net.xp_gained)
        continue;
      const auto &student = net.source.fact->terms.student_id;
      const auto &skill = net.source.fact->terms.skill_id;
      for (const auto &action_row : growth.actions.rows()) {
        const auto &prior = last(growth, action_row.first);
        if (prior.kind == LifeFixedBoundaryKind::completed ||
            prior.kind == LifeFixedBoundaryKind::stopped)
          fail("GrowthAdjustmentTerminalActionStillActive", action_row.first);
        bool affected = false, already = false;
        for (const auto &person : prior.input_snapshot.actors)
          if (person.actor_id == student)
            for (const auto &value : person.skill_qualification_views)
              if (value.skill_id == skill)
                affected = true;
        if (!affected)
          continue;
        for (const auto &row : growth.fixed_service_boundaries.rows())
          if (row.second.action_ref == action_row.first)
            if (const auto *change =
                    std::get_if<LifeActualInputChange>(&row.second.cause))
              if (const auto *value =
                      std::get_if<LifeGrowthInputChange>(&change->change))
                if (const auto *ref =
                        std::get_if<StudyPerformanceAdjustmentRef>(
                            &value->original_result_ref);
                    ref && ref->action_id == source.action_id &&
                    ref->lesson_ordinal == source.lesson_ordinal)
                  already = true;
        if (already)
          continue;
        if (prior.boundary_ordinal == std::numeric_limits<Integer>::max())
          fail("GrowthFixedOrdinalOverflow", action_row.first);
        LifeFixedServiceBoundary next;
        next.action_ref = prior.action_ref;
        next.action_kind = prior.action_kind;
        next.owner_ref = prior.owner_ref;
        next.previous_boundary_ref = prior.boundary_id;
        next.boundary_ordinal = prior.boundary_ordinal + 1;
        next.kind = LifeFixedBoundaryKind::input_rebound;
        next.occurred_at = body.actual_now;
        next.input_snapshot = prior.input_snapshot;
        ActorFoundationRuleRef used_rule;
        bool changed = false;
        for (auto &person : next.input_snapshot.actors) {
          const auto *current =
              body.state.character_condition_extensions.actor_condition_heads
                  .find(person.actor_id);
          if (!current || person.capability_frames.empty())
            fail("GrowthAdjustmentActualBodyRequired", person.actor_id);
          const auto *required = take(
              read_growth_fixed_required_skill(body, growth, next, person));
          for (auto &frame : person.capability_frames) {
            if (!same(*current, frame.condition_head_snapshot))
              fail("GrowthAdjustmentUnadoptedConditionChange", person.actor_id);
            std::optional<StableId> purpose_skill;
            if (frame.purpose_ref.id == "study" ||
                frame.purpose_ref.id == "provide_teaching") {
              if (!required)
                fail("GrowthAdjustmentSkillMaskMismatch", person.actor_id);
              purpose_skill = *required;
            }
            frame = take(freeze_character_capability(
                take(read_character_capability_at_head(
                    body, *current, frame.purpose_ref, purpose_skill,
                    body.actual_now))));
            if (person.actor_id == student)
              used_rule = frame.result.rule_ref;
          }
          for (auto &value : person.skill_qualification_views) {
            value.source_identity = common.state().identity;
            value.observed_at = body.actual_now;
            if (person.actor_id == student && value.skill_id == skill) {
              if (value.minimum_xp != net.xp_before)
                fail("GrowthAdjustmentInputBeforeMismatch", person.actor_id);
              value.minimum_xp = net.xp_after;
              value.minimum_level = growth_skill_level(net.xp_after);
              value.contribution_refs.emplace_back(source);
              changed = true;
            }
          }
        }
        if (!body.fixed_spatial)
          fail("GrowthFixedSpatialSourcesRequired", action_row.first,
               ErrorKind::unsupported_capability);
        take(refresh_growth_fixed_spatial_projection(body, *body.fixed_spatial,
                                                     next));
        if (next.action_kind == "long_rest") {
          auto *raw = growth.facts.find(next.action_ref);
          auto *rest = raw ? std::get_if<RestFact>(raw) : nullptr;
          auto &quiet = next.input_snapshot.specific.growth_fact
                            .original_quiet_or_rest_basis;
          if (!rest || !quiet)
            fail("GrowthLongRestQuietSourceRequired", next.action_ref);
          take(refresh_growth_long_rest_quiet_projection(body, rest->actor_id,
                                                         *quiet));
        }
        if (!changed || used_rule.id.empty())
          fail("GrowthAdjustmentActualReadInputRequired", action_row.first);
        next.cause = LifeActualInputChange{LifeGrowthInputChange{source},
                                           std::move(used_rule)};
        next.boundary_id =
            take(common.next_id("growth_fixed_service_boundary"));
        const auto id = next.boundary_id;
        for (auto &person : next.input_snapshot.actors)
          for (auto &value : person.skill_qualification_views)
            value.actual_boundary_ref = {
                {"growth", "growth_fixed_service_boundary", id}, 1};
        const auto &reg = registration(body, next.action_kind);
        if (!growth.fixed_service_boundaries.insert(id, std::move(next)))
          fail("DuplicateGrowthFixedBoundary", id);
        take(adopt_growth_fixed_boundary_in_candidate(
            common, growth, *body.growth_catalog, *body.shared, elapsed,
            *body.scale, body.catalog.identity, reg, id, body.actual_now, {},
            body.interaction, body.interaction_catalog));
      }
    }
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
} // namespace hanmo::resident
