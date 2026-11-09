#include "life_condition.hpp"
#include "authored_origin.hpp"
#include "battle_injury_catalog_v1.hpp"
#include "battle_injury_episode_v1.hpp"
#include "character_condition_read.hpp"
#include "condition_application_fact.hpp"
#include "condition_capacity_contract.hpp"
#include "military_condition.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
Integer tag(const LifeCauseRef4 &cause) {
  return cause.index() == 0 ? 4 : cause.index() == 1 ? 12 : 13;
}
const std::optional<ActorFoundationRuleRef> &
selected(const ActorFoundationCatalog &c, const LifeCauseRef4 &source) {
  return source.index() == 0   ? c.military_death_rule
         : source.index() == 1 ? c.natural_age_death_rule
                               : c.sea_death_rule;
}
bool same_time(const WorldMoment &a, const WorldMoment &b) {
  auto x = compare_world_moments(a, b);
  return x.ok && x.value == 0;
}
Status selected_life4(const ActorFoundationCatalog &catalog) {
  const StableId *selected = nullptr;
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "family_life") {
      if (selected)
        return common_failure("DuplicateFamilyLifeModule", {},
                              ErrorKind::internal);
      selected = &module.second;
    }
  return selected && *selected == "4"
             ? common_success()
             : common_failure("FamilyLife4SelectionRequired", {},
                              ErrorKind::unsupported_capability);
}
} // namespace
Status validate_life_termination_condition(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const FamilyDeathState &deaths, const ActorFoundationCatalog &catalog,
    const FamilyLifeCauseSources4 &sources, const ConditionReceiptV2 &r,
    const WorldMoment &current, MilitaryCasualtyHistoryOrder ordering) {
  auto profile = selected_life4(catalog);
  if (!profile.ok)
    return profile;
  std::optional<LifeTerminationConditionCause> military;
  const auto *cause = std::get_if<LifeTerminationConditionCause>(&r.cause);
  if (const auto *m = std::get_if<MilitaryCasualtyConditionCause>(&r.cause);
      m &&
      m->disposition == MilitaryCasualtyConditionCause::Disposition::dead) {
    military = LifeTerminationConditionCause{
        MilitaryDeathCause4{m->casualty_fact_id, m->battle_id, m->actor_id,
                            m->occurrence_at},
        m->source_schema, m->source_ordinal};
    cause = &*military;
  }
  const auto *key =
      std::get_if<LifeTerminationConditionBusinessKey>(&r.business_key);
  if (!cause || !key || !r.life_transition_ref)
    return common_failure("LifeConditionCauseRequired", r.result_id);
  const auto &rule = selected(catalog, cause->source);
  if (!rule || rule->catalog_digest != common.state().metadata.content_digest ||
      rule->version < 1 || !actor_foundation_rule_equal(r.rule_ref, *rule))
    return common_failure("LifeConditionRuleMismatch", r.result_id);
  auto source =
      read_life_cause4(common, sources, cause->source, &current, ordering);
  if (!source.ok)
    return {false, {}, source.error};
  auto death = read_verified_death4(
      common, deaths, sources,
      {"family_life", "family_death", *r.life_transition_ref}, current,
      ordering);
  if (!death.ok)
    return {false, {}, death.error};
  const auto *stored = deaths.deaths.find(*r.life_transition_ref);
  if (!stored || !same_life_cause4(stored->precise->cause, cause->source) ||
      key->cause_tag != tag(cause->source) ||
      key->source_id != source.value.cause_ref.id ||
      key->actor_id != r.actor_id ||
      key->source_ordinal != source.value.source_ordinal ||
      cause->source_schema != source.value.source_schema ||
      cause->source_ordinal != source.value.source_ordinal ||
      r.actor_id != source.value.actor_id ||
      r.actor_id != death.value.actor_id ||
      r.effective_at.kind != ActorFoundationAt::Kind::exact_moment ||
      !same_time(r.effective_at.exact_moment, source.value.occurred_at) ||
      r.condition_revision_before < 0 ||
      r.condition_revision_before == std::numeric_limits<Integer>::max() ||
      r.condition_revision_after != r.condition_revision_before + 1 ||
      r.scalar_changes.size() != 2 ||
      r.selected_injury_result.kind !=
          SelectedInjuryResult::Kind::not_an_injury ||
      !r.created_effect_keys.empty() || !r.recovery_resolution_refs.empty() ||
      !original_condition_capacity_refs_shape(catalog, r) ||
      !state.character_origin_records.origins.find(r.actor_id))
    return common_failure("LifeConditionReceiptMismatch", r.result_id);
  const auto &hp = r.scalar_changes[0];
  const auto &alive = r.scalar_changes[1];
  if (hp.field != ConditionScalarChange::Field::hp || hp.before < 0 ||
      hp.before > 100 || hp.after != 0 ||
      alive.field != ConditionScalarChange::Field::alive || alive.before != 1 ||
      alive.after != 0)
    return common_failure("LifeConditionScalarMismatch", r.result_id);
  return common_success();
}
Outcome<LifeConditionApplication> apply_verified_life_termination(
    CommonStore &common, AuthoredActorFoundationState &state,
    FamilyDeathState &deaths, const ActorFoundationCatalog &catalog,
    const FamilyLifeCauseSources4 &sources, const LifeCauseRef4 &cause,
    const ReadStamp &stamp, const ReadStampAuthority &stamps,
    const DomainReadVersionSource &versions,
    const CharacterConditionSources *episode_sources) {
  auto profile = selected_life4(catalog);
  if (!profile.ok)
    return {false, {}, profile.error};
  auto source = read_life_cause4(common, sources, cause, &stamp.span().to);
  if (!source.ok)
    return {false, {}, source.error};
  const auto &actual = source.value;
  auto valid = stamps.validate(stamp, common.state().identity,
                               ReadMode::phase_current, common, versions);
  if (!valid.ok)
    return {false, {}, valid.error};
  bool actor_read = false;
  for (const auto &r : stamp.resources())
    if (r.resource.kind == ResourceKey::Kind::actor &&
        r.resource.id == actual.actor_id)
      actor_read = true;
  if (!actor_read ||
      std::find(stamp.source_ids().begin(), stamp.source_ids().end(),
                actual.cause_ref.id) == stamp.source_ids().end() ||
      !same_time(stamp.span().to, actual.occurred_at))
    return {false,
            {},
            common_failure("LifeConditionPhaseSourceMismatch", actual.actor_id)
                .error};
  const auto &rule = selected(catalog, cause);
  if (!rule || rule->id.empty() || rule->version < 1 ||
      rule->catalog_digest != common.state().metadata.content_digest)
    return {false,
            {},
            common_failure("LifeConditionRuleRequired", actual.actor_id,
                           ErrorKind::unsupported_capability)
                .error};
  const bool has_episodes =
      battle_injury_source1_selected(catalog.identity) &&
      !state.character_condition_extensions.episodes.empty();
  if (has_episodes) {
    if (!episode_sources || &episode_sources->common != &common ||
        &episode_sources->state != &state ||
        &episode_sources->catalog != &catalog ||
        episode_sources->deaths != &deaths ||
        episode_sources->life_sources != &sources ||
        !same_time(episode_sources->actual_now, stamp.span().to))
      return {false,
              {},
              common_failure("LifeEpisodeSameCandidateSourcesRequired").error};
    valid = validate_battle_injury_episode_sources_v1(*episode_sources);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  if (const auto *prior = family_death_for_actor(deaths, actual.actor_id)) {
    if (!prior->precise || !same_life_cause4(prior->precise->cause, cause))
      return {false,
              {},
              common_failure("ConflictingDeathSource", actual.actor_id).error};
    const ConditionReceiptV2 *receipt = nullptr;
    for (const auto &row :
         state.character_condition_extensions.applied_receipts.rows())
      if (row.second.life_transition_ref ==
          std::optional<StableId>{prior->precise->death_id}) {
        if (receipt)
          return {
              false,
              {},
              common_failure("DuplicateLifeConditionReceipt", actual.actor_id)
                  .error};
        receipt = &row.second;
      }
    if (!receipt)
      return {
          false,
          {},
          common_failure("MissingLifeConditionReceipt", actual.actor_id).error};
    valid = validate_life_termination_condition(
        common, state, deaths, catalog, sources, *receipt, stamp.span().to);
    if (!valid.ok)
      return {false, {}, valid.error};
    valid = validate_condition_application_fact_for_receipt(common, state,
                                                            catalog, *receipt);
    if (!valid.ok)
      return {false, {}, valid.error};
    return {true,
            {{"family_life", "family_death", prior->precise->death_id},
             {receipt->result_id},
             {},
             true},
            {}};
  }
  const auto *actor = common.actor(actual.actor_id);
  auto *head = state.character_condition_extensions.actor_condition_heads.find(
      actual.actor_id);
  if (!actor || !actor->alive || actor->hp < 0 || actor->hp > 100 || !head ||
      head->condition_revision < 0 ||
      head->condition_revision == std::numeric_limits<Integer>::max() ||
      !state.character_origin_records.origins.find(actual.actor_id))
    return {false,
            {},
            common_failure("LifeConditionActorSourceRequired", actual.actor_id)
                .error};
  const auto capture = common.actor_condition_capture_identity();
  if (!capture)
    return {false, {}, common_failure("LifeConditionCaptureRequired").error};
  auto death_id = common.next_id("family_death");
  if (!death_id.ok)
    return {false, {}, death_id.error};
  auto receipt_id = common.next_id("condition");
  if (!receipt_id.ok)
    return {false, {}, receipt_id.error};


  auto terminal_memory = CheckpointMemoryCharge(0);
  ConditionReceiptV2 r;
  r.result_id = receipt_id.value;
  r.actor_id = actual.actor_id;
  r.business_key = LifeTerminationConditionBusinessKey{
      tag(cause), actual.cause_ref.id, actual.actor_id, actual.source_ordinal};
  if (const auto *m = std::get_if<MilitaryDeathCause4>(&cause))
    r.cause = military_death_condition_cause(*m, actual.source_schema,
                                             actual.source_ordinal);
  else
    r.cause = LifeTerminationConditionCause{cause, actual.source_schema,
                                            actual.source_ordinal};
  r.rule_ref = *rule;
  r.effective_at.exact_moment = actual.occurred_at;
  r.condition_revision_before = head->condition_revision;
  r.condition_revision_after = head->condition_revision + 1;
  r.scalar_changes = {{ConditionScalarChange::Field::hp, actor->hp, 0},
                      {ConditionScalarChange::Field::alive, 1, 0}};
  r.life_transition_ref = death_id.value;
  FamilyDeathRecord death;
  death.actor_id = actual.actor_id;
  death.world_id = common.state().identity.world_id;
  death.scenario_id = common.state().metadata.scenario_id;
  death.precise = FamilyDeathPayload4{death_id.value, actual.occurred_at,
                                      stamp.span().to, cause};
  const auto before = common.actor_condition_execution_drafts().size();
  ActorMutation mutation;
  mutation.actor_id = actual.actor_id;
  mutation.hp = 0;
  mutation.alive = false;
  mutation.available = false;
  valid = common.apply_actor(mutation);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (!deaths.deaths.insert(death_id.value, std::move(death)))
    return {false,
            {},
            common_failure("DuplicateFamilyDeathFact", death_id.value).error};
  valid = common.append_fact({death_id.value, "family_death", "family_life"});
  if (!valid.ok)
    return {false, {}, valid.error};
  if (!state.character_condition_extensions.applied_receipts.insert(
          receipt_id.value, std::move(r)))
    return {
        false,
        {},
        common_failure("DuplicateConditionReceipt", receipt_id.value).error};
  head->condition_revision += 1;
  head->latest_receipt_ref = ConditionReceiptRefV2{receipt_id.value};
  if (common.actor_condition_execution_drafts().size() != before + 1)
    return {false,
            {},
            common_failure("LifeConditionActualWriteCountMismatch").error};
  BoundActorConditionWrite actual_write{
      {receipt_id.value}, capture, Integer(before + 1)};
  valid = append_condition_application_fact_in_candidate(common, state, catalog,
                                                         actual_write);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_life_termination_condition(
      common, state, deaths, catalog, sources,
      *state.character_condition_extensions.applied_receipts.find(
          receipt_id.value),
      stamp.span().to);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (has_episodes) {
    // Death ends the acute episode, not its pinned permanent contribution.
    // This is the original Life writer's same candidate, never a second body
    // write.
    for (const auto &row :
         state.character_condition_extensions.episodes.rows()) {
      if (row.second.actor_id != actual.actor_id)
        continue;
      auto *episode =
          state.character_condition_extensions.episodes.find(row.first);
      if (episode->terminal_source_ref)
        return {false,
                {},
                common_failure("LifeEpisodeAlreadyTerminal", row.first).error};
      episode->terminal_source_ref =
          DeathRef{"family_life", "family_death", death_id.value};
    }
    valid = validate_battle_injury_episode_sources_v1(*episode_sources);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  std::vector<BoundActorConditionWrite> writes;
  writes.push_back(std::move(actual_write));
  return {true,
          {{"family_life", "family_death", death_id.value},
           {receipt_id.value},
           std::move(writes),
           false},
          {}};
}
Status validate_life_termination_coverage(
    const CommonStore &common, const AuthoredActorFoundationState &state,
    const FamilyDeathState &deaths, const ActorFoundationCatalog &catalog,
    const FamilyLifeCauseSources4 &sources, const WorldMoment &current,
    MilitaryCasualtyHistoryOrder ordering) {
  auto valid =
      validate_family_life4(common, deaths, sources, current, ordering);
  if (!valid.ok)
    return valid;
  for (const auto &row :
       state.character_condition_extensions.applied_receipts.rows())
    if (std::holds_alternative<LifeTerminationConditionCause>(
            row.second.cause) ||
        (std::holds_alternative<MilitaryCasualtyConditionCause>(
             row.second.cause) &&
         std::get<MilitaryCasualtyConditionCause>(row.second.cause)
                 .disposition ==
             MilitaryCasualtyConditionCause::Disposition::dead)) {
      valid = validate_life_termination_condition(common, state, deaths,
                                                  catalog, sources, row.second,
                                                  current, ordering);
      if (!valid.ok)
        return valid;
    }
  for (const auto &row : deaths.deaths.rows()) {
    std::size_t count = 0;
    for (const auto &r :
         state.character_condition_extensions.applied_receipts.rows())
      if (r.second.life_transition_ref == std::optional<StableId>{row.first})
        ++count;
    if (count != 1)
      return common_failure("LifeConditionDeathCoverageMismatch", row.first);
  }
  return common_success();
}
} // namespace hanmo::resident
