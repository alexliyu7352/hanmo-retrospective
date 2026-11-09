#include "condition_source_retention.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "condition_capacity.hpp"
#include "injury_sources_v1.hpp"
#include "resident/domains/growth/growth_model.hpp"
#include <array>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
SourceRef source(const CharacterConditionSourceRef &ref) {
  if (const auto *seed = std::get_if<InitialConditionSeedRef>(&ref))
    return initial_condition_seed_source(*seed);
  return character_condition_receipt_source(
      std::get<ConditionReceiptRefV2>(ref));
}
StableId compound(const StableId &first, Integer ordinal,
                  const StableId &actor = {}) {
  return std::to_string(first.size()) + ":" + first + ":" +
         std::to_string(ordinal) + ":" + actor;
}
bool same_consumer(const ConsumerRef &a, const ConsumerRef &b) {
  return a.domain == b.domain && a.kind == b.kind && a.id == b.id &&
         a.slot == b.slot;
}
bool same_pin(const SourcePin &a, const SourcePin &b) {
  return same_consumer(a.consumer, b.consumer) &&
         same_source_ref(a.source, b.source) && a.reason == b.reason;
}
} // namespace
RequiredSourceConsumerSlot
character_condition_consumer_slot(CharacterConditionConsumerSlot slot) {
  switch (slot) {
  case CharacterConditionConsumerSlot::origin_seed:
    return {"actor_foundation", "character_origin", "initial_condition_seed"};
  case CharacterConditionConsumerSlot::head_seed:
    return {"actor_foundation", "condition_head", "initial_condition_seed"};
  case CharacterConditionConsumerSlot::head_latest:
    return {"actor_foundation", "condition_head", "latest_receipt"};
  case CharacterConditionConsumerSlot::receipt_predecessor:
    return {"actor_foundation", "condition_receipt", "condition_before"};
  case CharacterConditionConsumerSlot::fixed_seed:
    return {"growth", "fixed_capability_frame", "initial_condition_seed"};
  case CharacterConditionConsumerSlot::fixed_latest:
    return {"growth", "fixed_capability_frame", "latest_receipt"};
  case CharacterConditionConsumerSlot::fixed_limits:
    return {"growth", "fixed_capability_frame", "limiting_conditions"};
  case CharacterConditionConsumerSlot::study_inputs:
    return {"growth", "study_lesson_capability", "condition_inputs"};
  case CharacterConditionConsumerSlot::study_applied:
    return {"growth", "study_lesson_capability", "applied_condition"};
  case CharacterConditionConsumerSlot::study_boundary:
    return {"growth", "study_condition_boundary", "condition_input"};
  case CharacterConditionConsumerSlot::gather_period_source:
    return {"actor_foundation", "condition_receipt", "actual_gather_period"};
  case CharacterConditionConsumerSlot::injury_application_source:
    return {"actor_foundation", "condition_receipt",
            "original_injury_application"};
  case CharacterConditionConsumerSlot::episode_origin:
    return {"actor_foundation", "condition_episode", "original_condition"};
  case CharacterConditionConsumerSlot::episode_terminal:
    return {"actor_foundation", "condition_episode", "actual_terminal_death"};
  case CharacterConditionConsumerSlot::effect_origin:
    return {"actor_foundation", "condition_effect", "original_condition"};
  case CharacterConditionConsumerSlot::death_result:
    return {"family_life", "family_death", "condition_result"};
  }
  return {};
}
Status
visit_character_condition_source_pins(const CharacterConditionSources &body,
                                      CharacterConditionConsumerSlot slot,
                                      CharacterConditionPinSink &sink) {
  try {
    if (body.catalog.original_growth && !body.growth)
      return common_failure("ConditionRetentionGrowthSourcesRequired");
    take(validate_complete_character_conditions(body));
    const auto spec = character_condition_consumer_slot(slot);
    if (spec.domain.empty())
      return common_failure("UnregisteredConditionConsumerSlot");
    auto emit = [&](const StableId &id, SourceRef ref,
                    SourcePinReason reason = SourcePinReason::dependent_state) {
      take(sink.actual_pin(
          {{spec.domain, spec.kind, id, spec.slot}, std::move(ref), reason}));
    };
    if (slot == CharacterConditionConsumerSlot::origin_seed) {
      for (const auto &row : body.state.character_origin_records.origins.rows())
        emit(row.first,
             initial_condition_seed_source(
                 row.second.initial_condition_seed_ref),
             SourcePinReason::unique_outcome);
    } else if (slot == CharacterConditionConsumerSlot::head_seed ||
               slot == CharacterConditionConsumerSlot::head_latest) {
      for (const auto &row : body.state.character_condition_extensions
                                 .actor_condition_heads.rows()) {
        if (slot == CharacterConditionConsumerSlot::head_seed)
          emit(row.first, initial_condition_seed_source(
                              row.second.initial_condition_seed_ref));
        else if (row.second.latest_receipt_ref)
          emit(row.first, character_condition_receipt_source(
                              *row.second.latest_receipt_ref));
      }
    } else if (slot == CharacterConditionConsumerSlot::receipt_predecessor) {
      for (const auto &row :
           body.state.character_condition_extensions.applied_receipts.rows()) {
        const auto &receipt = row.second;
        const auto *origin =
            body.state.character_origin_records.origins.find(receipt.actor_id);
        if (!origin)
          return common_failure("CharacterOriginRequired", receipt.actor_id);
        SourceRef previous =
            initial_condition_seed_source(origin->initial_condition_seed_ref);
        if (receipt.condition_revision_before) {
          const ConditionReceiptV2 *found = nullptr;
          for (const auto &candidate : body.state.character_condition_extensions
                                           .applied_receipts.rows())
            if (candidate.second.actor_id == receipt.actor_id &&
                candidate.second.condition_revision_after ==
                    receipt.condition_revision_before) {
              if (found)
                return common_failure("DuplicateConditionPredecessor",
                                      receipt.result_id);
              found = &candidate.second;
            }
          if (!found)
            return common_failure("ConditionPredecessorRequired",
                                  receipt.result_id);
          previous = character_condition_receipt_source({found->result_id});
        }
        emit(row.first, std::move(previous));
      }
    } else if (slot == CharacterConditionConsumerSlot::gather_period_source) {
      // The complete Condition validator already proves the actual accepted
      // actor, original complete FWP, method, applied M and paired output ref.
      // Pin that real economic producer, not a manufactured Growth activity.
      for (const auto &row :
           body.state.character_condition_extensions.applied_receipts.rows())
        if (const auto *original =
                std::get_if<GatherFatigueConditionCauseV1>(&row.second.cause))
          emit(row.first,
               {"local_economy", "resource_gather_receipt",
                original->receipt_id},
               SourcePinReason::unique_outcome);
    } else if (slot ==
               CharacterConditionConsumerSlot::injury_application_source) {
      if (!body.injury_sources) {
        if (battle_injury_source1_selected(body.catalog.identity))
          return common_failure("ConditionRetentionInjurySourcesRequired");
      } else
        for (const auto &row : body.injury_sources->applications.rows()) {
          // The complete original reader above authenticates this exact finite
          // payload. Both Rest and Treatment retain their real Growth service
          // completion row; fulfillment identity is that original action
          // itself.
          if (const auto *battle = std::get_if<CharacterBattleReceiptSourceV1>(
                  &row.second.original_source))
            emit(row.first, battle->original_receipt,
                 SourcePinReason::unique_outcome);
          else if (const auto *rest = std::get_if<OriginalRestProvidedCause>(
                       &row.second.original_source))
            emit(row.first, {"growth", "life_activity", rest->action_id},
                 SourcePinReason::unique_outcome);
          else {
            const auto &treatment = std::get<CharacterTreatmentActionSourceV1>(
                row.second.original_source);
            emit(row.first, {"growth", "life_activity", treatment.action_id},
                 SourcePinReason::unique_outcome);
          }
        }
    } else if (slot == CharacterConditionConsumerSlot::episode_origin ||
               slot == CharacterConditionConsumerSlot::episode_terminal) {
      for (const auto &row :
           body.state.character_condition_extensions.episodes.rows()) {
        if (slot == CharacterConditionConsumerSlot::episode_origin)
          emit(row.first, source(row.second.origin),
               SourcePinReason::unique_outcome);
        else if (row.second.terminal_source_ref) {
          const auto *death =
              std::get_if<DeathRef>(&*row.second.terminal_source_ref);
          if (!death)
            return common_failure(
                "ConditionEpisodeOriginalTerminalSourceRequired", row.first);
          emit(row.first, {death->domain, death->kind, death->id},
               SourcePinReason::unique_outcome);
        }
      }
    } else if (slot == CharacterConditionConsumerSlot::effect_origin) {
      for (const auto &row : body.state.character_condition_extensions
                                 .effect_contributions.rows()) {
        // Permanent original contributions remain dependent after episode
        // death.
        const auto *original = std::get_if<ConditionReceiptRefV2>(
            &row.second.identity.origin_result);
        if (!original)
          return common_failure("ConditionEffectOriginalReceiptRequired",
                                row.first);
        emit(row.first, character_condition_receipt_source(*original),
             SourcePinReason::unique_outcome);
      }
    } else if (slot == CharacterConditionConsumerSlot::death_result) {
      if (body.deaths)
        for (const auto &death : body.deaths->deaths.rows())
          for (const auto &row : body.state.character_condition_extensions
                                     .applied_receipts.rows())
            if (row.second.life_transition_ref ==
                std::optional<StableId>{death.first})
              emit(death.first, character_condition_receipt_source({row.first}),
                   SourcePinReason::unique_outcome);
    } else if (body.growth) {
      const auto &growth = *body.growth;
      if (slot == CharacterConditionConsumerSlot::fixed_seed ||
          slot == CharacterConditionConsumerSlot::fixed_latest ||
          slot == CharacterConditionConsumerSlot::fixed_limits) {
        for (const auto &row : growth.fixed_service_boundaries.rows()) {
          Integer ordinal = 0;
          for (const auto &actor : row.second.input_snapshot.actors)
            for (const auto &frame : actor.capability_frames) {
              const auto id = compound(row.first, ordinal++, actor.actor_id);
              if (slot == CharacterConditionConsumerSlot::fixed_seed)
                emit(id, initial_condition_seed_source(
                             frame.condition_head_snapshot
                                 .initial_condition_seed_ref));
              else if (slot == CharacterConditionConsumerSlot::fixed_latest) {
                if (frame.condition_head_snapshot.latest_receipt_ref)
                  emit(id,
                       character_condition_receipt_source(
                           *frame.condition_head_snapshot.latest_receipt_ref));
              } else
                for (std::size_t n = 0;
                     n < frame.result.limiting_condition_refs.size(); ++n) {
                  const auto ref =
                      source(frame.result.limiting_condition_refs[n]);
                  bool duplicate = false;
                  for (std::size_t p = 0; p < n; ++p)
                    if (same_source_ref(
                            source(frame.result.limiting_condition_refs[p]),
                            ref))
                      duplicate = true;
                  if (!duplicate)
                    emit(id, ref);
                }
            }
        }
      } else if (slot == CharacterConditionConsumerSlot::study_inputs ||
                 slot == CharacterConditionConsumerSlot::study_applied) {
        for (const auto &row :
             growth.lesson_capabilities.lesson_capability_sources) {
          auto actor = [&](const FrozenLessonCapability &frozen,
                           const ConditionReceiptRefV2 &receipt) {
            const auto id =
                compound(row.action_id, row.lesson_ordinal, frozen.actor_id);
            if (slot == CharacterConditionConsumerSlot::study_applied) {
              emit(id, character_condition_receipt_source(receipt),
                   SourcePinReason::unique_outcome);
              return;
            }
            for (std::size_t n = 0; n < frozen.condition_source_refs.size();
                 ++n) {
              const auto ref = source(frozen.condition_source_refs[n]);
              bool duplicate = false;
              for (std::size_t p = 0; p < n; ++p)
                if (same_source_ref(source(frozen.condition_source_refs[p]),
                                    ref))
                  duplicate = true;
              if (!duplicate)
                emit(id, ref, SourcePinReason::unique_outcome);
            }
          };
          actor(row.student_capability, row.student_condition_receipt_ref);
          if (row.teacher_capability && row.teacher_condition_receipt_ref)
            actor(*row.teacher_capability, *row.teacher_condition_receipt_ref);
          else if (bool(row.teacher_capability) !=
                   bool(row.teacher_condition_receipt_ref))
            return common_failure("StudyTeacherConditionSourceRequired",
                                  row.action_id);
        }
      } else if (slot == CharacterConditionConsumerSlot::study_boundary) {
        for (const auto &row :
             growth.lesson_capabilities.study_condition_boundaries) {
          emit(compound(row.action_id, row.boundary_ordinal, "student"),
               source(row.student_condition_ref));
          if (row.teacher_condition_ref)
            emit(compound(row.action_id, row.boundary_ordinal, "teacher"),
                 source(*row.teacher_condition_ref));
        }
      }
    }
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Status
validate_character_condition_source_pin(const CharacterConditionSources &body,
                                        CharacterConditionConsumerSlot slot,
                                        const SourcePin &pin) {
  struct Find final : CharacterConditionPinSink {
    const SourcePin &wanted;
    std::size_t count = 0;
    explicit Find(const SourcePin &value) : wanted(value) {}
    Status actual_pin(const SourcePin &value) override {
      if (same_pin(value, wanted))
        ++count;
      return common_success();
    }
  } find(pin);
  auto valid = visit_character_condition_source_pins(body, slot, find);
  if (!valid.ok)
    return valid;
  if (find.count != 1)
    return common_failure("CharacterConditionPinNotActual", pin.consumer.id);
  auto selected = read_initial_condition_seed_schema(body.catalog);
  if (!selected.ok)
    return {false, {}, selected.error};
  const Integer schema =
      pin.source.kind == "initial_condition_seed" ? selected.value : 2;
  auto actual = read_character_condition_source(body, pin.source, schema);
  return actual.ok ? common_success() : Status{false, {}, actual.error};
}
Outcome<bool> character_condition_source_owner_may_release(
    const CharacterConditionSources &body, const SourceRef &ref) {
  auto selected = read_initial_condition_seed_schema(body.catalog);
  if (!selected.ok)
    return {false, false, selected.error};
  const Integer schema =
      ref.kind == "initial_condition_seed" ? selected.value : 2;
  auto original = read_character_condition_source(body, ref, schema);
  if (!original.ok)
    return {false, false, original.error};
  struct Find final : CharacterConditionPinSink {
    const SourceRef &target;
    bool used = false;
    explicit Find(const SourceRef &value) : target(value) {}
    Status actual_pin(const SourcePin &pin) override {
      if (same_source_ref(pin.source, target))
        used = true;
      return common_success();
    }
  } find(ref);
  constexpr std::array<CharacterConditionConsumerSlot, 16> slots{
      CharacterConditionConsumerSlot::origin_seed,
      CharacterConditionConsumerSlot::head_seed,
      CharacterConditionConsumerSlot::head_latest,
      CharacterConditionConsumerSlot::receipt_predecessor,
      CharacterConditionConsumerSlot::fixed_seed,
      CharacterConditionConsumerSlot::fixed_latest,
      CharacterConditionConsumerSlot::fixed_limits,
      CharacterConditionConsumerSlot::study_inputs,
      CharacterConditionConsumerSlot::study_applied,
      CharacterConditionConsumerSlot::study_boundary,
      CharacterConditionConsumerSlot::death_result,
      CharacterConditionConsumerSlot::episode_origin,
      CharacterConditionConsumerSlot::episode_terminal,
      CharacterConditionConsumerSlot::effect_origin,
      CharacterConditionConsumerSlot::injury_application_source,
      CharacterConditionConsumerSlot::gather_period_source};
  for (auto slot : slots) {
    auto valid = visit_character_condition_source_pins(body, slot, find);
    if (!valid.ok)
      return {false, false, valid.error};
  }
  auto capacity = visit_condition_purpose_capacity_source_pins(body, find);
  if (!capacity.ok)
    return {false, false, capacity.error};
  return {true, !find.used, {}};
}
} // namespace hanmo::resident
