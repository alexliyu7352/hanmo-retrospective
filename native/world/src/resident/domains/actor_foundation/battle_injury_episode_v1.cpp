#include "injury_grade_effects_v3.hpp"
#include "battle_injury_episode_v1.hpp"
#include "authored_origin.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "character_condition_read.hpp"
#include "condition_application_fact.hpp"
#include "injury_capability_map.hpp"
#include "injury_sources_v1.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
namespace {
Status validate_episode_death_v1(const CharacterConditionSources &s,
                                 const ConditionEpisodeV2 &episode,
                                 const ConditionReceiptV2 &origin) {
  if (!s.deaths || !s.life_sources)
    return common_failure("BattleInjuryEpisodeLifeSourcesRequired",
                          episode.episode_id);
  const auto *actual = family_death_for_actor(*s.deaths, episode.actor_id);
  if (!episode.terminal_source_ref)
    return actual ? common_failure("BattleInjuryEpisodeDeathTerminalRequired",
                                   episode.episode_id)
                  : common_success();
  const auto *ref = std::get_if<DeathRef>(&*episode.terminal_source_ref);
  if (!ref || !actual || !actual->precise ||
      ref->id != actual->precise->death_id)
    return common_failure("BattleInjuryEpisodeActualDeathRequired",
                          episode.episode_id);
  auto death = read_verified_death4(s.common, *s.deaths, *s.life_sources, *ref,
                                    s.actual_now, s.military_history_order);
  if (!death.ok)
    return {false, {}, death.error};
  if (death.value.actor_id != episode.actor_id)
    return common_failure("BattleInjuryEpisodeDeathActorMismatch",
                          episode.episode_id);
  const ConditionReceiptV2 *life = nullptr;
  for (const auto &row :
       s.state.character_condition_extensions.applied_receipts.rows())
    if (row.second.life_transition_ref == std::optional<StableId>{ref->id}) {
      if (life)
        return common_failure("DuplicateEpisodeLifeCondition",
                              episode.episode_id);
      life = &row.second;
    }
  if (!life || life->actor_id != episode.actor_id ||
      life->condition_revision_before < origin.condition_revision_after)
    return common_failure("BattleInjuryEpisodeDeathOrderMismatch",
                          episode.episode_id);
  auto verified = validate_life_termination_condition(
      s.common, s.state, *s.deaths, s.catalog, *s.life_sources, *life,
      s.actual_now, s.military_history_order);
  if (!verified.ok)
    return verified;
  return validate_condition_application_fact_for_receipt(s.common, s.state,
                                                         s.catalog, *life);
}
} // namespace
Outcome<VerifiedBattleInjuryEffectV1>
read_battle_injury_effect_source_v1(const CharacterConditionSources &s,
                                    const StableId &key) {
  const auto &state = s.state.character_condition_extensions;
  const auto *contribution = state.effect_contributions.find(key);
  if (!contribution || key.empty())
    return {
        false,
        {},
        common_failure("BattleInjuryActualContributionRequired", key).error};
  const auto &id = contribution->identity;
  const auto *origin_ref =
      std::get_if<ConditionReceiptRefV2>(&id.origin_result);
  const auto *episode = state.episodes.find(id.episode_id);
  const auto *episode_origin =
      episode ? std::get_if<ConditionReceiptRefV2>(&episode->origin) : nullptr;
  if (!episode || episode->episode_id != id.episode_id ||
      episode->actor_id != id.actor_id || !origin_ref || !episode_origin ||
      (!injury_grade_effects_v3_selected(s.catalog) &&
       episode_origin->result_id != origin_ref->result_id))
    return {
        false,
        {},
        common_failure("BattleInjuryActualEpisodeOriginRequired", key).error};
  const auto *receipt = state.applied_receipts.find(origin_ref->result_id);
  if (!receipt || receipt->actor_id != id.actor_id ||
      !actor_foundation_rule_equal(
          episode->condition_rule_ref,
          receipt->selected_injury_result.injury_rule_ref))
    return {false,
            {},
            common_failure("BattleInjuryEpisodeRuleMismatch", key).error};
  auto source = validate_battle_injury_selected_result_v1(s, *receipt);
  if (!source.ok)
    return {false, {}, source.error};
  if (receipt->selected_injury_result.kind !=
          SelectedInjuryResult::Kind::applied ||
      episode->started_at.kind != ActorFoundationAt::Kind::exact_moment)
    return {false,
            {},
            common_failure("BattleInjuryAppliedEpisodeRequired", key).error};
  const auto *first = state.applied_receipts.find(episode_origin->result_id);
  if (!first || first->actor_id != id.actor_id ||
      first->condition_revision_after > receipt->condition_revision_after ||
      !actor_foundation_rule_equal(
          first->selected_injury_result.injury_rule_ref,
          episode->condition_rule_ref))
    return {
        false,
        {},
        common_failure("BattleInjuryActualFirstReceiptRequired", key).error};
  auto first_source = validate_battle_injury_selected_result_v1(s, *first);
  if (!first_source.ok)
    return {false, {}, first_source.error};
  if (first->selected_injury_result.kind != SelectedInjuryResult::Kind::applied)
    return {false,
            {},
            common_failure("BattleInjuryActualFirstEffectRequired", key).error};
  auto origin_order = compare_world_moments(first->effective_at.exact_moment,
                                            receipt->effective_at.exact_moment);
  if (!origin_order.ok)
    return {false, {}, origin_order.error};
  if (origin_order.value > 0)
    return {false,
            {},
            common_failure("BattleInjuryContributionBeforeEpisode", key).error};
  auto time = compare_world_moments(episode->started_at.exact_moment,
                                    first->effective_at.exact_moment);
  if (!time.ok)
    return {false, {}, time.error};
  if (time.value)
    return {
        false,
        {},
        common_failure("BattleInjuryEpisodeActualStartMismatch", key).error};
  const auto *application =
      s.injury_sources->applications.find(receipt->result_id);
  const auto *battle = application
                           ? std::get_if<CharacterBattleReceiptSourceV1>(
                                 &application->original_source)
                           : nullptr;
  if (!battle)
    return {
        false,
        {},
        common_failure("BattleInjuryEpisodeBattleSourceRequired", key).error};
  auto selected = read_selected_battle_injury_source_v1(
      s, s.injury_personal, s.injury_field, *battle, id.actor_id);
  if (!selected.ok)
    return {false, {}, selected.error};
  if (!selected.value.mapping ||
      !actor_foundation_rule_equal(episode->stage_ref,
                                   selected.value.mapping->stage_ref) ||
      episode->stage_revision != 0 || episode->next_calendar_due)
    return {false,
            {},
            common_failure("BattleInjuryOriginalStageRuleRequired", key).error};
  auto terminal = validate_episode_death_v1(s, *episode, *receipt);
  if (!terminal.ok)
    return {false, {}, terminal.error};
  if (contribution->current_state != ConditionEffectState::active ||
      contribution->last_resolution_ref)
    return {false,
            {},
            common_failure("BattleInjuryActualLifecycleReaderRequired", key,
                           ErrorKind::unsupported_capability)
                .error};
  // Uniqueness is by actual retained identity, not merely by an arbitrary table
  // key.
  std::size_t identical = 0;
  for (const auto &row : state.effect_contributions.rows()) {
    const auto &other = row.second.identity;
    const auto *other_origin =
        std::get_if<ConditionReceiptRefV2>(&other.origin_result);
    if (other.actor_id == id.actor_id && other.episode_id == id.episode_id &&
        other.effect_key == id.effect_key && other_origin &&
        other_origin->result_id == origin_ref->result_id)
      ++identical;
  }
  if (identical != 1)
    return {false,
            {},
            common_failure("DuplicateBattleInjuryContribution", key).error};
  for (const auto &row : state.episodes.rows()) {
    if (&row.second != episode && row.second.actor_id == id.actor_id &&
        actor_foundation_rule_equal(row.second.condition_rule_ref,
                                    episode->condition_rule_ref) &&
        !row.second.terminal_source_ref)
      return {false,
              {},
              common_failure("DuplicateActiveBattleInjuryEpisode", key).error};
  }
  const auto &effects = receipt->selected_injury_result.effect_items;
  if (receipt->created_effect_keys.size() != effects.size())
    return {
        false,
        {},
        common_failure("BattleInjuryOriginEffectCoverageMismatch", key).error};
  for (std::size_t i = 0; i < effects.size(); ++i)
    if (receipt->created_effect_keys[i] != effects[i].effect_key)
      return {false,
              {},
              common_failure("BattleInjuryOriginEffectKeyMismatch", key).error};
  const InjuryEffectItem *effect = nullptr;
  for (const auto &item : receipt->selected_injury_result.effect_items)
    if (item.effect_key == id.effect_key)
      effect = &item;
  if (!effect ||
      !std::holds_alternative<PermanentInjuryEffect>(effect->retention) ||
      !std::holds_alternative<InjuryAttributeReduction>(effect->effect_payload))
    return {false,
            {},
            common_failure("BattleInjuryOriginalPermanentEffectRequired", key)
                .error};
  return {true, {episode, receipt, contribution, effect}, {}};
}
} // namespace hanmo::resident

namespace hanmo::resident {
Status
validate_battle_injury_episode_sources_v1(const CharacterConditionSources &s) {
  const auto &state = s.state.character_condition_extensions;
  for (const auto &entry : state.effect_contributions.rows()) {
    auto actual = read_battle_injury_effect_source_v1(s, entry.first);
    if (!actual.ok)
      return {false, {}, actual.error};
  }
  for (const auto &entry : state.episodes.rows()) {
    const auto &episode = entry.second;
    const auto *ref = std::get_if<ConditionReceiptRefV2>(&episode.origin);
    const auto *origin =
        ref ? state.applied_receipts.find(ref->result_id) : nullptr;
    if (entry.first != episode.episode_id || !origin ||
        origin->actor_id != episode.actor_id ||
        origin->selected_injury_result.kind !=
            SelectedInjuryResult::Kind::applied ||
        !std::holds_alternative<CharacterInjurySourceRefV1>(origin->cause))
      return common_failure("BattleInjuryEpisodeActualOriginRequired",
                            entry.first);
    auto actual = validate_battle_injury_selected_result_v1(s, *origin);
    if (!actual.ok)
      return actual;
    for (const auto &effect : origin->selected_injury_result.effect_items) {
      std::size_t found = 0;
      for (const auto &item : state.effect_contributions.rows()) {
        const auto &id = item.second.identity;
        const auto *source =
            std::get_if<ConditionReceiptRefV2>(&id.origin_result);
        if (source && source->result_id == origin->result_id &&
            id.actor_id == episode.actor_id &&
            id.episode_id == episode.episode_id &&
            id.effect_key == effect.effect_key)
          ++found;
      }
      if (found != 1)
        return common_failure("BattleInjuryEpisodeEffectCoverageMismatch",
                              entry.first);
    }
    for (const auto &previous : state.applied_receipts.rows()) {
      const auto &row = previous.second;
      if (row.actor_id == episode.actor_id &&
          row.condition_revision_after < origin->condition_revision_after &&
          row.selected_injury_result.kind ==
              SelectedInjuryResult::Kind::applied &&
          actor_foundation_rule_equal(
              row.selected_injury_result.injury_rule_ref,
              episode.condition_rule_ref))
        return common_failure("BattleInjuryOriginalEpisodeNotReused",
                              entry.first);
    }
  }
  for (const auto &entry : state.applied_receipts.rows()) {
    const auto &receipt = entry.second;
    if (receipt.selected_injury_result.kind !=
        SelectedInjuryResult::Kind::applied)
      continue;
    if (!std::holds_alternative<CharacterInjurySourceRefV1>(receipt.cause))
      return common_failure("BattleInjuryForeignEffectAdapterRequired",
                            entry.first, ErrorKind::unsupported_capability);
    auto actual = validate_battle_injury_selected_result_v1(s, receipt);
    if (!actual.ok)
      return actual;
    const ConditionEpisodeV2 *episode = nullptr;
    for (const auto &row : state.episodes.rows())
      if (row.second.actor_id == receipt.actor_id &&
          actor_foundation_rule_equal(
              row.second.condition_rule_ref,
              receipt.selected_injury_result.injury_rule_ref)) {
        if (episode)
          return common_failure("DuplicateBattleInjuryRuleEpisode",
                                entry.first);
        episode = &row.second;
      }
    if (!episode)
      return common_failure("BattleInjuryEpisodeCoverageRequired", entry.first);
    const auto *ref = std::get_if<ConditionReceiptRefV2>(&episode->origin);
    const auto *origin =
        ref ? state.applied_receipts.find(ref->result_id) : nullptr;
    if (!origin)
      return common_failure("BattleInjuryOriginalEpisodeRequired", entry.first);
    if (injury_grade_effects_v3_selected(s.catalog)) {
      const auto &effects = receipt.selected_injury_result.effect_items;
      if (receipt.created_effect_keys.size() != effects.size())
        return common_failure("OriginalInjuryScarEffectCoverageRequired",
                              entry.first);
      for (std::size_t i = 0; i < effects.size(); ++i) {
        if (receipt.created_effect_keys[i] != effects[i].effect_key)
          return common_failure("OriginalInjuryScarEffectKeyMismatch",
                                entry.first);
        std::size_t matches = 0;
        for (const auto &item : state.effect_contributions.rows()) {
          const auto &id = item.second.identity;
          const auto *source =
              std::get_if<ConditionReceiptRefV2>(&id.origin_result);
          if (source && source->result_id == receipt.result_id &&
              id.actor_id == receipt.actor_id &&
              id.episode_id == episode->episode_id &&
              id.effect_key == effects[i].effect_key)
            ++matches;
        }
        if (matches != 1)
          return common_failure(
              "OriginalInjuryScarContributionCoverageRequired", entry.first);
      }
    } else if (origin != &receipt) {
      // No authored grade-to-stage formula exists in this finite map. Retain
      // the actual original stage/effects; do not stack another permanent copy.
      if (!receipt.created_effect_keys.empty() ||
          receipt.selected_injury_result.effect_items.size() !=
              origin->selected_injury_result.effect_items.size())
        return common_failure("BattleInjuryRepeatedEpisodeEffectsMismatch",
                              entry.first);
      for (std::size_t i = 0;
           i < receipt.selected_injury_result.effect_items.size(); ++i)
        if (!same_original_injury_effect(
                receipt.selected_injury_result.effect_items[i],
                origin->selected_injury_result.effect_items[i]))
          return common_failure("BattleInjuryStageEffectRuleRequired",
                                entry.first, ErrorKind::unsupported_capability);
    }
  }
  return common_success();
}
} // namespace hanmo::resident
