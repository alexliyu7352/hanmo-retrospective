#include "injury_grade_effects_v3.hpp"
#include "field_injury_write_v1.hpp"
#include "authored_origin.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "battle_injury_episode_v1.hpp"
#include "character_condition_replay.hpp"
#include "condition_application_fact.hpp"
#include "injury_capability_map.hpp"
#include "injury_progress_replay_v1.hpp"
#include "injury_source_links_v1.hpp"
#include "personal_injury_write_v1.hpp"
#include "resident/domains/growth/growth_permanent_contributions.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/domains/personal/personal_battle_lifecycle.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
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
template <class T> T take(Outcome<T> result) {
  if (!result.ok)
    throw Failure{std::move(result.error)};
  return std::move(result.value);
}
bool same(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
} // namespace
static Outcome<BoundActorConditionWrite> apply_battle_body_in_candidate_v1(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    CharacterInjurySourcesStateV1 &injury,
    const CharacterInjuryReplaySourcesV1 &context, const SourceRef &casualty,
    const StableId &actor_id, const FieldSegmentApplicationRead *field_clock,
    const PersonalBattleLifecycleWriteContext *personal_clock,
    const GrowthPermanentState &growth,
    GrowthCapabilityRevisionWrite &capability) {
  try {
    const auto &s = context.conditions;
    if (&s.common != &common || &s.state != &foundation ||
        &context.injury != &injury || s.injury_sources != &injury ||
        s.permanent_growth != &growth ||
        (bool(field_clock) == bool(personal_clock)) ||
        (field_clock &&
         (!s.military || !context.field || s.injury_field != context.field ||
          !field_clock->bound_to(common, *s.military, *context.field))) ||
        (personal_clock && (&personal_clock->common != &common ||
                            context.personal != &personal_clock->state ||
                            s.injury_personal != context.personal)))
      fail("FieldInjurySameCandidateRequired", actor_id);
    bool growth3 = false;
    for (const auto &module : s.catalog.identity.module_versions)
      if (module.first == "growth_permanent_contributions") {
        if (growth3 || module.second != "3")
          fail("FieldInjuryCapabilitySelectionRequired", actor_id);
        growth3 = true;
      }
    if (!growth3 || !capability.bound_to(common, growth))
      fail("FieldInjuryActualCapabilityWriterRequired", actor_id);
    const auto now =
        take(field_clock ? field_clock->read_current_moment()
                         : personal_clock->clock.read_current_moment());
    if (take(compare_world_moments(now, s.actual_now)))
      fail("FieldInjuryActualClockRequired", actor_id);
    const auto capture = common.actor_condition_capture_identity();
    if (!capture)
      fail("FieldInjuryActualCaptureRequired", actor_id);
    const auto savepoint = take(common.actor_condition_savepoint_boundary());


    auto source_memory = CheckpointMemoryCharge(0);
    CharacterBattleReceiptSourceV1 original{casualty, 1};
    auto selected = take(read_selected_battle_injury_source_v1(
        s, context.personal, context.field, original, actor_id));
    const bool personal = personal_clock != nullptr;
    if (selected.rule->cause_adapter !=
            (personal ? InjuryCapabilityMapEntry::CauseAdapter::personal_defeat
                      : InjuryCapabilityMapEntry::CauseAdapter::
                            field_segment_wounded) ||
        (!personal && selected.incoming_injury != 1) || !selected.occurred_at ||
        take(compare_world_moments(*selected.occurred_at, now)))
      fail("BattleInjuryActualSourceRequired", actor_id);
    if (field_clock)
      take(validate_field_segment_application_sources(
          common, *s.military, *context.field, *field_clock));
    const PersonalUnit *personal_unit = nullptr;
    if (personal) {
      const auto *battle = context.personal->battles.find(casualty.id);
      personal_unit = battle ? battle->units.find(actor_id) : nullptr;
      if (!personal_unit)
        fail("PersonalInjuryActualParticipantRequired", actor_id);
    }
    const bool injury_written = !personal || personal_unit->hp == 0;
    const bool positive = selected.incoming_injury > 0;
    if (!positive)
      selected.mapping = nullptr;
    for (const auto &row : injury.applications.rows()) {
      const auto *source = std::get_if<CharacterBattleReceiptSourceV1>(
          &row.second.original_source);
      if (source && row.second.actor_id == actor_id &&
          same_source_ref(source->original_receipt, casualty))
        fail("FieldInjuryAlreadyApplied", actor_id);
    }
    auto *head =
        foundation.character_condition_extensions.actor_condition_heads.find(
            actor_id);
    const auto *actor = common.state().actors.find(actor_id);
    if (!head || !actor || !actor->alive ||
        head->condition_revision == std::numeric_limits<Integer>::max())
      fail("FieldInjuryActualLivingActorRequired", actor_id);


    auto prefix_memory = CheckpointMemoryCharge(0);
    const auto verified =
        take(read_character_condition_verified_prefix(s, *head, now));
    if (verified.body.hp != actor->hp ||
        verified.body.fatigue != actor->fatigue ||
        verified.body.injury != actor->injury ||
        verified.body.alive != actor->alive)
      fail("FieldInjuryBeforeBodyMismatch", actor_id);
    const auto before =
        take(read_character_injury_progress_at_head_v1(context, *head, now));
    const auto after = positive ? take(apply_growth_battle_injury_v1(
                                      before, selected.incoming_injury))
                                : before;
    take(validate_battle_injury_episode_sources_v1(s));
    const bool grade_profile = injury_grade_effects_v3_selected(s.catalog);
    const bool applied =
        positive && selected.rule->selected_applied &&
        (!grade_profile || take(read_original_injury_scar_due_v3(
                               s, actor_id, head->condition_revision,
                               before.injury, after.injury)));
    if (!applied)
      selected.mapping = nullptr;
    const ConditionEpisodeV2 *existing = nullptr;
    if (applied) {
      if (!selected.mapping)
        fail("FieldInjuryActualMapRequired", actor_id);
      for (const auto &row :
           foundation.character_condition_extensions.episodes.rows())
        if (row.second.actor_id == actor_id &&
            !row.second.terminal_source_ref &&
            actor_foundation_rule_equal(row.second.condition_rule_ref,
                                        selected.rule->injury_rule_ref)) {
          if (existing)
            fail("DuplicateActiveBattleInjuryEpisode", actor_id);
          existing = &row.second;
        }
      if (existing) {
        const auto *ref = std::get_if<ConditionReceiptRefV2>(&existing->origin);
        const auto *first = ref ? foundation.character_condition_extensions
                                      .applied_receipts.find(ref->result_id)
                                : nullptr;
        if (!first || first->selected_injury_result.effect_items.size() !=
                          selected.mapping->effect_items.size())
          fail("FieldInjuryOriginalEpisodeEffectsRequired", actor_id);
        for (std::size_t i = 0; i < selected.mapping->effect_items.size(); ++i)
          if (!same_original_injury_effect(
                  first->selected_injury_result.effect_items[i],
                  selected.mapping->effect_items[i]))
            fail("FieldInjuryOriginalStageEffectRuleRequired", actor_id);
      }
    }
    const Integer hp_before = actor->hp;
    const Integer fatigue_before = actor->fatigue;
    const Integer hp_after = personal ? std::max<Integer>(1, personal_unit->hp)
                                      : std::min<Integer>(hp_before, 70);
    const Integer fatigue_after =
        personal ? std::min<Integer>(100, fatigue_before + 6) : fatigue_before;
    const Integer revision_before = head->condition_revision;


    auto identity_memory = CheckpointMemoryCharge(0);
    const auto identity = common.state().identity;
    const auto offset = common.actor_condition_execution_drafts().size();
    ActorMutation mutation;
    mutation.actor_id = actor_id;
    mutation.hp = hp_after;
    if (personal)
      mutation.fatigue = fatigue_after;
    if (injury_written)
      mutation.injury = after.injury;
    take(common.apply_actor(mutation));
    take(common.validate_actor_condition_savepoint_boundary(savepoint));
    const auto &drafts = common.actor_condition_execution_drafts();
    if (capture != common.actor_condition_capture_identity() ||
        drafts.size() != offset + 1)
      fail("FieldInjurySingleOriginalWriteRequired", actor_id);
    const auto &draft = drafts[offset];
    if (!same(identity, draft.identity) || draft.actor_id != actor_id ||
        draft.kind != ActorConditionExecutionDraft::Kind::mutation ||
        draft.sequence != Integer(offset + 1) || !draft.before ||
        !draft.hp_written || draft.injury_written != injury_written ||
        draft.fatigue_written != personal || draft.alive_written ||
        draft.before->hp != hp_before ||
        draft.before->injury != before.injury || draft.after.hp != hp_after ||
        draft.after.injury != after.injury ||
        draft.before->fatigue != fatigue_before ||
        draft.after.fatigue != fatigue_after ||
        draft.before->alive != draft.after.alive ||
        draft.next_instance_before != draft.next_instance_after)
      fail("FieldInjuryActualDraftMismatch", actor_id);
    ConditionReceiptV2 receipt;
    receipt.result_id = take(common.next_id("condition"));

    // Actual first copies in receipt, source application, head and returned
    // capture. Target owning tables retain their own same-job graph separately.
    (void)0;


    auto receipt_memory = CheckpointMemoryCharge(0);
    receipt.actor_id = actor_id;
    receipt.cause = CharacterInjurySourceRefV1{receipt.result_id};
    receipt.business_key =
        CharacterInjuryConditionBusinessKeyV1{{receipt.result_id}, actor_id};
    receipt.rule_ref = selected.rule->rule_ref;
    receipt.effective_at.exact_moment = now;
    receipt.condition_revision_before = revision_before;
    receipt.condition_revision_after = revision_before + 1;
    receipt.scalar_changes.reserve(personal ? (injury_written ? 3 : 2) : 2);
    receipt.scalar_changes.push_back(
        {ConditionScalarChange::Field::hp, hp_before, hp_after});
    if (personal)
      receipt.scalar_changes.push_back({ConditionScalarChange::Field::fatigue,
                                        fatigue_before, fatigue_after});
    if (injury_written)
      receipt.scalar_changes.push_back(
          {ConditionScalarChange::Field::injury, before.injury, after.injury});
    receipt.selected_injury_result.kind =
        !positive ? SelectedInjuryResult::Kind::not_an_injury
        : applied ? SelectedInjuryResult::Kind::applied
                  : SelectedInjuryResult::Kind::no_additional_effect;
    if (positive) {
      receipt.selected_injury_result.injury_rule_ref =
          selected.rule->injury_rule_ref;
      receipt.selected_injury_result.result_choice_ref =
          selected.rule->result_choice_ref;
    }
    if (selected.mapping)
      receipt.selected_injury_result.effect_items =
          selected.mapping->effect_items;
    StableId episode_id;


    auto existing_episode_memory = CheckpointMemoryCharge(0);
    if (selected.mapping && (!existing || grade_profile)) {
      episode_id = existing ? existing->episode_id
                            : take(common.next_id("condition_episode"));
      receipt.created_effect_keys.reserve(
          selected.mapping->effect_items.size());
      for (const auto &effect : selected.mapping->effect_items)
        receipt.created_effect_keys.push_back(effect.effect_key);
    }
    const auto result_id = receipt.result_id;
    CharacterInjuryApplicationV1 application{
        actor_id, original, {before, after, 0, 0}};
    if (!injury.applications.insert(result_id, std::move(application)) ||
        !foundation.character_condition_extensions.applied_receipts.insert(
            result_id, std::move(receipt)))
      fail("FieldInjuryReceiptCollision", result_id);
    if (selected.mapping && !existing) {


      auto episode_memory = CheckpointMemoryCharge(0);
      ConditionEpisodeV2 episode;
      episode.episode_id = episode_id;
      episode.actor_id = actor_id;
      episode.condition_rule_ref = selected.rule->injury_rule_ref;
      episode.origin = ConditionReceiptRefV2{result_id};
      episode.started_at.exact_moment = now;
      episode.stage_ref = selected.mapping->stage_ref;
      if (!foundation.character_condition_extensions.episodes.insert(
              episode_id, std::move(episode)))
        fail("FieldInjuryEpisodeCollision", episode_id);
    }
    if (selected.mapping && (!existing || grade_profile)) {
      for (const auto &effect : selected.mapping->effect_items) {


        auto contribution_memory = CheckpointMemoryCharge(0);
        ConditionEffectContribution contribution;
        contribution.identity = {actor_id, episode_id,
                                 ConditionReceiptRefV2{result_id},
                                 effect.effect_key};
        const auto key = take(common.next_id("condition_effect"));
        if (!foundation.character_condition_extensions.effect_contributions
                 .insert(key, std::move(contribution)))
          fail("FieldInjuryEffectCollision", key);
      }
    }
    head->condition_revision = revision_before + 1;
    head->latest_receipt_ref = ConditionReceiptRefV2{result_id};
    BoundActorConditionWrite bound{{result_id}, capture, draft.sequence};
    take(append_condition_application_fact_in_candidate(common, foundation,
                                                        s.catalog, bound));
    take(validate_character_injury_bound_source_link_v1(
        common, foundation, s.catalog, injury, bound));
    take(validate_battle_injury_selected_result_v1(
        s, *foundation.character_condition_extensions.applied_receipts.find(
               result_id)));
    take(validate_battle_injury_episode_sources_v1(s));
    // Original revision owner also emits the actual registered-purpose
    // boolean transitions; no zero revision or synthetic capacity snapshot.
    take(capability.record_actual_condition_change(bound));
    return {true, std::move(bound), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Outcome<BoundActorConditionWrite> apply_field_segment_injury_in_candidate_v1(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    CharacterInjurySourcesStateV1 &injury,
    const CharacterInjuryReplaySourcesV1 &context, const SourceRef &source,
    const StableId &actor, const FieldSegmentApplicationRead &clock,
    const GrowthPermanentState &growth,
    GrowthCapabilityRevisionWrite &capability) {
  return apply_battle_body_in_candidate_v1(common, foundation, injury, context,
                                           source, actor, &clock, nullptr,
                                           growth, capability);
}
Outcome<BoundActorConditionWrite>
apply_personal_terminal_condition_in_candidate_v1(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    CharacterInjurySourcesStateV1 &injury,
    const CharacterInjuryReplaySourcesV1 &context, const SourceRef &source,
    const StableId &actor, const PersonalBattleLifecycleWriteContext &clock,
    const GrowthPermanentState &growth,
    GrowthCapabilityRevisionWrite &capability) {
  return apply_battle_body_in_candidate_v1(common, foundation, injury, context,
                                           source, actor, nullptr, &clock,
                                           growth, capability);
}
bool NativePersonalTerminalConditionWrite::bound_to(
    const CommonStore &common, const PersonalState &personal,
    const PersonalCatalog &catalog) const noexcept {
  return &common_ == &common && &clock_.common == &common &&
         &clock_.state == &personal && sources_.personal == &personal &&
         sources_.conditions.personal_catalog == &catalog &&
         &sources_.conditions.common == &common &&
         &sources_.conditions.state == &state_ &&
         sources_.conditions.injury_sources == &injury_ &&
         &sources_.injury == &injury_ &&
         sources_.conditions.permanent_growth == &growth_ &&
         battle_injury_source1_selected(sources_.conditions.catalog.identity) &&
         capability_.bound_to(common, growth_);
}
Status NativePersonalTerminalConditionWrite::apply_actual_terminal(
    const StableId &battle, const StableId &actor) {
  if (!sources_.conditions.personal_catalog ||
      !bound_to(common_, clock_.state, *sources_.conditions.personal_catalog))
    return common_failure("PersonalTerminalConditionSameCandidateRequired",
                          actor);


  auto source_memory = CheckpointMemoryCharge(0);
  auto result = apply_personal_terminal_condition_in_candidate_v1(
      common_, state_, injury_, sources_,
      {"personal", "personal_battle", battle}, actor, clock_, growth_,
      capability_);
  return result.ok ? common_success() : Status{false, {}, result.error};
}
} // namespace hanmo::resident
